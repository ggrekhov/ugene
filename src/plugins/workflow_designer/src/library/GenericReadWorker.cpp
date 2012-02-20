/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2012 UniPro <ugene@unipro.ru>
 * http://ugene.unipro.ru
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "GenericReadWorker.h"
#include "GenericReadActor.h"
#include "CoreLib.h"

#include <U2Core/AppContext.h>
#include <U2Core/AppResources.h>
#include <U2Core/DocumentModel.h>
#include <U2Core/IOAdapter.h>
#include <U2Core/IOAdapterUtils.h>
#include <U2Core/Log.h>
#include <U2Core/ProjectModel.h>
#include <U2Core/DocumentUtils.h>
#include <U2Core/GObjectTypes.h>
#include <U2Core/GObjectUtils.h>
#include <U2Core/GObjectRelationRoles.h>
#include <U2Core/DNASequenceObject.h>
#include <U2Core/MAlignmentObject.h>
#include <U2Core/AnnotationTableObject.h>
#include <U2Core/MSAUtils.h>
#include <U2Core/U2SafePoints.h>
#include <U2Core/U2DbiRegistry.h>
#include <U2Core/U2OpStatusUtils.h>
#include <U2Core/U2SequenceUtils.h>
#include <U2Lang/BaseSlots.h>
#include <U2Lang/BaseAttributes.h>
#include <U2Lang/WorkflowEnv.h>
#include <U2Lang/CoreLibConstants.h>
#include <U2Formats/DocumentFormatUtils.h>

#include <memory>

