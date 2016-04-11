/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2016 UniPro <ugene@unipro.ru>
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

#ifndef _U2_SAVE_ALIGNMENT_H_
#define _U2_SAVE_ALIGNMENT_H_

#include <U2Core/BaseDocumentFormats.h>
#include <U2Core/DocumentModel.h>
#include <U2Core/Task.h>
#include <U2Core/MAlignment.h>

namespace U2 {

class Document;

/** Save Alignment Task (to CLUSTAL, NEXUS, ...) */
class U2CORE_EXPORT SaveAlignmentTask : public Task {
    Q_OBJECT
public:
    SaveAlignmentTask(const MAlignment& ma, const QString& fileName, DocumentFormatId f, const QVariantMap& hints = QVariantMap());

    void run();

    virtual Document* getDocument() const {return doc.data();}

    MAlignment& getMAlignment() {return ma;}

private:
    MAlignment              ma;
    QString                 fileName;
    QVariantMap             hints;
    DocumentFormatId        format;
    QScopedPointer<Document> doc;
};

} // U2

#endif // _U2_SAVE_ALIGNMENT_H_
