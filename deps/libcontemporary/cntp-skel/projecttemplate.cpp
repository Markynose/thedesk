#include "projecttemplate.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQueue>

struct ProjectTemplatePrivate {
        QDir templateRoot;
        QJsonObject definition;

        QString projectName;
        QMap<QString, QString> templateValues;
};

ProjectTemplate::ProjectTemplate(QString name, QObject* parent) :
    QObject{parent} {
    d = new ProjectTemplatePrivate();

    QDir skels(SKELS_PATH);
    d->templateRoot = skels.absoluteFilePath(name);

    QFile definitionFile(d->templateRoot.absoluteFilePath(".cntp.skel.json"));
    definitionFile.open(QFile::ReadOnly);
    d->definition = QJsonDocument::fromJson(definitionFile.readAll()).object();
    definitionFile.close();
}

ProjectTemplate::~ProjectTemplate() {
    delete d;
}

QStringList ProjectTemplate::templates() {
    QStringList templates;
    QDir skels(SKELS_PATH);
    for (QString directory : skels.entryList(QDir::Dirs)) {
        QDir templateDir(skels.absoluteFilePath(directory));
        if (templateDir.entryList(QDir::Files | QDir::Hidden).contains(".cntp.skel.json")) {
            templates.append(directory);
        }
    }
    return templates;
}

bool ProjectTemplate::templateExists(QString name) {
    return ProjectTemplate::templates().contains(name);
}

void ProjectTemplate::setProjectName(QString projectName) {
    d->projectName = projectName;
}

QStringList ProjectTemplate::templateParameters() {
    QStringList params = d->definition.value("params").toObject().keys();
    params.append(d->templateValues.keys());
    params.append({"LCPNAME", "UCPNAME",
        "PNAME", "YEAR"});
    return params;
}

QString ProjectTemplate::templateValue(QString parameter) {
    if (parameter == "PNAME") {
        return d->projectName;
    } else if (parameter == "LCPNAME") {
        return d->projectName.toLower();
    } else if (parameter == "UCPNAME") {
        return d->projectName.toUpper();
    } else if (parameter == "YEAR") {
        return QString::number(QDate::currentDate().year());
    } else {
        QString param;

        if (d->templateValues.contains(parameter)) {
            param = d->templateValues.value(parameter);
        } else {
            param = d->definition.value("params").toObject().value(parameter).toString();

            // Transform using template values
            for (QString templ : templateParameters()) {
                if (templ == parameter) continue;
                param.replace("$" + templ, templateValue(templ));
            }
        }

        return param;
    }
}

void ProjectTemplate::setTemplateValue(QString parameter, QString value) {
    d->templateValues.insert(parameter, value);
}

QStringList ProjectTemplate::inputParameters() {
    return d->definition.value("inputparams").toObject().keys();
}

QString ProjectTemplate::inputParameterPrompt(QString parameter) {
    return d->definition.value("inputparams").toObject().value(parameter).toObject().value("prompt").toString();
}

QString ProjectTemplate::inputParameterReadableName(QString parameter) {
    return d->definition.value("inputparams").toObject().value(parameter).toObject().value("readableName").toString();
}

void ProjectTemplate::create(QString path) {
    QDir rootPath(path);
    rootPath.mkpath(".");

    QQueue<QPair<QStringList, QDir>> dirsToCopy;
    QList<QPair<QString, QString>> filesToCopy;

    for (QFileInfo item : d->templateRoot.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden)) {
        if (item.fileName() == ".cntp.skel.json") continue;
        if (item.isDir()) {
            dirsToCopy.enqueue({{transformPathPart(item.fileName())}, QDir(item.filePath())});
        } else {
            filesToCopy.append({item.absoluteFilePath(), rootPath.absoluteFilePath(transformPathPart(item.fileName()))});
        }
    }

    while (!dirsToCopy.isEmpty()) {
        QPair<QStringList, QDir> part = dirsToCopy.dequeue();
        QTextStream(stdout) << "Writing directory " << part.second.absolutePath() << " to " << part.first.join("/") << "\n";

        QString destDirName = transformPathPart(part.second.dirName());
        QDir dir(rootPath.absoluteFilePath(part.first.join("/")));
        dir.mkpath(".");

        for (QFileInfo item : part.second.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (item.isDir()) {
                QStringList pathParts = part.first;
                pathParts.append(transformPathPart(item.fileName()));
                dirsToCopy.enqueue({pathParts, QDir(item.filePath())});
            } else {
                filesToCopy.append({item.absoluteFilePath(), dir.absoluteFilePath(transformPathPart(item.fileName()))});
            }
        }
    }

    for (QPair<QString, QString> file : filesToCopy) {
        QTextStream(stdout) << "Writing file " << file.first << " to " << file.second << "\n";

        QFile inputFile(file.first);
        inputFile.open(QFile::ReadOnly);
        QString output = inputFile.readAll();
        inputFile.close();

        for (QString templ : templateParameters()) {
            output = output.replace(QStringLiteral("$($%1)").arg(templ), templateValue(templ));
        }

        QFile outputFile(file.second);
        outputFile.open(QFile::WriteOnly);
        outputFile.write(output.toUtf8());
        outputFile.close();
    }
}

QString ProjectTemplate::transformPathPart(QString pathPart) {
    for (QString templ : templateParameters()) {
        pathPart = pathPart.replace(templ, templateValue(templ));
    }
    return pathPart;
}
