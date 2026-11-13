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

#ifndef THELIBS_GLOBAL_H
#define THELIBS_GLOBAL_H

#include "tsysconfigdetect.h"
#include <QSettings>
#include <QtCore/qglobal.h>

#ifdef QT_WIDGETS_LIB
    #include <QApplication>
    #include <QStyle>
    #include <QStyleFactory>
#endif

#ifdef HAVE_QT_DBUS
    #include <QDBusConnection>
    #include <QDBusMessage>
    #include <QDBusReply>
#endif

#if defined(LIBCONTEMPORARY_LIBRARY)
    #define LIBCONTEMPORARY_EXPORT Q_DECL_EXPORT
#else
    #define LIBCONTEMPORARY_EXPORT Q_DECL_IMPORT
#endif

#define THE_LIBS_API_VERSION 9
#define THE_LIBS_VERSION "6.1"

#ifdef QT_WIDGETS_LIB
    #define SC_DPI(pixels) \
        static_cast<int>(pixels * libContemporaryCommon::getDPIScaling())
    #define SC_DPI_T(value, type) static_cast<type>(value * libContemporaryCommon::getDPIScaling())
    #define SC_DPI_W(pixels, widget) static_cast<int>(pixels * libContemporaryCommon::getDPIScaling(widget))
    #define SC_DPI_WT(value, type, widget) static_cast<type>(value * libContemporaryCommon::getDPIScaling(widget))
#endif

struct libContemporaryCommonPrivate;
class LIBCONTEMPORARY_EXPORT libContemporaryCommon : public QObject {
        Q_OBJECT

    public:
        /// Get the application instance of the class so you can connect to signals
        /// \return The application instance of libContemporaryCommon
        static libContemporaryCommon* instance();

#ifdef QT_WIDGETS_LIB
        /// Get the screen scaling ratio
        /// \deprecated Use the overload that takes a QPaintDevice instead
        /// \return The screen scaling ratio
        [[deprecated("getDPIScaling is deprecated, Qt now handles DPI scaling automatically")]] static double getDPIScaling();

        /// Get the screen scaling ratio
        /// \param paintDevice The device to get the scaling ratio for
        /// \return The screen scaling ratio
        /// \note Use the SC_DPI_W macro for a more convenient way to scale a number
        [[deprecated("getDPIScaling is deprecated, Qt now handles DPI scaling automatically")]] static double getDPIScaling(const QPaintDevice* paintDevice);

        /// Tint an icon
        /// \param image The icon to tint
        /// \param tint The colour to tint the icon with
        static void tintImage(QImage& image, QColor tint);

        /// Tint a pixmap
        /// \param pixmap The pixmap to tint
        /// \param tint The colour to tint the pixmap with
        /// \returns The tinted pixmap
        static QPixmap getTintedPixmap(QPixmap pixmap, QColor tint);

        /// Fixate the height of a widget when the DPI changes
        /// \param widget The widget to fixate the height of
        /// \param calculateHeight Function to calculate the desired height
        static void fixateHeight(QWidget* widget, std::function<int()> calculateHeight);
#endif

        /// Search for an executable in the system PATH environment variable
        /// \param executable Executable to search for
        /// \return List of executables that were found. If no executables were found, the list will be empty.
        static QStringList searchInPath(QString executable);

        /// Get the preferred colour for drawing lines
        /// \param textColor The contextual text colour
        /// \return The colour to draw lines in
        static QColor lineColor(QColor textColor);
        // theDesk: the app runs the Material 3 Expressive style (style-m3)
        static bool isMaterial3();

        static QString humanReadablePartJoinString();

    public Q_SLOTS:
        /// Get whether Power Stretch (or system equivalent) is enabled.
        /// \return True if Power Stretch is enabled
        bool powerStretchEnabled();

        /// Get whether animations should be used
        /// \return True if animations should be used
        bool allowSystemAnimations();

    private Q_SLOTS:
        void powerStretchChangedPrivate(bool isOn);
        void dbusPropertyChangedPrivate(QString interfaceName, QMap<QString, QVariant> changedProperties, QStringList invalidatedProperties);

    Q_SIGNALS:
        /// Emitted when Power Stretch (or system equivalent) is changed by the system
        /// \param isOn True if Power Stretch is enabled
        void powerStretchChanged(bool isOn);

    private:
        libContemporaryCommon();

        libContemporaryCommonPrivate* d;
};

#endif // THELIBS_GLOBAL_H
