/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2016 UniPro <ugene@unipro.ru>
 * http://ugene.net
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

#include "MSAEditorConsensusCache.h"

#include <U2Algorithm/MSAConsensusAlgorithm.h>

#include <U2Core/MultipleAlignmentObject.h>
#include <U2Core/U2SafePoints.h>

namespace U2 {

MSAEditorConsensusCache::MSAEditorConsensusCache(QObject* p, MultipleAlignmentObject* o, MSAConsensusAlgorithmFactory* factory)
: QObject(p), curCacheSize(0), aliObj(o), algorithm(NULL)
{
    setConsensusAlgorithm(factory);

    connect(aliObj, SIGNAL(si_alignmentChanged(const MultipleAlignment&, const MaModificationInfo&)),
        SLOT(sl_alignmentChanged()));
    connect(aliObj, SIGNAL(si_invalidateAlignmentObject()), SLOT(sl_invalidateAlignmentObject()));

    curCacheSize = aliObj->getLength();
    updateMap.resize(curCacheSize);
    cache.resize(curCacheSize);
}

MSAEditorConsensusCache::~MSAEditorConsensusCache() {
    delete algorithm;
    algorithm = NULL;
}

void MSAEditorConsensusCache::setConsensusAlgorithm(MSAConsensusAlgorithmFactory* factory) {
    delete algorithm;
    algorithm = NULL;
    algorithm = factory->createAlgorithm(aliObj->getMultipleAlignment());
    connect(algorithm, SIGNAL(si_thresholdChanged(int)), SLOT(sl_thresholdChanged(int)));
    updateMap.fill(false);
}

void MSAEditorConsensusCache::sl_alignmentChanged() {
    if(curCacheSize != aliObj->getLength()) {
        curCacheSize = aliObj->getLength();
        updateMap.resize(curCacheSize);
        cache.resize(aliObj->getLength());
    }
    updateMap.fill(false);
}

void MSAEditorConsensusCache::updateCacheItem(int pos) {
    if(!updateMap.at(pos) && aliObj != NULL) {
        const MultipleAlignment ma = aliObj->getMultipleAlignment();

        QString errorMessage = tr("Can not update consensus chache item");
        SAFE_POINT(pos >= 0 && pos < curCacheSize, errorMessage,);
        SAFE_POINT(curCacheSize == ma->getLength(), errorMessage,);

        CacheItem& ci = cache[pos];
        int count = 0;
        int nSeq = ma->getNumRows();
        SAFE_POINT(0 != nSeq, errorMessage,);

        ci.topChar = algorithm->getConsensusCharAndScore(ma, pos, count);
        ci.topPercent = (char)qRound(count * 100. / nSeq);
        assert(ci.topPercent >=0 && ci.topPercent<=100);
        updateMap.setBit(pos, true);
    }
}

char MSAEditorConsensusCache::getConsensusChar(int pos) {
    updateCacheItem(pos);
    const CacheItem& ci = cache[pos];
    return ci.topChar;
}

int MSAEditorConsensusCache::getConsensusCharPercent(int pos) {
    updateCacheItem(pos);
    const CacheItem& ci = cache[pos];
    return ci.topPercent;
}

QByteArray MSAEditorConsensusCache::getConsensusLine(bool withGaps) {
    QByteArray res;
    const MultipleAlignment ma = aliObj->getMultipleAlignment();
    for (int i = 0, n = ma->getLength(); i < n; i++) {
        char c = getConsensusChar(i);
        if (c!=U2Msa::GAP_CHAR || withGaps) {
            res.append(c);
        }
    }
    return res;
}

void MSAEditorConsensusCache::sl_thresholdChanged(int newValue) {
    Q_UNUSED(newValue);
    updateMap.fill(false);
}

void MSAEditorConsensusCache::sl_invalidateAlignmentObject() {
    aliObj = NULL;
}

}//namespace

