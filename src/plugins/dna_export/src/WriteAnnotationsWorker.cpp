/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2011 UniPro <ugene@unipro.ru>
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

#include <U2Core/DocumentModel.h>
#include <U2Core/AppContext.h>
#include <U2Core/BaseDocumentFormats.h>
#include <U2Core/SaveDocumentTask.h>
#include <U2Core/QVariantUtils.h>
#include <U2Core/FailTask.h>
#include <U2Core/IOAdapter.h>
#include <U2Core/DocumentUtils.h>
#include <U2Core/GUrlUtils.h>
#include <U2Lang/BaseTypes.h>
#include <U2Lang/BaseSlots.h>
#include <U2Lang/BasePorts.h>
#include <U2Lang/BaseAttributes.h>
#include <U2Lang/BaseActorCategories.h>
#include <U2Lang/CoreLibConstants.h>
#include <U2Lang/WorkflowEnv.h>
#include <U2Lang/ActorPrototypeRegistry.h>
#include <U2Designer/DelegateEditors.h>
#include <U2Misc/DialogUtils.h>

#include "ExportAnnotations2CSVTask.h"
#include "WriteAnnotationsWorker.h"

namespace U2 {
namespace LocalWorkflow {

const QString WriteAnnotationsWorkerFactory::ACTOR_ID("write-annotations");

static const QString WRITE_ANNOTATIONS_IN_TYPE_ID("write-annotations-in-type");
static const QString CSV_FORMAT_ID("csv");
static const QString ANNOTATIONS_NAME("annotations-name");
static const QString ANNOTATIONS_NAME_DEF_VAL("unknown features");

/*******************************
 * WriteAnnotationsWorker
 *******************************/
WriteAnnotationsWorker::~WriteAnnotationsWorker() {
    qDeleteAll(createdAnnotationObjects);
}

void WriteAnnotationsWorker::init() {
    annotationsPort = ports.value(BasePorts::IN_ANNOTATIONS_PORT_ID());
}

bool WriteAnnotationsWorker::isReady() {
    return annotationsPort->hasMessage();
}

Task * WriteAnnotationsWorker::tick() {
    Message inputMessage = getMessageAndSetupScriptValues(annotationsPort);
    QString formatId = actor->getParameter(BaseAttributes::DOCUMENT_FORMAT_ATTRIBUTE().getId())->getAttributeValue<QString>();
    DocumentFormat * format = AppContext::getDocumentFormatRegistry()->getFormatById(formatId);
    if( formatId != CSV_FORMAT_ID && format == NULL ) {
        return new FailTask(tr("Unrecognized formatId: '%1'").arg(formatId));
    }
    
    QString filepath = actor->getParameter(BaseAttributes::URL_OUT_ATTRIBUTE().getId())->getAttributeValue<QString>();
    filepath = filepath.isEmpty() ? inputMessage.getData().toMap().value(BaseSlots::URL_SLOT().getId()).value<QString>() : filepath;
    if (filepath.isEmpty()) {
        return new FailTask(tr("Unspecified URL to write %1").arg(formatId));
    }
    QStringList exts = formatId == CSV_FORMAT_ID ? QStringList("csv") : format->getSupportedDocumentFileExtensions();
    filepath = GUrlUtils::ensureFileExt(filepath, exts).getURLString();
    
    SaveDocFlags fl(actor->getParameter(BaseAttributes::FILE_MODE_ATTRIBUTE().getId())->getAttributeValue<uint>());
    
    QString objName = actor->getParameter(ANNOTATIONS_NAME)->getAttributeValue<QString>();
    if(objName.isEmpty()) {
        objName = ANNOTATIONS_NAME_DEF_VAL;
        coreLog.details(tr("Annotations name not specified. Default value used: '%1'").arg(objName));
    }
    AnnotationTableObject * att = new AnnotationTableObject(objName);
    QList<SharedAnnotationData> atl = QVariantUtils::var2ftl(inputMessage.getData().toMap().
        value(BaseSlots::ANNOTATION_TABLE_SLOT().getId()).toList());
    foreach(const SharedAnnotationData & ad, atl) {
        att->addAnnotation(new Annotation(ad), QString());
    }
    
    QSet<QString> excludeFileNames = DocumentUtils::getNewDocFileNameExcludesHint();
    if(formatId == CSV_FORMAT_ID) {
        createdAnnotationObjects << att; // will delete in destructor
        TaskStateInfo ti;
        if(fl.testFlag(SaveDoc_Roll) && !GUrlUtils::renameFileWithNameRoll(filepath, ti, excludeFileNames, &coreLog)) {
            return new FailTask(ti.getError());
        }
        return new ExportAnnotations2CSVTask(att->getAnnotations(), QByteArray(), NULL, false, filepath, fl.testFlag(SaveDoc_Append));
    } else {
        fl |= SaveDoc_DestroyAfter;
        IOAdapterFactory* iof = AppContext::getIOAdapterRegistry()->getIOAdapterFactoryById(BaseIOAdapters::url2io(filepath));
        Document * doc = new Document(AppContext::getDocumentFormatRegistry()->getFormatById(formatId), iof, filepath);
        doc->setLoaded(true);
        att->setModified(false);
        doc->addObject(att); // savedoc task will delete doc -> doc will delete att
        return new SaveDocumentTask(doc, fl, excludeFileNames);
    }
}

bool WriteAnnotationsWorker::isDone() {
    return annotationsPort->isEnded();
}

void WriteAnnotationsWorker::cleanup() {
}

/*******************************
 * WriteAnnotationsWorkerFactory
 *******************************/
void WriteAnnotationsWorkerFactory::init() {
    // ports description
    QList<PortDescriptor*> portDescs;
    {
        QMap<Descriptor, DataTypePtr> inM;
        inM[BaseSlots::ANNOTATION_TABLE_SLOT()] = BaseTypes::ANNOTATION_TABLE_LIST_TYPE();
        inM[BaseSlots::URL_SLOT()] = BaseTypes::STRING_TYPE();
        DataTypePtr inSet(new MapDataType(WRITE_ANNOTATIONS_IN_TYPE_ID, inM));
        Descriptor inPortDesc(BasePorts::IN_ANNOTATIONS_PORT_ID(), WriteAnnotationsWorker::tr("Input annotations"), 
            WriteAnnotationsWorker::tr("Input annotations which will be written to output file"));
        portDescs << new PortDescriptor(inPortDesc, inSet, true);
    }
    QList<DocumentFormatId> supportedFormats;
    // attributes description
    QList<Attribute*> attrs;
    {
        DocumentFormatConstraints constr;
        constr.supportedObjectTypes.insert( GObjectTypes::ANNOTATION_TABLE );
        constr.addFlagToSupport(DocumentFormatFlag_SupportWriting);
        supportedFormats = AppContext::getDocumentFormatRegistry()->selectFormats( constr );
        supportedFormats.append(CSV_FORMAT_ID);
        
        attrs << new Attribute(BaseAttributes::DOCUMENT_FORMAT_ATTRIBUTE(), BaseTypes::STRING_TYPE(), false,
            supportedFormats.contains(BaseDocumentFormats::PLAIN_GENBANK) ? BaseDocumentFormats::PLAIN_GENBANK : supportedFormats.first());
        attrs << new Attribute(BaseAttributes::URL_OUT_ATTRIBUTE(), BaseTypes::STRING_TYPE(), true );
        attrs << new Attribute(BaseAttributes::FILE_MODE_ATTRIBUTE(), BaseTypes::NUM_TYPE(), false, SaveDoc_Roll);
        Descriptor annotationsNameDesc(ANNOTATIONS_NAME, WriteAnnotationsWorker::tr("Annotations name"), 
            WriteAnnotationsWorker::tr("Object name of saving annotations"));
        attrs << new Attribute(annotationsNameDesc, BaseTypes::STRING_TYPE(), false, QVariant(ANNOTATIONS_NAME_DEF_VAL));
    }
    
    Descriptor protoDesc(WriteAnnotationsWorkerFactory::ACTOR_ID, 
        WriteAnnotationsWorker::tr("Write annotations"), 
        WriteAnnotationsWorker::tr("Writes all supplied annotations to file(s) in selected formatId."));
    ActorPrototype * proto = new IntegralBusActorPrototype(protoDesc, portDescs, attrs);
    
    // proto delegates
    QMap<QString, PropertyDelegate*> delegates;
    {
        QVariantMap m;
        foreach( const DocumentFormatId & fid, supportedFormats ) {
            m[fid] = fid;
        }
        delegates[BaseAttributes::DOCUMENT_FORMAT_ATTRIBUTE().getId()] = new ComboBoxDelegate(m);
        delegates[BaseAttributes::URL_OUT_ATTRIBUTE().getId()] = new URLDelegate(
            DialogUtils::prepareDocumentsFileFilterByObjType(GObjectTypes::ANNOTATION_TABLE, true), QString(), false );
        delegates[BaseAttributes::FILE_MODE_ATTRIBUTE().getId()] = new FileModeDelegate(attrs.size() > 2);
    }
    proto->setEditor(new DelegateEditor(delegates));
    proto->setPrompter(new WriteAnnotationsPrompter());
    proto->setValidator(new ScreenedParamValidator(BaseAttributes::URL_OUT_ATTRIBUTE().getId(), 
        BasePorts::IN_ANNOTATIONS_PORT_ID(), BaseSlots::URL_SLOT().getId()));
    proto->setPortValidator(BasePorts::IN_ANNOTATIONS_PORT_ID(), new ScreenedSlotValidator(BaseSlots::URL_SLOT().getId()));
    WorkflowEnv::getProtoRegistry()->registerProto(BaseActorCategories::CATEGORY_DATASINK(), proto);
    WorkflowEnv::getDomainRegistry()->getById(LocalDomainFactory::ID)->registerEntry(new WriteAnnotationsWorkerFactory());
}

Worker * WriteAnnotationsWorkerFactory::createWorker(Actor* a) {
    return new WriteAnnotationsWorker(a);
}

/***************************
 * WriteAnnotationsPrompter
 ***************************/
QString WriteAnnotationsPrompter::composeRichDoc() {
    QString unsetStr = "<font color='red'>"+tr("unset")+"</font>";
    IntegralBusPort * input = qobject_cast<IntegralBusPort*>(target->getPort(BasePorts::IN_ANNOTATIONS_PORT_ID()));
    QString annName = getProducers(BasePorts::IN_ANNOTATIONS_PORT_ID(), BaseSlots::ANNOTATION_TABLE_SLOT().getId());
    annName = annName.isEmpty() ? unsetStr : annName;
    
    QString url = getScreenedURL(input, BaseAttributes::URL_OUT_ATTRIBUTE().getId(), BaseSlots::URL_SLOT().getId());
    QString format = getParameter(BaseAttributes::DOCUMENT_FORMAT_ATTRIBUTE().getId()).value<QString>();
    
    return tr("Save all annotations from <u>%1</u> to %2 in %3 format")
        .arg(annName)
        .arg(getHyperlink(BaseAttributes::URL_OUT_ATTRIBUTE().getId(), url))
        .arg(getHyperlink(BaseAttributes::DOCUMENT_FORMAT_ATTRIBUTE().getId(), format));
}

} // LocalWorkflow
} // U2
