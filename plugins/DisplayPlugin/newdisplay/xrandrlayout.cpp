/****************************************
 *
 *   theDesk - laying out displays, the way xrandr does
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "xrandrlayout.h"

#include <QCryptographicHash>
#include <QGuiApplication>
#include <QHash>
#include <QPointF>
#include <QRect>
#include <algorithm>
#include <tlogger.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

namespace {
    Display* display() {
        auto* x11 = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QX11Application>() : nullptr;
        return x11 ? x11->display() : nullptr;
    }

    // RandR state read once (no probe: X has already seen the plug)
    struct Snapshot {
            Display* dpy = nullptr;
            Window root = 0;
            XRRScreenResources* res = nullptr;

            explicit Snapshot(Display* dpy) :
                dpy(dpy) {
                if (!dpy) return;
                root = DefaultRootWindow(dpy);
                res = XRRGetScreenResourcesCurrent(dpy, root);
            }

            ~Snapshot() {
                if (res) XRRFreeScreenResources(res);
            }

            const XRRModeInfo* mode(RRMode id) const {
                for (int i = 0; i < res->nmode; i++) {
                    if (res->modes[i].id == id) return &res->modes[i];
                }
                return nullptr;
            }
    };

    QByteArray edidOf(Display* dpy, RROutput output) {
        Atom edid = XInternAtom(dpy, "EDID", True);
        if (edid == None) return {};
        Atom type;
        int format;
        unsigned long count, after;
        unsigned char* data = nullptr;
        QByteArray bytes;
        if (XRRGetOutputProperty(dpy, output, edid, 0, 256, False, False, AnyPropertyType, &type, &format, &count, &after, &data) == Success && data) {
            bytes = QByteArray(reinterpret_cast<char*>(data), int(count));
            XFree(data);
        }
        return bytes;
    }

    bool isBuiltIn(const QString& name) {
        return name.startsWith(QStringLiteral("eDP")) || name.startsWith(QStringLiteral("LVDS")) || name.startsWith(QStringLiteral("DSI"));
    }

    void setTransform(Display* dpy, RRCrtc crtc, double sx, double sy) {
        XTransform transform = {};
        transform.matrix[0][0] = XDoubleToFixed(sx);
        transform.matrix[1][1] = XDoubleToFixed(sy);
        transform.matrix[2][2] = XDoubleToFixed(1);
        const bool scaled = sx != 1 || sy != 1;
        char bilinear[] = "bilinear";
        char nearest[] = "nearest";
        XRRSetCrtcTransform(dpy, crtc, &transform, scaled ? bilinear : nearest, nullptr, 0);
    }
} // namespace

bool XRandrLayout::available() {
    Display* dpy = display();
    if (!dpy) return false;
    int major = 0, minor = 0;
    return XRRQueryVersion(dpy, &major, &minor) && (major > 1 || (major == 1 && minor >= 3));
}

QList<XRandrLayout::Output> XRandrLayout::outputs() {
    QList<Output> list;
    Snapshot s(display());
    if (!s.res) return list;
    for (int i = 0; i < s.res->noutput; i++) {
        XRROutputInfo* info = XRRGetOutputInfo(s.dpy, s.res, s.res->outputs[i]);
        if (!info) continue;
        Output output;
        output.name = QString::fromLocal8Bit(info->name, info->nameLen);
        output.connected = info->connection == RR_Connected;
        output.on = info->crtc != 0;
        if (info->nmode > 0) {
            if (const XRRModeInfo* mode = s.mode(info->modes[0])) output.preferred = QSize(int(mode->width), int(mode->height));
        }
        if (output.connected) output.edidHash = QString::fromLatin1(QCryptographicHash::hash(edidOf(s.dpy, s.res->outputs[i]), QCryptographicHash::Sha1).toHex().left(16));
        XRRFreeOutputInfo(info);
        list.append(output);
    }
    return list;
}

QString XRandrLayout::mainOutput() {
    const QList<Output> all = outputs();
    for (const Output& output : all) {
        if (output.connected && isBuiltIn(output.name)) return output.name;
    }
    Display* dpy = display();
    if (dpy) {
        Snapshot s(dpy);
        RROutput primary = XRRGetOutputPrimary(dpy, s.root);
        for (int i = 0; s.res && i < s.res->noutput; i++) {
            if (s.res->outputs[i] != primary) continue;
            XRROutputInfo* info = XRRGetOutputInfo(dpy, s.res, primary);
            const QString name = QString::fromLocal8Bit(info->name, info->nameLen);
            const bool connected = info->connection == RR_Connected;
            XRRFreeOutputInfo(info);
            if (connected) return name;
        }
    }
    for (const Output& output : all) {
        if (output.connected) return output.name;
    }
    return {};
}

QString XRandrLayout::arrangementKey() {
    QStringList ids;
    for (const Output& output : outputs()) {
        if (output.connected) ids.append(output.edidHash.isEmpty() ? output.name : output.edidHash);
    }
    std::sort(ids.begin(), ids.end());
    return ids.join(QLatin1Char('-'));
}

bool XRandrLayout::apply(Layout layout) {
    Display* dpy = display();
    if (!dpy) return false;
    Snapshot s(dpy);
    if (!s.res) return false;
    const QString mainName = mainOutput();

    struct Plan {
            RROutput output;
            QString name;
            XRROutputInfo* info;
            bool on;
            RRMode mode = None;
            QSize modeSize;
            QRect area; // in the X screen (after scaling)
            RRCrtc crtc = None;
    };
    QList<Plan> plans;
    for (int i = 0; i < s.res->noutput; i++) {
        XRROutputInfo* info = XRRGetOutputInfo(dpy, s.res, s.res->outputs[i]);
        if (!info) continue;
        if (info->connection != RR_Connected || info->nmode == 0) {
            XRRFreeOutputInfo(info); // not connected: its CRTC is switched off below
            continue;
        }
        Plan plan{s.res->outputs[i], QString::fromLocal8Bit(info->name, info->nameLen), info, true};
        const bool isMain = plan.name == mainName;
        plan.on = layout == OnlyMain ? isMain : layout == OnlyOthers ? !isMain : true;

        // The main screen keeps the mode it has (else its preferred); the
        // others take their preferred one
        RRMode mode = info->modes[0];
        if (isMain && info->crtc) {
            XRRCrtcInfo* crtc = XRRGetCrtcInfo(dpy, s.res, info->crtc);
            if (crtc) {
                if (crtc->mode) mode = crtc->mode;
                XRRFreeCrtcInfo(crtc);
            }
        }
        plan.mode = mode;
        if (const XRRModeInfo* m = s.mode(mode)) plan.modeSize = QSize(int(m->width), int(m->height));
        plans.append(plan);
    }
    auto mainPlan = std::find_if(plans.begin(), plans.end(), [&](const Plan& p) {
        return p.name == mainName;
    });
    if (mainPlan == plans.end()) {
        for (Plan& p : plans) XRRFreeOutputInfo(p.info);
        return false;
    }

    // Where each one goes
    const QSize mainSize = mainPlan->modeSize;
    int right = 0;
    if (layout != OnlyOthers) {
        mainPlan->area = QRect(QPoint(0, 0), mainSize);
        right = mainSize.width();
    }
    for (Plan& p : plans) {
        if (p.name == mainName || !p.on) continue;
        if (layout == Mirror) {
            p.area = QRect(QPoint(0, 0), mainSize); // scaled to show the main screen
        } else {
            p.area = QRect(QPoint(right, 0), p.modeSize);
            right += p.modeSize.width();
        }
    }
    QRect bounds;
    for (const Plan& p : plans) {
        if (p.on) bounds = bounds.united(p.area);
    }
    if (bounds.isEmpty()) {
        for (Plan& p : plans) XRRFreeOutputInfo(p.info);
        return false;
    }

    // A CRTC for each display that's on (its own one first)
    QList<RRCrtc> used;
    bool ok = true;
    for (Plan& p : plans) {
        if (!p.on) continue;
        QList<RRCrtc> candidates;
        if (p.info->crtc) candidates.append(p.info->crtc);
        for (int i = 0; i < p.info->ncrtc; i++) candidates.append(p.info->crtcs[i]);
        for (RRCrtc candidate : std::as_const(candidates)) {
            if (!used.contains(candidate)) {
                p.crtc = candidate;
                used.append(candidate);
                break;
            }
        }
        if (p.crtc == None) ok = false;
    }
    auto scaleOf = [](const Plan& p) {
        return QPointF(p.modeSize.width() > 0 ? double(p.area.width()) / p.modeSize.width() : 1, p.modeSize.height() > 0 ? double(p.area.height()) / p.modeSize.height() : 1);
    };
    // Already as planned (same output, mode, place, scale): left alone, so
    // the panel doesn't blank for nothing
    auto unchanged = [&](const Plan& p) {
        if (p.crtc == None) return false;
        XRRCrtcInfo* info = XRRGetCrtcInfo(dpy, s.res, p.crtc);
        if (!info) return false;
        bool same = info->mode == p.mode && info->x == p.area.x() && info->y == p.area.y() && info->noutput == 1 && info->outputs[0] == p.output && info->rotation == RR_Rotate_0;
        XRRFreeCrtcInfo(info);
        if (!same) return false;
        XRRCrtcTransformAttributes* attributes = nullptr;
        if (XRRGetCrtcTransform(dpy, p.crtc, &attributes) && attributes) {
            const QPointF scale = scaleOf(p);
            same = qAbs(XFixedToDouble(attributes->currentTransform.matrix[0][0]) - scale.x()) < 0.001 && qAbs(XFixedToDouble(attributes->currentTransform.matrix[1][1]) - scale.y()) < 0.001;
            XFree(attributes);
        }
        return same;
    };
    QList<RRCrtc> keep;
    for (const Plan& p : plans) {
        if (p.on && unchanged(p)) keep.append(p.crtc);
    }

    XGrabServer(dpy);

    // As xrandr does: switch off every CRTC that changes or isn't wanted
    // (including any left lit for a display that's gone), size the screen,
    // then switch them on as planned
    for (int i = 0; i < s.res->ncrtc; i++) {
        const RRCrtc crtc = s.res->crtcs[i];
        if (keep.contains(crtc)) continue;
        XRRCrtcInfo* info = XRRGetCrtcInfo(dpy, s.res, crtc);
        const bool active = info && info->mode != None;
        if (info) XRRFreeCrtcInfo(info);
        if (active) XRRSetCrtcConfig(dpy, s.res, crtc, CurrentTime, 0, 0, None, RR_Rotate_0, nullptr, 0);
    }
    const int dpi = 96;
    XRRSetScreenSize(dpy, s.root, bounds.width(), bounds.height(), qMax(1, qRound(25.4 * bounds.width() / dpi)), qMax(1, qRound(25.4 * bounds.height() / dpi)));

    for (Plan& p : plans) {
        if (!p.on || p.crtc == None || keep.contains(p.crtc)) continue;
        const QPointF scale = scaleOf(p);
        setTransform(dpy, p.crtc, scale.x(), scale.y());
        RROutput output = p.output;
        if (XRRSetCrtcConfig(dpy, s.res, p.crtc, CurrentTime, p.area.x(), p.area.y(), p.mode, RR_Rotate_0, &output, 1) != RRSetConfigSuccess) ok = false;
    }

    // The primary: the main screen, unless it's off
    for (const Plan& p : plans) {
        if (p.on && (p.name == mainName || layout == OnlyOthers)) {
            XRRSetOutputPrimary(dpy, s.root, p.output);
            break;
        }
    }

    XUngrabServer(dpy);
    XSync(dpy, False);
    for (Plan& p : plans) XRRFreeOutputInfo(p.info);
    tDebug("XRandrLayout") << "applied layout" << int(layout) << QStringLiteral("screen %1x%2").arg(bounds.width()).arg(bounds.height()) << (ok ? "ok" : "with errors");
    return ok;
}
