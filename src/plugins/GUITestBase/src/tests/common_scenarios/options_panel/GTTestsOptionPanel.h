/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2013 UniPro <ugene@unipro.ru>
 * http://ugene.unipro.ru
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
k
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

#ifndef GTTESTS_OPTION_PANEL_H_
#define GTTESTS_OPTION_PANEL_H_

#include <U2Test/GUITestBase.h>
#include "api/GTKeyboardDriver.h"
#include "api/GTWidget.h"
#include "GTUtilsDialog.h"

namespace U2 {

class EnterClicker : public Filler {
public:
    EnterClicker(U2OpStatus &_os)
        :Filler(_os,"SequenceReadingModeSelectorDialog"){}
    virtual void run(){
        GTGlobals::sleep(1000);
#ifdef Q_OS_MAC
        QWidget* dialog = QApplication::activeModalWidget();
        QList<QPushButton*> list= dialog->findChildren<QPushButton*>();

        foreach(QPushButton* but, list){
            if (but->text().contains("OK"))
                GTWidget::click(os,but);
        }
#else
        GTKeyboardDriver::keyClick(os,GTKeyboardDriver::key["enter"]);
#endif
    }
private:

};

namespace GUITest_common_scenarios_options_panel {
#undef GUI_TEST_PREFIX
#define GUI_TEST_PREFIX "GUITest_common_scenarios_options_panel_"

GUI_TEST_CLASS_DECLARATION(test_0001)
GUI_TEST_CLASS_DECLARATION(test_0001_1)

GUI_TEST_CLASS_DECLARATION(test_0002)
GUI_TEST_CLASS_DECLARATION(test_0002_1)

GUI_TEST_CLASS_DECLARATION(test_0003)
GUI_TEST_CLASS_DECLARATION(test_0003_1)

GUI_TEST_CLASS_DECLARATION(test_0004)

GUI_TEST_CLASS_DECLARATION(test_0005)

GUI_TEST_CLASS_DECLARATION(test_0006)
GUI_TEST_CLASS_DECLARATION(test_0006_1)


#undef GUI_TEST_PREFIX
} // namespace U2

} //namespace

#endif // GTTESTSMSAEDITOR_H
