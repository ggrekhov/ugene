/**
* UGENE - Integrated Bioinformatics Tools.
* Copyright (C) 2008-2017 UniPro <ugene@unipro.ru>
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

#include <U2Core/AppContext.h>
#include <U2Core/BaseDocumentFormats.h>
#include <U2Core/Counter.h>
#include <U2Core/DNAAlphabet.h>
#include <U2Core/FormatUtils.h>
#include <U2Core/IOAdapterUtils.h>
#include <U2Core/L10n.h>
#include <U2Core/MultipleChromatogramAlignmentObject.h>
#include <U2Core/SaveDocumentTask.h>
#include <U2Core/U2OpStatusUtils.h>
#include <U2Core/U2SafePoints.h>

#include <U2Designer/DelegateEditors.h>

#include <U2Lang/ActorPrototypeRegistry.h>
#include <U2Lang/BaseActorCategories.h>
#include <U2Lang/BasePorts.h>
#include <U2Lang/BaseSlots.h>
#include <U2Lang/BaseTypes.h>
#include <U2Lang/WorkflowEnv.h>
#include <U2Lang/WorkflowMonitor.h>

#include "AlignToReferenceBlastWorker.h"
#include "align_worker_subtasks/BlastReadsSubTask.h"
#include "align_worker_subtasks/ComposeResultSubTask.h"
#include "align_worker_subtasks/FormatDBSubTask.h"
#include "align_worker_subtasks/PrepareReferenceSequenceTask.h"
#include "blast/BlastAllSupport.h"
#include "blast/FormatDBSupport.h"

namespace U2 {
namespace LocalWorkflow {

const QString AlignToReferenceBlastWorkerFactory::ACTOR_ID("align-to-reference");
namespace {
    const QString OUT_PORT_ID = "out";
    const QString REF_ATTR_ID = "reference";
    const QString RESULT_URL_ATTR_ID = "result-url";
    const QString IDENTITY_ID = "identity";

    const QString ALGORITHM = "algorithm";
}

/************************************************************************/
/* AlignToReferenceBlastWorkerFactory */
/************************************************************************/
AlignToReferenceBlastWorkerFactory::AlignToReferenceBlastWorkerFactory()
: DomainFactory(ACTOR_ID)
{

}

Worker * AlignToReferenceBlastWorkerFactory::createWorker(Actor *a) {
    return new AlignToReferenceBlastWorker(a);
}

