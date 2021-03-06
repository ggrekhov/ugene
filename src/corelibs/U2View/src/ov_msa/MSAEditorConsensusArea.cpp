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

#include <U2Gui/GUIUtils.h>

#include "MSAEditor.h"
#include "MSAEditorConsensusArea.h"
#include "view_rendering/MaConsensusAreaRenderer.h"


namespace U2 {

/************************************************************************/
/* MSAEditorConsensusArea */
/************************************************************************/
MSAEditorConsensusArea::MSAEditorConsensusArea(MsaEditorWgt *ui)
    : MaEditorConsensusArea(ui) {
    initRenderer();
    setupFontAndHeight();

    connect(editor, SIGNAL(si_buildStaticMenu(GObjectView *, QMenu *)), SLOT(sl_buildStaticMenu(GObjectView *, QMenu *)));
    connect(editor, SIGNAL(si_buildPopupMenu(GObjectView * , QMenu *)), SLOT(sl_buildContextMenu(GObjectView *, QMenu *)));
}

void MSAEditorConsensusArea::sl_buildStaticMenu(GObjectView * /*view*/, QMenu *menu) {
    buildMenu(menu);
}

void MSAEditorConsensusArea::sl_buildContextMenu(GObjectView * /*view*/, QMenu *menu) {
    buildMenu(menu);
}

void MSAEditorConsensusArea::initRenderer() {
    renderer = new MaConsensusAreaRenderer(this);
}

void MSAEditorConsensusArea::buildMenu(QMenu *menu) {
    QMenu* copyMenu = GUIUtils::findSubMenu(menu, MSAE_MENU_COPY);
    SAFE_POINT(copyMenu != NULL, "copyMenu", );
    copyMenu->addAction(copyConsensusAction);
    copyMenu->addAction(copyConsensusWithGapsAction);

    menu->addAction(configureConsensusAction);
}

} // namespace U2
