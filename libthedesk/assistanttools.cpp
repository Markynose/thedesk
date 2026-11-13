#include "assistanttools.h"

#include <QJsonArray>

AssistantTools* AssistantTools::instance() {
    static auto* tools = new AssistantTools();
    return tools;
}

void AssistantTools::add(const AssistantTool& tool) {
    remove(tool.name);
    list.append(tool);
    emit changed();
}

void AssistantTools::remove(const QString& name) {
    list.removeIf([&name](const AssistantTool& tool) {
        return tool.name == name;
    });
}

QList<AssistantTool> AssistantTools::tools() const {
    return list;
}

bool AssistantTools::has(const QString& name) const {
    return std::any_of(list.cbegin(), list.cend(), [&name](const AssistantTool& tool) {
        return tool.name == name;
    });
}

AssistantTool AssistantTools::tool(const QString& name) const {
    for (const AssistantTool& tool : list) {
        if (tool.name == name) return tool;
    }
    return {};
}

QJsonObject AssistantTools::schemaOf(const QJsonObject& properties, const QStringList& optional) {
    QJsonObject props;
    QJsonArray required;
    for (auto it = properties.begin(); it != properties.end(); ++it) {
        // "number", "string", "boolean", or "string: what it is"
        const QString spec = it.value().toString();
        const QString type = spec.section(':', 0, 0).trimmed();
        QJsonObject prop{{"type", type}};
        if (spec.contains(':')) prop.insert("description", spec.section(':', 1).trimmed());
        props.insert(it.key(), prop);
        if (!optional.contains(it.key())) required.append(it.key());
    }
    return QJsonObject{{"type", "object"}, {"properties", props}, {"required", required}};
}
