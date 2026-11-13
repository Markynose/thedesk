#ifndef PROJECTTEMPLATE_H
#define PROJECTTEMPLATE_H

#include <QObject>

struct ProjectTemplatePrivate;
class ProjectTemplate : public QObject {
        Q_OBJECT
    public:
        explicit ProjectTemplate(QString name, QObject* parent = nullptr);
        ~ProjectTemplate();

        static QStringList templates();
        static bool templateExists(QString name);

        void setProjectName(QString projectName);

        QStringList templateParameters();
        QString templateValue(QString parameter);
        void setTemplateValue(QString parameter, QString value);

        QStringList inputParameters();
        QString inputParameterPrompt(QString parameter);
        QString inputParameterReadableName(QString parameter);

        void create(QString path);

    signals:

    private:
        ProjectTemplatePrivate* d;

        QString transformPathPart(QString pathPart);
};

#endif // PROJECTTEMPLATE_H
