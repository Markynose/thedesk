// Render theDesk's horizon scenes (libthedesk/horizon/horizonscene.cpp) to PNG.
//   horizon-wallpaper <outdir> [WIDTHxHEIGHT]
// Writes horizon-<scene>.png for every scene.
#include "horizonscene.h"

#include <QDir>
#include <QGuiApplication>
#include <QTextStream>

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    QTextStream out(stdout), err(stderr);

    if (argc < 2) {
        err << "usage: horizon-wallpaper <outdir> [WIDTHxHEIGHT]\n";
        return 2;
    }
    QDir dir(QString::fromLocal8Bit(argv[1]));
    dir.mkpath(".");

    QSize size(1920, 1080);
    if (argc > 2) {
        QStringList parts = QString::fromLocal8Bit(argv[2]).split('x');
        if (parts.size() == 2) size = QSize(parts[0].toInt(), parts[1].toInt());
    }
    if (size.isEmpty()) {
        err << "invalid size\n";
        return 2;
    }

    for (int i = 0; i < HorizonScene::SceneCount; i++) {
        QString path = dir.filePath(QStringLiteral("horizon-%1.png").arg(HorizonScene::sceneName(i)));
        if (!HorizonScene::render(i, size).save(path)) {
            err << "failed to write " << path << "\n";
            return 1;
        }
        out << path << "\n";
    }
    return 0;
}
