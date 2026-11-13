/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2020 Victor Tran
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * *************************************/
#include "plugin.h"

#include "claude/claudechunk.h"
#include <claude/claudeusage.h>

#include "OverviewPane/overviewpane.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QIcon>
#include <barmanager.h>
#include <clockchunk.h>
#include <icontextchunk.h>
#include <keygrab.h>
#include <localemanager.h>
#include <statemanager.h>
#include <statuscentermanager.h>
#include <tapplication.h>
#include <tsettings.h>

struct PluginPrivate {
        ClaudeChunk* claude = nullptr;
        ClockChunk* chunk;
        OverviewPane* overviewPane;
};

Plugin::Plugin() {
    d = new PluginPrivate();
}

Plugin::~Plugin() {
    delete d;
}

void Plugin::activate() {
    tApplication::addPluginTranslator(CNTP_TARGET_NAME);

    tSettings::registerDefaults("theSuite", "the24", "/usr/share/defaults/the24.conf");

    d->chunk = new ClockChunk();
    StateManager::barManager()->addChunk(d->chunk);

    d->overviewPane = new OverviewPane();
    StateManager::statusCenterManager()->addPane(d->overviewPane);

    // The clock in the bar and Super+O open the Overview
    connect(d->chunk, &Chunk::clicked, this, [] {
        StateManager::statusCenterManager()->showWithPane(QStringLiteral("OverviewPane"));
    });
    connect(new KeyGrab(QKeySequence(Qt::MetaModifier | Qt::Key_O), "overview"), &KeyGrab::activated, this, [] {
        StateManager::statusCenterManager()->showWithPane(QStringLiteral("OverviewPane"));
    });

    // Claude's limits in the bar, while the Claude Code mod writes them
    ClaudeUsage::instance()->setAnnounces(true);
    d->claude = new ClaudeChunk();
    d->claude->follow();
}

void Plugin::deactivate() {
    if (d->claude) {
        if (d->claude->chunkRegistered()) StateManager::barManager()->removeChunk(d->claude);
        d->claude->deleteLater();
    }
    StateManager::statusCenterManager()->removePane(d->overviewPane);
    StateManager::barManager()->removeChunk(d->chunk);

    d->overviewPane->deleteLater();
    d->chunk->deleteLater();
    tApplication::removePluginTranslator(CNTP_TARGET_NAME);
}
