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

#ifndef NATIVEEVENTFILTER_H
#define NATIVEEVENTFILTER_H

#include "libcontemporary_global.h"
#include <QAbstractNativeEventFilter>
#include <QObject>

namespace libContemporaryPrivate {
    class NativeEventFilter : public QObject,
                              public QAbstractNativeEventFilter {
            Q_OBJECT
        public:
            explicit NativeEventFilter(QObject* parent = nullptr);

            static NativeEventFilter* instance(QObject* parentIfNotCreated = nullptr);
        signals:
            void powerStretchChanged(bool powerStretchOn);
            void thumbnailToolbarButtonClicked(int buttonId);
            void thumbnailCreated(quintptr hwnd);

        public slots:

        private:
            bool filter(const QByteArray& eventType, void* message);

            // QAbstractNativeEventFilter interface
        public:
#if defined(T_QT_5)
            bool nativeEventFilter(const QByteArray& eventType, void* message, long* result);
#elif defined(T_QT_6)
            bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result);
#endif
    };
} // namespace libContemporaryPrivate

#endif // NATIVEEVENTFILTER_H