void AlignToReferenceBlastWorkerFactory::init() {
    QList<PortDescriptor*> ports;
    {
        Descriptor inDesc(BasePorts::IN_SEQ_PORT_ID(), AlignToReferenceBlastPrompter::tr("Input sequence"), AlignToReferenceBlastPrompter::tr("Input sequence."));
        Descriptor outDesc(OUT_PORT_ID, AlignToReferenceBlastPrompter::tr("Aligned data"), AlignToReferenceBlastPrompter::tr("Aligned data."));

        QMap<Descriptor, DataTypePtr> inType;
        inType[BaseSlots::DNA_SEQUENCE_SLOT()] = BaseTypes::DNA_SEQUENCE_TYPE();

        QMap<Descriptor, DataTypePtr> outType;
        outType[BaseSlots::DNA_SEQUENCE_SLOT()] = BaseTypes::DNA_SEQUENCE_TYPE();
        outType[BaseSlots::ANNOTATION_TABLE_SLOT()] = BaseTypes::ANNOTATION_TABLE_TYPE();

        ports << new PortDescriptor(inDesc, DataTypePtr(new MapDataType(ACTOR_ID + "-in", inType)), true /*input*/);
        ports << new PortDescriptor(outDesc, DataTypePtr(new MapDataType(ACTOR_ID + "-out", outType)), false /*input*/, true /*multi*/);
    }
    QList<Attribute*> attributes;
    {
        Descriptor refDesc(REF_ATTR_ID, AlignToReferenceBlastPrompter::tr("Reference URL"),
                           AlignToReferenceBlastPrompter::tr("A URL to the file with a reference sequence."));
        attributes << new Attribute(refDesc, BaseTypes::STRING_TYPE(), true);

        Descriptor outputUrlDesc(RESULT_URL_ATTR_ID, AlignToReferenceBlastPrompter::tr("Result alignment URL"),
                           AlignToReferenceBlastPrompter::tr("An URL to write the result alignment."));
        attributes << new Attribute(outputUrlDesc, BaseTypes::STRING_TYPE(), true);

        Descriptor identityDesc(IDENTITY_ID, AlignToReferenceBlastPrompter::tr("Minimum read identity"),
                                AlignToReferenceBlastPrompter::tr("Reads, whose identity with the reference is less than the stated value, will be ignored."));
        attributes << new Attribute(identityDesc, BaseTypes::NUM_TYPE(), false, 80);
    }

    QMap<QString, PropertyDelegate*> delegates;
    {
        delegates[REF_ATTR_ID] = new URLDelegate("", "", false, false, false);
        delegates[RESULT_URL_ATTR_ID] = new URLDelegate(FormatUtils::prepareDocumentsFileFilter(BaseDocumentFormats::UGENEDB, false, QStringList()), "", false, false, true, NULL, BaseDocumentFormats::UGENEDB);
        QVariantMap m;
        m["minimum"] = 0;
        m["maximum"] = 100;
        m["suffix"] = "%";
        delegates[IDENTITY_ID] = new SpinBoxDelegate(m);
    }

    Descriptor desc(ACTOR_ID, AlignToReferenceBlastWorker::tr("Map to Reference"),
        AlignToReferenceBlastWorker::tr("Align input sequences (e.g. Sanger reads) to the reference sequence."));
    ActorPrototype *proto = new IntegralBusActorPrototype(desc, ports, attributes);
    proto->setEditor(new DelegateEditor(delegates));
    proto->setPrompter(new AlignToReferenceBlastPrompter(NULL));
    proto->addExternalTool(ET_BLASTALL);
    proto->addExternalTool(ET_FORMATDB);
    WorkflowEnv::getProtoRegistry()->registerProto(BaseActorCategories::CATEGORY_ALIGNMENT(), proto);

    DomainFactory *localDomain = WorkflowEnv::getDomainRegistry()->getById(LocalDomainFactory::ID);
    localDomain->registerEntry(new AlignToReferenceBlastWorkerFactory());
}

/************************************************************************/
/* AlignToReferenceBlastPrompter */
/************************************************************************/
AlignToReferenceBlastPrompter::AlignToReferenceBlastPrompter(Actor *a)
: PrompterBase<AlignToReferenceBlastPrompter>(a)
{

}

QString AlignToReferenceBlastPrompter::composeRichDoc() {
    IntegralBusPort *input = qobject_cast<IntegralBusPort*>(target->getPort(BasePorts::IN_SEQ_PORT_ID()));
    SAFE_POINT(NULL != input, "No input port", "");
    Actor *producer = input->getProducer(BaseSlots::DNA_SEQUENCE_SLOT().getId());
    const QString unsetStr = "<font color='red'>"+tr("unset")+"</font>";
    const QString producerName = (NULL != producer) ? producer->getLabel() : unsetStr;
    const QString refLink = getHyperlink(REF_ATTR_ID, getURL(REF_ATTR_ID));
    return tr("Aligns each sequence from <u>%1</u> to the reference sequence from <u>%2</u>.").arg(producerName).arg(refLink);
}

/************************************************************************/
/* AlignToReferenceBlastWorker */
/************************************************************************/
AlignToReferenceBlastWorker::AlignToReferenceBlastWorker(Actor *a)
    : BaseDatasetWorker(a, BasePorts::IN_SEQ_PORT_ID(), OUT_PORT_ID)
{

}

Task *AlignToReferenceBlastWorker::createPrepareTask(U2OpStatus & /*os*/) const {
    const QString referenceUrl = getValue<QString>(REF_ATTR_ID);
    return new PrepareReferenceSequenceTask(referenceUrl, context->getDataStorage()->getDbiRef());
}

void AlignToReferenceBlastWorker::onPrepared(Task *task, U2OpStatus &os) {
    PrepareReferenceSequenceTask *prepareTask = qobject_cast<PrepareReferenceSequenceTask *>(task);
    CHECK_EXT(NULL != prepareTask, os.setError(L10N::internalError("Unexpected prepare task")), );
    reference = context->getDataStorage()->getDataHandler(prepareTask->getReferenceEntityRef());
}

