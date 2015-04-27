/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2015 UniPro <ugene@unipro.ru>
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

#include "AlignSequencesToAlignmentTaskSettings.h"

namespace U2 {

/************************************************************************/
/* AlignSequencesToAlignmentTaskSettings */
/************************************************************************/
AlignSequencesToAlignmentTaskSettings::AlignSequencesToAlignmentTaskSettings() : addAsFragments(false), reorderSequences(false), referenceRowId(-1) {}

AlignSequencesToAlignmentTaskSettings::AlignSequencesToAlignmentTaskSettings(const U2EntityRef& _msaRef, const U2AlphabetId& newAlphabet, bool addAsFragments) 
: addAsFragments(addAsFragments), reorderSequences(false), referenceRowId(-1) {
    msaRef = _msaRef;
    alphabet = newAlphabet;
}

AlignSequencesToAlignmentTaskSettings::AlignSequencesToAlignmentTaskSettings(const AlignSequencesToAlignmentTaskSettings &s)
: AbstractAlignmentTaskSettings(s), addedSequencesRefs(s.addedSequencesRefs), addedSequencesNames(s.addedSequencesNames), addAsFragments(s.addAsFragments),
reorderSequences(s.reorderSequences), referenceRowId(s.referenceRowId){
}

bool AlignSequencesToAlignmentTaskSettings::isValid() const {
    return AbstractAlignmentTaskSettings::isValid() && !addedSequencesRefs.isEmpty() && !addedSequencesNames.isEmpty();
}

}   //namespace