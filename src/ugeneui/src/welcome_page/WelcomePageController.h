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

#ifndef _U2_WELCOME_PAGE_CONTROLLER_H_
#define _U2_WELCOME_PAGE_CONTROLLER_H_

#include <QObject>

#include <U2Core/U2OpStatus.h>

namespace U2 {

class MWMDIManager;
class MWMDIWindow;
class WelcomePageMdi;

class WelcomePageController : public QObject {
    Q_OBJECT
public:
    WelcomePageController();

    void onPageLoaded();

public slots:
    void sl_showPage();
    void sl_onRecentChanged();

    // javascript
    void performAction(const QString &actionId);
    void openUrl(const QString &urlId);
    void openFile(const QString &url);

private slots:
    void sl_onMdiClose(MWMDIWindow *mdi);

private:
    static QString getUrlById(const QString &urlId);
    static MWMDIManager * getMdiManager();

private:
    WelcomePageMdi *welcomePage;
};

} // U2

#endif // _U2_WELCOME_PAGE_CONTROLLER_H_
