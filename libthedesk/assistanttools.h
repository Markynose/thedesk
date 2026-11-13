#ifndef ASSISTANTTOOLS_H
#define ASSISTANTTOOLS_H

#include "libthedesk_global.h"
#include <QJsonObject>
#include <QObject>
#include <functional>

// What one of theDesk's actions did, for the assistant and its panel
struct LIBTHEDESK_EXPORT AssistantToolResult {
        QString text; // what the assistant reads back

        // An optional card in the panel: "level" (a slider, value 0..1),
        // "switch" (value 0/1) or "image" (path is a file or a URL)
        QString card, icon, title, subtitle, path;
        double value = 0;
        std::function<void(double)> setValue; // the card's control, moved by hand
        std::function<void()> undo;           // puts things back as they were
};

// One action the assistant (Pebble) may take; plugins add theirs
struct LIBTHEDESK_EXPORT AssistantTool {
        QString name;        // snake_case, unique
        QString description; // for the assistant: what it does, when to use it
        QJsonObject schema;  // JSON schema of the arguments
        bool asks = false;   // ask the user first (it sends, deletes or closes something)
        std::function<AssistantToolResult(QJsonObject)> run;
};

class LIBTHEDESK_EXPORT AssistantTools : public QObject {
        Q_OBJECT
    public:
        static AssistantTools* instance();

        void add(const AssistantTool& tool);
        void remove(const QString& name);
        QList<AssistantTool> tools() const;
        bool has(const QString& name) const;
        AssistantTool tool(const QString& name) const;

        // A schema in one line: {"percent": "number"} -> an object schema, all required
        static QJsonObject schemaOf(const QJsonObject& properties, const QStringList& optional = {});

    signals:
        void changed();

    private:
        AssistantTools() = default;
        QList<AssistantTool> list;
};

#endif // ASSISTANTTOOLS_H
