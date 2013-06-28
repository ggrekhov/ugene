/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2013 UniPro <ugene@unipro.ru>
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

#include <U2Core/FailTask.h>
#include <U2Core/U2OpStatusUtils.h>
#include <U2Core/U2SafePoints.h>
#include <U2Core/QVariantUtils.h>
#include <U2Core/AppContext.h>

#include <U2Designer/DelegateEditors.h>

#include <U2Lang/ActorPrototypeRegistry.h>
#include <U2Lang/BaseActorCategories.h>
#include <U2Lang/BaseTypes.h>
#include <U2Lang/BaseSlots.h>
#include <U2Lang/BasePorts.h>
#include <U2Lang/WorkflowEnv.h>
#include <U2Lang/WorkflowMonitor.h>

#include "SNPReportWriter.h"

namespace U2 {
namespace LocalWorkflow {

const QString SNPReportWriterFactory::ACTOR_ID("snp-report-writer-id");

static const QString REPORT_PATH("report_path");
static const QString DB_PATH("db_path");

/************************************************************************/
/* Worker */
/************************************************************************/
SNPReportWriter::SNPReportWriter(Actor *p)
    : BaseWorker(p)
    , inChannel(NULL)
{

}

void SNPReportWriter::init() {
    inChannel = ports.value(BasePorts::IN_VARIATION_TRACK_PORT_ID());
}

Task* SNPReportWriter::tick() {
    U2OpStatus2Log os;

    SNPReportWriterSettings settings = createSNPWriterSettings(os);
    if (os.hasError()) {
        return new FailTask(os.getError());
    }

    while (inChannel->hasMessage()) {
        Message m = getMessageAndSetupScriptValues(inChannel);
        QVariantMap data = m.getData().toMap();

        QVariant inVar;
        if (!data.contains(BaseSlots::VARIATION_TRACK_SLOT().getId())) {
            os.setError("Variations slot is empty");
            return new FailTask(os.getError());
        }

        QScopedPointer<VariantTrackObject> trackObj(NULL);
        {
            SharedDbiDataHandler objId = data.value(BaseSlots::VARIATION_TRACK_SLOT().getId())
                .value<SharedDbiDataHandler>();
            trackObj.reset(StorageUtils::getVariantTrackObject(context->getDataStorage(), objId));
            SAFE_POINT(NULL != trackObj.data(), tr("Can't get track object"), NULL);

        }
      

        U2VariantTrack track = trackObj->getVariantTrack(os);
        if(os.hasError()){
            return new FailTask(os.getError());
        }
        tracks.append(track);

        U2DbiRef curDbiRef = trackObj->getEntityRef().dbiRef;
        if (!dbiRef.isValid()){
            dbiRef = curDbiRef;
        }else{
            SAFE_POINT(dbiRef == curDbiRef, SNPReportWriter::tr("Tracks are stored in different databases"), NULL);
        }
       
    }

    if (!inChannel->isEnded()) {
        return NULL;
    }

    Task* t = new SNPReportWriterTask(settings, tracks, dbiRef);
    connect(t, SIGNAL(si_stateChanged()), SLOT(sl_taskFinished()));
    return t;


    if (inChannel->isEnded()) {
        setDone();
    }
    return NULL;
}


void SNPReportWriter::sl_taskFinished() {
    SNPReportWriterTask *t = dynamic_cast<SNPReportWriterTask*>(sender());
    if (!t->isFinished() || t->hasError()) {
        return;
    }
    if (inChannel->isEnded() && !inChannel->hasMessage()) {
        setDone();
    }
    if (!t->getOutputFilePath().isEmpty()){
        context->getMonitor()->addOutputFile(t->getOutputFilePath(), getActor()->getId());
    }
    
}

void SNPReportWriter::cleanup(){
    tracks.clear();
    dbiRef = U2DbiRef();
}

SNPReportWriterSettings SNPReportWriter::createSNPWriterSettings( U2OpStatus &os ){
    SNPReportWriterSettings settings;

    settings.reportPath = getValue<QString>(REPORT_PATH);
    settings.dbPath = getValue<QString>(DB_PATH);

    return settings;
}

/************************************************************************/
/* Factory */
/************************************************************************/


void SNPReportWriterFactory::init() {
    QList<PortDescriptor*> p; QList<Attribute*> a;
    {
        Descriptor sd(BasePorts::IN_VARIATION_TRACK_PORT_ID(), "Input variations", "Variations for annotations.");

        QMap<Descriptor, DataTypePtr> inM;
        inM[BaseSlots::VARIATION_TRACK_SLOT()] = BaseTypes::VARIATION_TRACK_TYPE();
        p << new PortDescriptor(sd, DataTypePtr(new MapDataType("in.variations", inM)), true /*input*/);
    }

    QList<Attribute*> attrs;
    {
        Descriptor reportPath(REPORT_PATH,
            SNPReportWriter::tr("Report path"),
            SNPReportWriter::tr("Path to save SNP effects reports."));


        attrs << new Attribute(reportPath, BaseTypes::STRING_TYPE(), true, "");

        Descriptor dbPath(DB_PATH,
            SNPReportWriter::tr("Database path"),
            SNPReportWriter::tr("Path to SNP database."));


        attrs << new Attribute(dbPath, BaseTypes::STRING_TYPE(), true, "");
    }

    QMap<QString, PropertyDelegate*> delegates;
    {
        delegates[REPORT_PATH] = new URLDelegate("", "", false);
        delegates[DB_PATH] = new URLDelegate("", "", false);
    }

    Descriptor protoDesc(SNPReportWriterFactory::ACTOR_ID,
        SNPReportWriter::tr("Write SNP report"),
        SNPReportWriter::tr("Use variations and their effects to write a report."));

    ActorPrototype *proto = new IntegralBusActorPrototype(protoDesc, p, attrs);
    proto->setPrompter(new SNPReportPrompter());
    proto->setEditor(new DelegateEditor(delegates));
    WorkflowEnv::getProtoRegistry()->registerProto(BaseActorCategories::CATEGORY_SCHEMAS(), proto);
    WorkflowEnv::getDomainRegistry()->getById(LocalDomainFactory::ID)->registerEntry(new SNPReportWriterFactory());
}

Worker *SNPReportWriterFactory::createWorker(Actor *a) {
    return new SNPReportWriter(a);
}

QString SNPReportPrompter::composeRichDoc() {
    QString res = ""; 

    Actor* annProducer = qobject_cast<IntegralBusPort*>(target->getPort(BasePorts::IN_VARIATION_TRACK_PORT_ID()))->getProducer(BaseSlots::VARIATION_TRACK_SLOT().getId());

    QString unsetStr = "<font color='red'>"+tr("unset")+"</font>";
    QString annUrl = annProducer ? annProducer->getLabel() : unsetStr;

    QString path = getHyperlink(REPORT_PATH, getURL(REPORT_PATH));

    res.append(tr("Uses SNPs from <u>%1</u> as input.").arg(annUrl));
    res.append(tr(" Writes SNP report to <u>%1</u>.").arg(path.isEmpty() ? unsetStr : path));

    return res;
}

} // LocalWorkflow
} // U2
