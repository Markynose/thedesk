/****************************************
 *
 *   theDesk - the desktop's sounds
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "desksound.h"

#include "quietmodemanager.h"
#include "statemanager.h"
#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QAudioSink>
#include <QBuffer>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QMediaDevices>
#include <QPointer>
#include <QStandardPaths>
#include <QUrl>
#include <tlogger.h>
#include <tsettings.h>

namespace {
    QAudioFormat pcmFormat() {
        QAudioFormat format;
        format.setSampleRate(48000);
        format.setChannelCount(2);
        format.setSampleFormat(QAudioFormat::Int16);
        return format;
    }

    // A sound decoded once, kept for every later play
    struct Decoded {
            QByteArray pcm;
            bool ready = false;
            bool failed = false;
            QList<QPointer<DeskSound>> waiting;
    };

    QHash<QString, Decoded*> cache; // by file path

    tSettings* settings() {
        static tSettings* s = [] {
            auto* s = new tSettings();
            QObject::connect(s, &tSettings::settingChanged, [](QString key, QVariant) {
                if (key == QStringLiteral("Sounds/theme")) cache.clear(); // files may differ now
            });
            return s;
        }();
        return s;
    }

    // A theme's folders by its folder or its index.theme's Name
    QStringList themeDirs(const QString& theme) {
        QStringList dirs;
        const QStringList roots = {QDir::homePath() + QStringLiteral("/.local/share/sounds"), QStringLiteral("/usr/share/sounds")};
        for (const QString& root : roots) {
            QDir dir(root + QLatin1Char('/') + theme);
            if (dir.exists()) dirs.append(dir.absolutePath());
        }
        return dirs;
    }

    QString findIn(const QString& dir, const QString& name) {
        static const QStringList suffixes = {QStringLiteral("ogg"), QStringLiteral("oga"), QStringLiteral("wav"), QStringLiteral("flac"), QStringLiteral("mp3")};
        QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QFileInfo info(it.next());
            if (info.completeBaseName() == name && suffixes.contains(info.suffix().toLower())) return info.absoluteFilePath();
        }
        return {};
    }
} // namespace

struct DeskSoundPrivate {
        QString path;
        DeskSound::Kind kind;
        QAudioSink* sink = nullptr;
        QBuffer* buffer = nullptr;
        bool stopped = false;
};

DeskSound::DeskSound(QObject* parent) :
    QObject(parent) {
    d = new DeskSoundPrivate();
}

DeskSound::~DeskSound() {
    delete d;
}

QString DeskSound::location(const QString& name) {
    const QString theme = settings()->value(QStringLiteral("Sounds/theme")).toString();
    QStringList dirs;
    if (!theme.isEmpty()) dirs.append(themeDirs(theme));
    dirs.append(themeDirs(QStringLiteral("freedesktop")));

    // "battery-caution" falls back to "battery", as the spec has it, but
    // only after every theme was asked for the full name
    QString wanted = name;
    while (!wanted.isEmpty()) {
        for (const QString& dir : std::as_const(dirs)) {
            const QString found = findIn(dir, wanted);
            if (!found.isEmpty()) return found;
        }
        const qsizetype dash = wanted.lastIndexOf(QLatin1Char('-'));
        if (dash < 0) break;
        wanted.truncate(dash);
    }
    return {};
}

static Decoded* decode(const QString& path) {
    if (Decoded* known = cache.value(path)) return known;

    auto* decoded = new Decoded();
    cache.insert(path, decoded);

    auto* decoder = new QAudioDecoder();
    decoder->setAudioFormat(pcmFormat());
    decoder->setSource(QUrl::fromLocalFile(path));
    QObject::connect(decoder, &QAudioDecoder::bufferReady, decoder, [decoder, decoded] {
        const QAudioBuffer buffer = decoder->read();
        decoded->pcm.append(buffer.constData<char>(), buffer.byteCount());
    });
    auto done = [decoder, decoded](bool ok) {
        decoded->ready = ok;
        decoded->failed = !ok;
        const auto waiting = decoded->waiting;
        decoded->waiting.clear();
        for (const QPointer<DeskSound>& sound : waiting) {
            if (!sound) continue;
            if (ok) QMetaObject::invokeMethod(sound, "start");
            else sound->stop();
        }
        decoder->deleteLater();
    };
    QObject::connect(decoder, &QAudioDecoder::finished, decoder, [done] {
        done(true);
    });
    QObject::connect(decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), decoder, [decoder, path, done](QAudioDecoder::Error) {
        tWarn("DeskSound") << "Can't decode" << path << decoder->errorString();
        done(false);
    });
    decoder->start();
    return decoded;
}

bool DeskSound::allowed(Kind kind) {
    if (!settings()->value(QStringLiteral("Sounds/enabled")).toBool()) return false;
    switch (StateManager::quietModeManager()->currentMode()) {
        case QuietModeManagerTd::Mute:
            return false;
        case QuietModeManagerTd::NoNotifications:
            return kind == System;
        case QuietModeManagerTd::CriticalOnly:
            return kind != Notification;
        default:
            return true;
    }
}

DeskSound* DeskSound::play(const QString& name, Kind kind) {
    const QVariant own = settings()->value(QStringLiteral("Sounds/") + name);
    if (own.isValid() && !own.toBool()) return nullptr;
    if (!allowed(kind)) return nullptr;

    const QString path = location(name);
    if (path.isEmpty()) return nullptr;
    return playPath(path, kind);
}

DeskSound* DeskSound::playFile(const QString& path, Kind kind) {
    if (!allowed(kind) || !QFileInfo::exists(path)) return nullptr;
    return playPath(path, kind);
}

DeskSound* DeskSound::playPath(const QString& path, Kind kind) {
    auto* sound = new DeskSound();
    sound->d->path = path;
    sound->d->kind = kind;

    Decoded* decoded = decode(path);
    if (decoded->failed) {
        sound->deleteLater();
        return nullptr;
    }
    if (decoded->ready) {
        sound->start();
    } else {
        decoded->waiting.append(sound);
    }
    return sound;
}

void DeskSound::preload(const QStringList& names) {
    for (const QString& name : names) {
        const QString path = location(name);
        if (!path.isEmpty()) decode(path);
    }
}

void DeskSound::start() {
    if (d->stopped) return;
    Decoded* decoded = cache.value(d->path);
    if (!decoded || !decoded->ready || decoded->pcm.isEmpty()) {
        stop();
        return;
    }

    d->buffer = new QBuffer(this);
    d->buffer->setData(decoded->pcm);
    d->buffer->open(QIODevice::ReadOnly);

    const double volume = settings()->value(QStringLiteral("Sounds/volume")).toDouble();
    d->sink = new QAudioSink(QMediaDevices::defaultAudioOutput(), pcmFormat(), this);
    d->sink->setVolume(volume > 0 ? qMin(volume, 1.0) : 1.0);
    connect(d->sink, &QAudioSink::stateChanged, this, [this](QAudio::State state) {
        if (state != QAudio::IdleState || d->stopped) return;
        if (d->kind == Ringtone) {
            // Round again, until the call is dealt with
            d->buffer->seek(0);
            d->sink->start(d->buffer);
            return;
        }
        stop();
    });
    d->sink->start(d->buffer);
}

void DeskSound::stop() {
    if (d->stopped) return;
    d->stopped = true;
    if (d->sink) d->sink->stop();
    emit finished();
    this->deleteLater();
}
