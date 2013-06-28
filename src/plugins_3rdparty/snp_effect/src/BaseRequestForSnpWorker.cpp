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
#include <U2Core/AppContext.h>
#include <U2Core/U2VariantDbi.h>
#include <U2Core/MultiTask.h>

#include <U2Formats/Database.h>
#include <U2Formats/S3DatabaseUtils.h>

#include <U2Lang/BasePorts.h>
#include <U2Lang/BaseSlots.h>

#include "RequestForSnpTask.h"
#include "BaseRequestForSnpWorker.h"

namespace U2 {

namespace LocalWorkflow {

BaseRequestForSnpWorker::BaseRequestForSnpWorker( Actor *p )
    : BaseWorker( p ), inChannel( NULL ), outChannel( NULL )
{

}

void BaseRequestForSnpWorker::init( )
{
    inChannel = ports.value( BasePorts::IN_VARIATION_TRACK_PORT_ID( ) );
    outChannel = ports.value( BasePorts::OUT_VARIATION_TRACK_PORT_ID( ) );
}

Task* BaseRequestForSnpWorker::tick( )
{
    U2OpStatus2Log os;
    if ( inChannel->hasMessage( ) ) {
        Message m = getMessageAndSetupScriptValues( inChannel );
        QVariantMap data = m.getData( ).toMap( );

        QVariant inVar;
        if ( !data.contains(BaseSlots::VARIATION_TRACK_SLOT( ).getId( ) ) ) {
            os.setError( "Variations slot is empty" );
            return new FailTask( os.getError( ) );
        }

        QScopedPointer<VariantTrackObject> trackObj( NULL );
        {
            SharedDbiDataHandler objId = data.value( BaseSlots::VARIATION_TRACK_SLOT( ).getId( ) )
                .value<SharedDbiDataHandler>( );
            trackObj.reset( StorageUtils::getVariantTrackObject( context->getDataStorage( ), objId ) );
            SAFE_POINT( NULL != trackObj.data( ), tr( "Can't get track object" ), NULL );

        }
        U2VariantTrack track = trackObj->getVariantTrack( os );
        if ( os.hasError( ) ) {
            return new FailTask( os.getError( ) );
        }

        U2OpStatusImpl os;
        U2DbiRef dbiRef = trackObj->getEntityRef( ).dbiRef;
        DbiConnection* sessionHandle = new DbiConnection(dbiRef, os);
        CHECK_OP(os, NULL);
        QScopedPointer<DbiConnection> session(sessionHandle);
        U2Dbi* sessionDbi = session->dbi;
        if(sessionDbi == NULL){
            outChannel->put( Message::getEmptyMapMessage( ) );
            return NULL;
        }    
        U2VariantDbi* varDbi = sessionDbi->getVariantDbi();
        if(varDbi == NULL){
            outChannel->put( Message::getEmptyMapMessage( ) );
            return NULL;
        }
        QScopedPointer <Database> db(S3DatabaseUtils::openDatabase(getDatabasePath()));
        if(db.isNull() || db->getDbi().dbi == NULL){
            outChannel->put( Message::getEmptyMapMessage( ) );
            return NULL;
        }
        U2Dbi* dbDbi = db->getDbi().dbi;
        QList<Task*> tasks;
        QScopedPointer<U2DbiIterator<U2Variant> > snpIter( varDbi->getVariants(track.id, U2_REGION_MAX, os));
        CHECK_OP(os, NULL);
        while(snpIter->hasNext()){
            const U2Variant& var = snpIter->next();
            Task* t = new RequestForSnpTask( getRequestingScriptPath( ), getInputDataForRequest( var, track, dbDbi ), var);
            connect( t, SIGNAL( si_stateChanged( ) ), SLOT( sl_taskFinished( ) ) );
            tasks.append(t);
        }

        if (!tasks.isEmpty()){
            SequentialMultiTask* trackTasks = new SequentialMultiTask(tr("Requesting data for SNPs"), tasks, TaskFlags_NR_FOSCOE);
            connect( trackTasks, SIGNAL( si_stateChanged( ) ), SLOT( sl_trackTaskFinished( ) ) );
            return trackTasks;
        }else{
            outChannel->put( Message::getEmptyMapMessage( ) );
            return NULL;
        }
    }
    if ( inChannel->isEnded( ) ) {
        setDone( );
        outChannel->setEnded( );
    }
    return NULL;
}

void BaseRequestForSnpWorker::sl_taskFinished( )
{
    RequestForSnpTask *t = dynamic_cast<RequestForSnpTask *>( sender( ) );
    SAFE_POINT( NULL != t, "Invalid task is encountered", );
    if ( !t->isFinished( ) || t->hasError( ) ) {
        return;
    }
    resultCache.insert(t->getVariant().id, qMakePair(t->getVariant(), t->getResult()));
    if (checkFlushCache()){
        flushCache();
    }
}
void BaseRequestForSnpWorker::sl_trackTaskFinished( ){
    flushCache();
    outChannel->put( Message::getEmptyMapMessage( ) );
    if ( inChannel->isEnded( ) && !inChannel->hasMessage( ) ) {
        setDone( );
        outChannel->setEnded( );
    }
}

QString BaseRequestForSnpWorker::getRequestingScriptPath( ) const
{
    QString result( AppContext::getWorkingDirectoryPath( )
        + "/../../data/snp_scripts/" + getRequestingScriptName( ) );
    QFileInfo info( result );
    return info.absoluteFilePath( );
}

#define FLUSH_CACHE_ITEMS_SIZE 1000
bool BaseRequestForSnpWorker::checkFlushCache(){
    int size = resultCache.size();
    if (size < FLUSH_CACHE_ITEMS_SIZE){
        return false;
    }
    return true;
}

void BaseRequestForSnpWorker::flushCache(){
    U2OpStatusImpl os;
    U2DbiRef dbiRef = context->getDataStorage()->getDbiRef();
    DbiConnection* sessionHandle = new DbiConnection(dbiRef, os);
    CHECK_OP(os, );
    QScopedPointer<DbiConnection> session(sessionHandle);
    U2Dbi* sessionDbi = session->dbi;
    if(sessionDbi == NULL){
        return;
    }    
    
    foreach(const U2DataId& var, resultCache.keys()){

        handleResult(resultCache.value(var).first, resultCache.value(var).second, sessionDbi);
    }
    clearCache();
}

void BaseRequestForSnpWorker::clearCache(){
    resultCache.clear();
}


} // namespace LocalWorkflow

} // namespace U2