namespace U2 {
using namespace Workflow;
namespace LocalWorkflow {

/**************************
 * GenericMSAReader
 **************************/
void GenericMSAReader::init() {
    mtype = WorkflowEnv::getDataTypeRegistry()->getById(GenericMAActorProto::TYPE);
    urls = WorkflowUtils::expandToUrls(actor->getParameter(BaseAttributes::URL_IN_ATTRIBUTE().getId())->getAttributeValue<QString>(context));
    assert(ports.size() == 1);
    ch = ports.values().first();
}

bool GenericMSAReader::isReady() {
    return !isDone();
}

Task* GenericMSAReader::tick() {
    if (cache.isEmpty() && !urls.isEmpty()) {
        Task* t = createReadTask(urls.takeFirst());
        connect(t, SIGNAL(si_stateChanged()), SLOT(sl_taskFinished()));
        return t;
    }
    while (!cache.isEmpty()) {
        ch->put(cache.takeFirst());
    }
    if (urls.isEmpty()) {
        done = true;
        ch->setEnded();
    }
    return NULL;
}

bool GenericMSAReader::isDone() {
    return done && cache.isEmpty();
}

void GenericMSAReader::sl_taskFinished() {
    LoadMSATask* t = qobject_cast<LoadMSATask*>(sender());
    if (!t->isFinished() || t->hasError()) {
        return;
    }
    foreach(MAlignment ma, t->results) {
        QVariantMap m;
        m.insert(BaseSlots::URL_SLOT().getId(), t->url);
        m.insert(BaseSlots::MULTIPLE_ALIGNMENT_SLOT().getId(), qVariantFromValue<MAlignment>(ma)); 
        cache.append(Message(mtype, m));
    }
}

/**************************
 * LoadMSATask
 **************************/
void LoadMSATask::prepare() {
    int memUseMB = 0;
    QFileInfo file(url);
    memUseMB = file.size() / (1024*1024);
    IOAdapterFactory* iof = AppContext::getIOAdapterRegistry()->getIOAdapterFactoryById(IOAdapterUtils::url2io(url));
    if (iof->getAdapterId() == BaseIOAdapters::GZIPPED_LOCAL_FILE || iof->getAdapterId() == BaseIOAdapters::GZIPPED_HTTP_FILE) {
        memUseMB *= 2.5; //Need to calculate compress level
    }
    coreLog.trace(QString("load document:Memory resource %1").arg(memUseMB));

    if (memUseMB > 0) {
        addTaskResource(TaskResourceUsage(RESOURCE_MEMORY, memUseMB, false));
    }
}

void LoadMSATask::run() {
    QFileInfo fi(url);
    if(!fi.exists()){
        stateInfo.setError(  tr("File '%1' not exists").arg(url) );
        return;
    }
    DocumentFormat* format = NULL;
    QList<DocumentFormat*> fs = DocumentUtils::toFormats(DocumentUtils::detectFormat(url));
    foreach(DocumentFormat* f, fs) {
        if (f->getSupportedObjectTypes().contains(GObjectTypes::MULTIPLE_ALIGNMENT)) {
            format = f;
            break;
        }
    }
    if (format == NULL) {
        foreach(DocumentFormat* f, fs) {
            if (f->getSupportedObjectTypes().contains(GObjectTypes::SEQUENCE)) {
                format = f;
                break;
            }
        }
    }
    if (format == NULL) {
        stateInfo.setError(  tr("Unsupported document format") );
        return;
    }
    ioLog.info(tr("Reading MSA from %1 [%2]").arg(url).arg(format->getFormatName()));
    IOAdapterFactory* iof = AppContext::getIOAdapterRegistry()->getIOAdapterFactoryById(IOAdapterUtils::url2io(url));
    std::auto_ptr<Document> doc(format->loadDocument(iof, url, QVariantMap(), stateInfo));
    CHECK_OP(stateInfo, );
    if (!doc->findGObjectByType(GObjectTypes::MULTIPLE_ALIGNMENT).isEmpty()) {
        foreach(GObject* go, doc->findGObjectByType(GObjectTypes::MULTIPLE_ALIGNMENT)) {
            results.append(((MAlignmentObject*)go)->getMAlignment());
        }
    } else {
        MAlignment ma = MSAUtils::seq2ma(doc->findGObjectByType(GObjectTypes::SEQUENCE), stateInfo);
        if (!hasError()) {
            results.append(ma);
        } 
    }

}

/**************************
 * GenericSeqReader
 **************************/
void GenericSeqReader::init() {
    GenericMSAReader::init();
    mtype = WorkflowEnv::getDataTypeRegistry()->getById(GenericSeqActorProto::TYPE);
    GenericSeqActorProto::Mode mode = GenericSeqActorProto::Mode(actor->getParameter(
                                                                    GenericSeqActorProto::MODE_ATTR)->getAttributeValue<int>(context));
    if (GenericSeqActorProto::MERGE == mode) {
        QString mergeToken = DocumentReadingMode_SequenceMergeGapSize;
        cfg[mergeToken] = actor->getParameter(GenericSeqActorProto::GAP_ATTR)->getAttributeValue<int>(context);
    }
    selector.acc = actor->getParameter(GenericSeqActorProto::ACC_ATTR)->getAttributeValue<QString>(context);
}

void GenericSeqReader::sl_taskFinished() {
    LoadSeqTask* t = qobject_cast<LoadSeqTask*>(sender());
    if (!t->isFinished() || t->hasError()) {
        return;
    }
    foreach(const QVariantMap& m, t->results) {
        cache.append(Message(mtype, m));
    }
}

/**************************
 * LoadSeqTask
 **************************/
void LoadSeqTask::prepare() {
    int memUseMB = 0;
    QFileInfo file(url);
    memUseMB = file.size() / (1024*1024);
    IOAdapterFactory* iof = AppContext::getIOAdapterRegistry()->getIOAdapterFactoryById(IOAdapterUtils::url2io(url));
    if (iof->getAdapterId() == BaseIOAdapters::GZIPPED_LOCAL_FILE || iof->getAdapterId() == BaseIOAdapters::GZIPPED_HTTP_FILE) {
        memUseMB *= 2.5; //Need to calculate compress level
    }
    coreLog.trace(QString("load document:Memory resource %1").arg(memUseMB));

    if (memUseMB > 0) {
        addTaskResource(TaskResourceUsage(RESOURCE_MEMORY, memUseMB, false));
    }
}

void LoadSeqTask::run() {
    QFileInfo fi(url);
    if(!fi.exists()){
        stateInfo.setError(  tr("File '%1' not exists").arg(url) );
        return;
    }
    QList<DocumentFormat*> fs = DocumentUtils::toFormats(DocumentUtils::detectFormat(url));
    DocumentFormat* format = NULL;

    foreach( DocumentFormat * f, fs ) {
        const QSet<GObjectType>& types = f->getSupportedObjectTypes();
        if (types.contains(GObjectTypes::SEQUENCE) || types.contains(GObjectTypes::MULTIPLE_ALIGNMENT)) {
            format = f;
            break;
        }
    }

    if (format == NULL) {
        stateInfo.setError(  tr("Unsupported document format") );
        return;
    }
    ioLog.info(tr("Reading sequences from %1 [%2]").arg(url).arg(format->getFormatName()));
    IOAdapterFactory* iof = AppContext::getIOAdapterRegistry()->getIOAdapterFactoryById(IOAdapterUtils::url2io(url));
    cfg.insert(DocumentFormat::DBI_ALIAS_HINT, QString(WORKFLOW_SESSION_TMP_DBI_ALIAS));
    std::auto_ptr<Document> doc(format->loadDocument(iof, url, cfg, stateInfo));
    doc->setDocumentOwnsDbiResources(false);
    CHECK_OP(stateInfo, );

    const QSet<GObjectType>& types = format->getSupportedObjectTypes();
    if (types.contains(GObjectTypes::SEQUENCE)) {
        QList<GObject*> seqObjs = doc->findGObjectByType(GObjectTypes::SEQUENCE);
        QList<GObject*> annObjs = doc->findGObjectByType(GObjectTypes::ANNOTATION_TABLE);
        foreach(GObject* go, seqObjs) {
            assert(go != NULL);
            if (!selector->objectMatches((U2SequenceObject*)go)) {
                continue;
            }
            QVariantMap m;
            m.insert(BaseSlots::URL_SLOT().getId(), url);
            m.insert(BaseSlots::DNA_SEQUENCE_SLOT().getId(), go->getEntityRef().entityId);
            QList<GObject*> allLoadedAnnotations = doc->findGObjectByType(GObjectTypes::ANNOTATION_TABLE);
            QList<GObject*> annotations = GObjectUtils::findObjectsRelatedToObjectByRole(go, 
                GObjectTypes::ANNOTATION_TABLE, GObjectRelationRole::SEQUENCE, 
                allLoadedAnnotations, UOF_LoadedOnly);
            if (!annotations.isEmpty()) {
                QList<SharedAnnotationData> l;
                foreach(GObject * annGObj, annotations) {
                    AnnotationTableObject* att = qobject_cast<AnnotationTableObject*>(annGObj);
                    foreach(Annotation* a, att->getAnnotations()) {
                        l << a->data();
                    }
                    annObjs.removeAll(annGObj);
                }
                m.insert(BaseSlots::ANNOTATION_TABLE_SLOT().getId(), qVariantFromValue<QList<SharedAnnotationData> >(l));
            }
            results.append(m);
        }

        // if there are annotations that are not connected to a sequence -> put them  independently
        foreach(GObject * annObj, annObjs) {
            AnnotationTableObject* att = qobject_cast<AnnotationTableObject*>(annObj);
            if(att->findRelatedObjectsByRole(GObjectRelationRole::SEQUENCE).isEmpty()) {
                assert(att != NULL);
                QVariantMap m;
                m.insert(BaseSlots::URL_SLOT().getId(), url);

                QList<SharedAnnotationData> l;
                foreach(Annotation* a, att->getAnnotations()) {
                    l << a->data();
                }
                m.insert(BaseSlots::ANNOTATION_TABLE_SLOT().getId(), qVariantFromValue<QList<SharedAnnotationData> >(l));
                results.append(m);
            }
        }
    } else {
        //TODO merge seqs from alignment
        //             QString mergeToken = MERGE_MULTI_DOC_GAP_SIZE_SETTINGS;
        //             bool merge = cfg.contains(mergeToken);
        //             int gaps = cfg.value(mergeToken).toInt();
        U2OpStatus2Log os;
        foreach(GObject* go, doc->findGObjectByType(GObjectTypes::MULTIPLE_ALIGNMENT)) {
            foreach(const DNASequence& s, MSAUtils::ma2seq(((MAlignmentObject*)go)->getMAlignment(), false)) {
                if (!selector->matches(s)) {
                    continue;
                }
                QVariantMap m;
                U2EntityRef seqRef = U2SequenceUtils::import(doc->getDbiRef(), s, os);
                CHECK_OP(os, );
                m.insert(BaseSlots::URL_SLOT().getId(), url);
                m.insert(BaseSlots::DNA_SEQUENCE_SLOT().getId(), qVariantFromValue<U2DataId>(seqRef.entityId));
                results.append(m);
            }
        }
    }
}

/**************************
 * DNASelector
 **************************/
bool DNASelector::matches( const DNASequence& dna) {
    if (acc.isEmpty()) {
        return true;
    }
    if (dna.info.contains(DNAInfo::ACCESSION)) {
        return dna.info.value(DNAInfo::ACCESSION).toStringList().contains(acc);
    }
    return acc == dna.getName();
}

bool DNASelector::objectMatches( const U2SequenceObject *dna) {
    if (acc.isEmpty()) {
        return true;
    }
    QVariantMap info = dna->getSequenceInfo();
    if (info.contains(DNAInfo::ACCESSION)) {
        return info.value(DNAInfo::ACCESSION).toStringList().contains(acc);
    }
    return acc == dna->getSequenceName();
}

} // Workflow namespace
} // U2 namespace