Task * AlignToReferenceBlastWorker::createTask(const QList<Message> &messages) const {
    QList<SharedDbiDataHandler> reads;
    foreach (const Message &message, messages) {
        QVariantMap data = message.getData().toMap();
        if (data.contains(BaseSlots::DNA_SEQUENCE_SLOT().getId())) {
            reads << data[BaseSlots::DNA_SEQUENCE_SLOT().getId()].value<SharedDbiDataHandler>();
        }
    }
    int readIdentity = getValue<int>(IDENTITY_ID);
    return new AlignToReferenceBlastTask(getValue<QString>(REF_ATTR_ID), getValue<QString>(RESULT_URL_ATTR_ID), reference, reads, readIdentity, context->getDataStorage());
}

QVariantMap AlignToReferenceBlastWorker::getResult(Task *task, U2OpStatus &os) const {
    AlignToReferenceBlastTask *alignTask = qobject_cast<AlignToReferenceBlastTask*>(task);
    CHECK_EXT(NULL != alignTask, os.setError(L10N::internalError("Unexpected task")), QVariantMap());

    const QMap<QString, bool> acceptedReads = alignTask->getAcceptedReads();
    const QStringList discardedReads = alignTask->getDiscardedReads();

    algoLog.info(QString("Reads discarded by the mapper: %1").arg(discardedReads.count()));
    foreach (const QString &readName, discardedReads) {
        algoLog.details(readName);
    }
    algoLog.info(QString("Reads accepted by the mapper: %1").arg(acceptedReads.count()));
    foreach (const QString &readName, acceptedReads.keys()) {
        algoLog.details((acceptedReads[readName] ? "&#x2190;&nbsp;&nbsp;" : "&#x2192;&nbsp;&nbsp;") + readName);
    }
    algoLog.info(QString("Total reads processed by the mapper: %1").arg(acceptedReads.count() + discardedReads.count()));

    if (0 != discardedReads.count()) {
        monitor()->addInfo(QString("%1 %2 not mapped").arg(discardedReads.count()).arg(discardedReads.count() == 1 ? "read was" : "reads were"), actor->getId(), Problem::U2_WARNING);
    }

    const QString resultUrl = alignTask->getResultUrl();
    if (QFileInfo(resultUrl).exists()) {
        monitor()->addOutputFile(resultUrl, actor->getId());
    } else {
        os.setError(tr("The result file was not produced"));
    }

    QVariantMap result;
    result[BaseSlots::DNA_SEQUENCE_SLOT().getId()] = qVariantFromValue<SharedDbiDataHandler>(reference);
    result[BaseSlots::ANNOTATION_TABLE_SLOT().getId()] = qVariantFromValue<SharedDbiDataHandler>(alignTask->getAnnotations());
    return result;
}

MessageMetadata AlignToReferenceBlastWorker::generateMetadata(const QString &datasetName) const {
    return MessageMetadata(getValue<QString>(REF_ATTR_ID), datasetName);
}

/************************************************************************/
/* AlignToReferenceBlastTask */
/************************************************************************/
AlignToReferenceBlastTask::AlignToReferenceBlastTask(const QString& refUrl, const QString &resultUrl,
                                                     const SharedDbiDataHandler &reference,
                                                     const QList<SharedDbiDataHandler> &reads,
                                                     int minIdentityPercent,
                                                     DbiDataStorage *storage)
    : Task(tr("Align to reference"), TaskFlags_NR_FOSE_COSC | TaskFlag_ReportingIsSupported | TaskFlag_ReportingIsEnabled),
      referenceUrl(refUrl),
      resultUrl(resultUrl),
      reference(reference),
      reads(reads),
      minIdentityPercent(minIdentityPercent),
      formatDbSubTask(NULL),
      blastTask(NULL),
      composeSubTask(NULL),
      saveTask(NULL),
      storage(storage)
{
    GCOUNTER(cvar, tvar, "AlignToReferenceBlastTask");
}

void AlignToReferenceBlastTask::prepare() {
    formatDbSubTask = new U2::Workflow::FormatDBSubTask(referenceUrl, reference, storage);
    addSubTask(formatDbSubTask);
}

