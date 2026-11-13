/*******************************************************************************
 *
 * libcontemporary - Common libraries
 * Copyright (C) 2022 Victor Tran
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

#include "tnotification.h"

#include <QDebug>
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

@interface AppNotificationDelegate : NSResponder<NSUserNotificationCenterDelegate>

@end

@implementation AppNotificationDelegate
- (BOOL)userNotificationCenter:(NSUserNotificationCenter*)center shouldPresentNotification:(NSUserNotification*)notification {
    return YES;
}

- setDelegateForUserNotificationCenter:(NSUserNotificationCenter*) center {
    center.delegate = self;
}
@end

struct tNotificationPrivateByOS {
    static bool registeredAppDelegate;
    static void registerDelegate();
};
bool tNotificationPrivateByOS::registeredAppDelegate = false;

void tNotificationPrivateByOS::registerDelegate() {
    AppNotificationDelegate* appDelegate = [[AppNotificationDelegate alloc] init];

    [appDelegate setDelegateForUserNotificationCenter:[NSUserNotificationCenter defaultUserNotificationCenter]];
    tNotificationPrivateByOS::registeredAppDelegate = true;
}

void tNotification::post(bool deleteWhenDone) {
    NSUserNotification* notification = [NSUserNotification new];
    notification.title = sum.toNSString();
    notification.informativeText = txt.toNSString();

    if (!this->noSound) {
        notification.soundName = NSUserNotificationDefaultSoundName;
    }

    [[NSUserNotificationCenter defaultUserNotificationCenter] deliverNotification:notification];
    [notification autorelease];

    if (deleteWhenDone) {
        this->deleteLater();
    }
}

void tNotification::initialize() {
    dd = new tNotificationPrivateByOS();
    if (!dd->registeredAppDelegate) dd->registerDelegate();
}

void tNotification::destroy() {
    delete dd;
}

void tNotification::dismiss() {

}
