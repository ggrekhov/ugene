/**
* UGENE - Integrated Bioinformatics Tools.
* Copyright (C) 2008-2017 UniPro <ugene@unipro.ru>
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

#ifndef GTUTILSMCAEDITORSEQUENCEAREA_H
#define GTUTILSMCAEDITORSEQUENCEAREA_H

#include <GTGlobals.h>

#include <U2View/McaEditorSequenceArea.h>

namespace U2 {
    using namespace HI;

class GTUtilsMcaEditorSequenceArea {
public:
    static McaEditorSequenceArea* getSequenceArea(HI::GUITestOpStatus &os);
    static QStringList getVisibleNames(HI::GUITestOpStatus &os);
    static int getRowHeight(HI::GUITestOpStatus &os, int rowNumber);
    static void clickToPosition(HI::GUITestOpStatus &os, const QPoint &globalMaPosition);
    static void scrollToPosition(HI::GUITestOpStatus &os, const QPoint &position);
    static void clickCollapseTriangle(HI::GUITestOpStatus &os, QString seqName, bool showChromatogram);
    static bool isChromatogramShown(HI::GUITestOpStatus &os, QString seqName);
    static QStringList getNameList(HI::GUITestOpStatus &os);
    static void callContextMenu(HI::GUITestOpStatus &os, const QPoint &innerCoords = QPoint());
    static void moveTo(HI::GUITestOpStatus &os, const QPoint &p);
    static QPoint convertCoordinates(HI::GUITestOpStatus &os, const QPoint p);
    static QString getReferenceReg(HI::GUITestOpStatus &os, int num, int length);
    static void moveTheBorderBetweenAlignmentAndRead(HI::GUITestOpStatus &os, int shift);
    static void dragAndDrop(HI::GUITestOpStatus &os, const QPoint p);
    static int getSelectedRowsNum(GUITestOpStatus &os);
    static QRect getSelectedRect(GUITestOpStatus &os);
    static qint64 getReferenceSelectedNum(GUITestOpStatus &os);
    static void clickToReferencePosition(GUITestOpStatus &os, const qint64 num);
};

}//namespace
#endif // GTUTILSMSAEDITORSEQUENCEAREA_H