QList<Task*> AlignToReferenceBlastTask::onSubTaskFinished(Task *subTask) {
    QList<Task*> result;
    CHECK(subTask != NULL, result);
    CHECK(!subTask->isCanceled() && !subTask ->hasError(), result);

    if (subTask == formatDbSubTask) {
        QString dbPath = formatDbSubTask->getResultPath();
        blastTask = new BlastReadsSubTask(dbPath, reads, reference, minIdentityPercent, storage);
        result << blastTask;
    } else if (subTask == blastTask) {
        composeSubTask = new ComposeResultSubTask(reference, reads, blastTask->getBlastSubtasks(), storage);
        composeSubTask->setSubtaskProgressWeight(0.5f);
        result << composeSubTask;
    } else if (subTask == composeSubTask) {
        DocumentFormat *ugenedbFormat = AppContext::getDocumentFormatRegistry()->getFormatById(BaseDocumentFormats::UGENEDB);
        QScopedPointer<Document> document(ugenedbFormat->createNewLoadedDocument(IOAdapterUtils::get(IOAdapterUtils::url2io(resultUrl)), resultUrl, stateInfo));
        CHECK_OP(stateInfo, result);

        document->setDocumentOwnsDbiResources(false);

        MultipleChromatogramAlignmentObject *mcaObject = composeSubTask->takeMcaObject();
        SAFE_POINT_EXT(NULL != mcaObject, setError("Result MCA object is NULL"), result);
        document->addObject(mcaObject);

        U2SequenceObject *referenceSequenceObject = composeSubTask->takeReferenceSequenceObject();
        SAFE_POINT_EXT(NULL != referenceSequenceObject, setError("Result reference sequence object is NULL"), result);
        document->addObject(referenceSequenceObject);

        mcaObject->addObjectRelation(GObjectRelation(GObjectReference(referenceSequenceObject), ObjectRole_ReferenceSequence));

        saveTask = new SaveDocumentTask(document.take(), SaveDocFlags(SaveDoc_DestroyAfter) | SaveDoc_Roll);
        result << saveTask;
    }
    return result;
}

QString AlignToReferenceBlastTask::generateReport() const {
    QString result;

    QScopedPointer<U2SequenceObject> refObject(StorageUtils::getSequenceObject(storage, reference));
    CHECK(NULL != refObject, "");

    const QMap<QString, bool> acceptedReads = getAcceptedReads();
    const QStringList filtredReads = getDiscardedReads();

    result += "<br><table><tr><td><b>" + tr("Details") + "</b></td></tr></table>\n";
    result += "<u>" + tr("Reference sequence:") + QString("</u> %1<br>").arg(refObject->getSequenceName());
    result += "<u>" + tr("Aligned reads (%1):").arg(acceptedReads.size()) + "</u>";
    result += "<table>";
    foreach (const QString &readName, acceptedReads.keys()) {
        const QString read = (acceptedReads[readName] ? "&#x2190;&nbsp;&nbsp;" : "&#x2192;&nbsp;&nbsp;") + readName;
        result += "<tr><td width=50>" + tr("") + "</td><td>" + read + "</td></tr>";
    }

    result += "</table>";
    if (!filtredReads.isEmpty()) {
        result += "<br><u>" + tr("Filtered by quality (%1):").arg(filtredReads.size()) + "</u>";
        result += "<table>";
        foreach (const QString &readName, filtredReads) {
            result += "<tr><td width=50></td><td width=300 nowrap>" + readName + "</td></tr>";
        }
        result += "</table>";
    }

    return result;
}

QString AlignToReferenceBlastTask::getResultUrl() const {
    CHECK(NULL != saveTask, "");
    return saveTask->getURL().getURLString();
}

SharedDbiDataHandler AlignToReferenceBlastTask::getAnnotations() const {
    CHECK(NULL != composeSubTask, SharedDbiDataHandler());
    return composeSubTask->getAnnotations();
}

QMap<QString, bool> AlignToReferenceBlastTask::getAcceptedReads() const {
    QMap<QString, bool> acceptedReads;
    CHECK(NULL != blastTask, acceptedReads);
    foreach (BlastAndSwReadTask *subTask, blastTask->getBlastSubtasks()) {
        if (subTask->getReadIdentity() >= minIdentityPercent) {
            acceptedReads.insert(subTask->getReadName(), subTask->isComplement());
        }
    }
    return acceptedReads;
}

QStringList AlignToReferenceBlastTask::getDiscardedReads() const {
    QStringList discardedReads;
    CHECK(NULL != blastTask, discardedReads);
    foreach (BlastAndSwReadTask* subTask, blastTask->getBlastSubtasks()) {
        if (subTask->getReadIdentity() < minIdentityPercent) {
            discardedReads << subTask->getReadName();
        }
    }
    return discardedReads;
}

} // LocalWorkflow
} // U2
