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

#ifndef _U2_AUTO_ANNOTATION_UTILS_H_
#define _U2_AUTO_ANNOTATION_UTILS_H_

#include "ADVSingleSequenceWidget.h"

namespace U2 {

class MWMDIWindow;
class AutoAnnotationObject;

class AutoAnnotationsADVAction : public ADVSequenceWidgetAction {
    Q_OBJECT
public:
    AutoAnnotationsADVAction(ADVSequenceWidget* widget, AutoAnnotationObject* aaObj);
    ~AutoAnnotationsADVAction();
    QList<QAction*> getToggleActions();
    AutoAnnotationObject* getAAObj() {return aaObj; }
    QAction* findToggleAction(const QString& groupName);
    static const QString ACTION_NAME;

private slots:
    void sl_toggle(bool toggled);
    void sl_autoAnnotationLockStateChanged();
private:
    void updateMenu();
    AutoAnnotationObject* aaObj;
    QMenu* menu;
};

class U2VIEW_EXPORT AutoAnnotationUtils {
public:
    static AutoAnnotationsADVAction* findAutoAnnotationADVAction(ADVSequenceObjectContext* ctx);
    static QAction* findAutoAnnotationsToggleAction(ADVSequenceObjectContext* ctx, const QString& name);
    static void triggerAutoAnnotationsUpdate(ADVSequenceObjectContext* ctx, const QString& aaGroupName);
};

}

#endif // _U2_AUTO_ANNOTATION_UTILS_H_
