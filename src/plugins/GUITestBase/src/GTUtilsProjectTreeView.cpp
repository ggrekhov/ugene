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

#include "GTUtilsProjectTreeView.h"
#include "api/GTMouseDriver.h"
#include "api/GTKeyboardDriver.h"
#include "api/GTWidget.h"
#include "GTUtilsTreeView.h"
#include <U2Core/ProjectModel.h>

namespace U2 {

#define GT_CLASS_NAME "GTUtilsProjectTreeView"

const QString GTUtilsProjectTreeView::widgetName = "documentTreeWidget";

#define GT_METHOD_NAME "openView"
void GTUtilsProjectTreeView::openView(U2OpStatus& os) {

    GTGlobals::FindOptions options;
    options.failIfNull = false;

    QWidget *documentTreeWidget = GTWidget::findWidget(os, widgetName, NULL, options);
    if (!documentTreeWidget) {
        toggleView(os);
    }
    GTGlobals::sleep(500);

    documentTreeWidget = GTWidget::findWidget(os, widgetName, NULL, options);
    GT_CHECK(documentTreeWidget != NULL, "Can't open document tree widget view, findWidget returned NULL");
}
#undef GT_METHOD_NAME

void GTUtilsProjectTreeView::toggleView(U2OpStatus& os) {

    GTKeyboardDriver::keyClick(os, '1', GTKeyboardDriver::key["alt"]);
}

void GTUtilsProjectTreeView::rename(U2OpStatus &os, const QString &itemName, const QString &newItemName) {

    GTMouseDriver::moveTo(os, getTreeViewItemPosition(os, itemName));
    GTMouseDriver::click(os);

    GTKeyboardDriver::keyClick(os, GTKeyboardDriver::key["F2"]);
    GTKeyboardDriver::keySequence(os, newItemName);
    GTKeyboardDriver::keyClick(os, GTKeyboardDriver::key["Enter"]);

    GTGlobals::sleep(500);
}

#define GT_METHOD_NAME "getTreeViewItemRect"
QRect GTUtilsProjectTreeView::getTreeViewItemRect(U2OpStatus &os, const QString &itemName) {

    openView(os);
    QTreeWidget *treeWidget = getTreeWidget(os);
    GT_CHECK_RESULT(treeWidget != NULL, "treeWidget " + itemName + " is NULL", QRect());

    QTreeWidgetItem *item = getTreeWidgetItem(os, itemName);
    GT_CHECK_RESULT(item != NULL, "treeWidgetItem " + itemName + " is NULL", QRect());

    GTUtilsTreeView::expandTo(os, treeWidget, item);
    GT_CHECK_RESULT(item->isHidden() == false, "item " + itemName + " is hidden", QRect());

    return treeWidget->visualItemRect(item);
}
#undef GT_METHOD_NAME

#define GT_METHOD_NAME "getTreeViewItemPosition"
QPoint GTUtilsProjectTreeView::getTreeViewItemPosition(U2OpStatus &os, const QString &itemName) {

    QRect r = getTreeViewItemRect(os, itemName);

    QTreeWidget *treeWidget = getTreeWidget(os);
    GT_CHECK_RESULT(treeWidget != NULL, "treeWidget " + itemName + " is NULL", QPoint());

    return treeWidget->mapToGlobal(r.center());
}
#undef GT_METHOD_NAME

QTreeWidget* GTUtilsProjectTreeView::getTreeWidget(U2OpStatus &os) {

    QTreeWidget *treeWidget = static_cast<QTreeWidget*>(GTWidget::findWidget(os, widgetName));
    return treeWidget;
}

QString GTUtilsProjectTreeView::getProjectTreeItemName(ProjViewItem* projViewItem) {

    if (projViewItem->isDocumentItem()) {
        ProjViewDocumentItem *documentItem = (ProjViewDocumentItem*)projViewItem;
        CHECK_EXT((documentItem != NULL) && (documentItem->doc != NULL),,"");
        return documentItem->doc->getName();
    }

    if (projViewItem->isObjectItem()) {
        ProjViewObjectItem *objectItem = (ProjViewObjectItem*)projViewItem;
        CHECK_EXT((objectItem!=NULL) && (objectItem->obj != NULL),,"");
        return objectItem->obj->getGObjectName();
    }

    if (projViewItem->isTypeItem()) {
        ProjViewTypeItem *typeItem = (ProjViewTypeItem*)projViewItem;
        CHECK_EXT(typeItem != NULL,,"");
        return typeItem->typePName;
    }

    return "";
}

QList<ProjViewItem*> GTUtilsProjectTreeView::getProjectViewItems(QTreeWidgetItem* root) {

    QList<ProjViewItem*> treeItems;

    for (int i=0; i<root->childCount(); i++) {
        treeItems.append((ProjViewItem*)root->child(i));
        treeItems.append(getProjectViewItems(root->child(i)));
    }

    return treeItems;
}


QTreeWidgetItem* GTUtilsProjectTreeView::getTreeWidgetItem(QTreeWidget* tree, const QString &itemName) {

    if (itemName.isEmpty()) {
        return NULL;
    }

    QList<ProjViewItem*> treeItems = getProjectViewItems(tree->invisibleRootItem());
    foreach (ProjViewItem* item, treeItems) {
        QString treeItemName = getProjectTreeItemName(item);
        if (treeItemName == itemName) {
            return item;
        }
    }

    return NULL;
}

#define GT_METHOD_NAME "getTreeWidgetItem"
QTreeWidgetItem* GTUtilsProjectTreeView::getTreeWidgetItem(U2OpStatus &os, const QString &itemName) {

    QTreeWidget *treeWidget = getTreeWidget(os);
    GT_CHECK_RESULT(treeWidget != NULL, "Tree widget not found", NULL);

    QTreeWidgetItem *item = getTreeWidgetItem(treeWidget, itemName);
    GT_CHECK_RESULT(item != NULL, "Item " + itemName + " not found in tree widget", NULL);

    return item;
}
#undef GT_METHOD_NAME

#undef GT_CLASS_NAME

}
