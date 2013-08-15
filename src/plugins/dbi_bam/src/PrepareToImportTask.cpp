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


#include <U2Core/AppContext.h>
#include <U2Core/AppSettings.h>
#include <U2Core/UserApplicationsSettings.h>
#include <U2Formats/BAMUtils.h>


#include "PrepareToImportTask.h"

namespace U2 {
namespace BAM {

bool PrepareToImportTask::checkStatus( U2OpStatusImpl &status ) {
    if( status.hasError() ) {
        if( status.hasError() ) {
            setError( status.getError() );
            return false;
        }
    }
    return true;
}

void PrepareToImportTask::run() {
    GUrl currentURL;
    QString destinationURL ;
    QString dirPath = QString();
    U2OpStatusImpl status;

    currentURL = sourceURL;
    if( samFormat == true ) {
        dirPath = sourceURL.dirPath();
        if( !dirPath.endsWith("/") ) {
            dirPath.append("/");
        }
        destinationURL = dirPath + currentURL.fileName() + ".bam";
        bool samToBam = true;
        stateInfo.setDescription( "Converting SAM to BAM" );
        BAMUtils::convertToSamOrBam( currentURL, destinationURL, status, samToBam );
        if( !checkStatus( status ) ) {
            return;
        }
        currentURL = destinationURL;
    }
    bool sortedBam = BAMUtils::isSortedBam( currentURL, status );
    if( !checkStatus( status ) ) {
        return;
    }
    if( !sortedBam ) {
        const QString sortedFileName = dirPath + currentURL.fileName() + "_sorted";
        stateInfo.setDescription( "Sorting BAM" );
        currentURL = BAMUtils::sortBam( currentURL, sortedFileName, status );
    } 
    if( !checkStatus( status ) ) {
        return;
    }
    if( !BAMUtils::hasValidBamIndex( currentURL ) ) {
        stateInfo.setDescription( "Creating BAM index" );
        BAMUtils::createBamIndex( currentURL, status );
    }
    checkStatus( status );
    sourceURL = currentURL;
}

const GUrl& PrepareToImportTask::getSourceUrl() const {
    return sourceURL;
}

} // namespace BAM
} // namespace U2