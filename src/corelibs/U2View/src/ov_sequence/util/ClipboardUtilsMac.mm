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

#import "ClipboardUtilsMac.h"
#import <AppKit/NSPasteboard.h>

namespace U2 {

bool putIntoMacClipboard(const QString &data) {
    @try {
        NSPasteboard *p = [NSPasteboard generalPasteboard];
        [p declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
        [p setString:@"" forType:NSStringPboardType];
        NSString *str = [NSString stringWithCString:data.toStdString().c_str() encoding:[NSString defaultCStringEncoding]];
        [p setString:str forType:NSStringPboardType];
    }
    @catch (...) {
        return false;
    }
    return true;
}

} // namespace
