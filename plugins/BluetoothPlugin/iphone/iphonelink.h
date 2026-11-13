/****************************************
 *
 *   theDesk - an iPhone's notifications, over Bluetooth Low Energy
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef IPHONELINK_H
#define IPHONELINK_H

#include <BluezQt/Manager>
#include <QObject>

struct IPhoneLinkPrivate;

/**
 * An iPhone shares its notifications with any Bluetooth LE accessory it's
 * paired with, the way it does with a watch (Apple Notification Center
 * Service, ANCS). No app on the phone.
 *
 * Linking: startSetup() makes this computer visible for a few minutes and
 * advertises that it wants the phone's notifications; on the iPhone,
 * Settings > Bluetooth shows it; tapping it pairs (the code is confirmed on
 * both sides, through the plugin's agent). From then on the phone connects
 * by itself when it's near (this computer keeps a quiet advertisement up
 * while a linked phone is away).
 *
 * Its notifications show as theDesk's, with the phone's own actions (answer
 * or decline a call, mark as read...), and go away when they're dealt with
 * on the phone.
 */
class IPhoneLink : public QObject {
        Q_OBJECT
    public:
        explicit IPhoneLink(BluezQt::ManagerPtr manager, QObject* parent = nullptr);
        ~IPhoneLink() override;

        static IPhoneLink* instance();

        void startSetup();
        bool isLinked(const QString& address) const;
        // The phone's battery, from its own battery service (exact; BlueZ's
        // general reading can be stale or coarse); -1 when unknown
        int battery(const QString& address) const;
        // Whether a linked iPhone may play its sound here (off: it stays a
        // notification source, not a speaker)
        static bool audioAllowed();
        // Bluetooth profile UUIDs that carry sound
        static bool isAudioProfile(const QString& uuid);

    signals:
        void linkedChanged();
        void batteryChanged(QString address);

    private:
        IPhoneLinkPrivate* d;

        void watch(BluezQt::DevicePtr device);
        void look(BluezQt::DevicePtr device);
        void updateAdvertising();
        void keepSoundOnPhone(BluezQt::DevicePtr device);
        void advertise(bool on);
};

#endif // IPHONELINK_H
