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

tNotification::tNotification(QString summary, QString text, QObject *parent) : QObject(parent)
{
    this->sum = summary;
    this->txt = text;

    //Call the operating system specific handler
    initialize();
}

tNotification::~tNotification() {
    //Call the operating system specific handler
    destroy();
}

void tNotification::setTransient(bool transient) {
    this->isTransient = transient;
}

bool tNotification::transient() {
    return isTransient;
}

void tNotification::setTimeout(int timeout) {
    this->time = timeout;
}

int tNotification::timeout() {
    return this->time;
}

void tNotification::setSummary(QString summary) {
    this->sum = summary;
}

QString tNotification::summary() {
    return this->sum;
}

void tNotification::setText(QString text) {
    this->txt = text;
}

QString tNotification::text() {
    return this->txt;
}

void tNotification::setCategory(QString category) {
    this->cat = category;
}

QString tNotification::category() {
    return this->cat;
}

void tNotification::setSound(QString sound) {
    this->snd = sound;
}

QString tNotification::sound() {
    return this->snd;
}

void tNotification::setSoundOn(bool soundOn) {
    this->noSound = !soundOn;
}

bool tNotification::soundOn() {
    return !this->noSound;
}

void tNotification::setUrgency(Urgency urgency) {
    this->urg = urgency;
}

tNotification::Urgency tNotification::urgency() {
    return this->urg;
}

void tNotification::setAppName(QString appName) {
    this->app = appName;
}

QString tNotification::appName() {
    return this->app;
}

void tNotification::insertHint(QString key, QVariant hint) {
    extraHints.insert(key, hint);
}

void tNotification::setAppIcon(QString appIcon) {
    this->aIcon = appIcon;
}

QString tNotification::appIcon() {
    return this->aIcon;
}

void tNotification::insertAction(QString key, QString text) {
    this->actions.insert(key, text);
}

void tNotification::removeAction(QString key) {
    this->actions.remove(key);
}

void tNotification::clearActions() {
    this->actions.clear();
}
