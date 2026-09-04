#include "field_help_registry.h"

#include <QFile>
#include <QJsonDocument>
#include <QStringList>

namespace {

QString stringField(const QJsonObject &object, const char *key) {
    const QJsonValue value = object.value(QString::fromLatin1(key));
    return value.isString() ? value.toString() : QString();
}

void appendIfPresent(QStringList &lines,
                     const QString &prefix,
                     const QJsonObject &field,
                     const char *key) {
    const QString value = stringField(field, key);
    if (!value.isEmpty()) {
        lines.append(prefix + value);
    }
}

} // namespace

FieldHelpRegistry::FieldHelpRegistry(QObject *parent) : QObject(parent) {
    QFile file(":/qt/qml/qrest_data_tools_gui/doc/Description.json");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return;
    }

    m_fields = document.object().value("fields").toObject();
}

QString FieldHelpRegistry::label(const QString &fieldKey,
                                 const QString &fallback) const {
    const QString configured = stringField(fieldObject(fieldKey), "label");
    if (!configured.isEmpty()) {
        return configured;
    }
    return fallback.isEmpty() ? fieldKey : fallback;
}

QString FieldHelpRegistry::helpText(const QString &fieldKey) const {
    const QJsonObject field = fieldObject(fieldKey);
    QStringList lines;

    const QString tip = stringField(field, "tip");
    if (!tip.isEmpty()) {
        lines.append(tip);
    } else {
        const QString summary = stringField(field, "summary");
        if (!summary.isEmpty()) {
            lines.append(summary);
        }
    }

    appendIfPresent(lines, "Details: ", field, "details");
    appendIfPresent(lines, "Range: ", field, "range");
    appendIfPresent(lines, "Example: ", field, "example");

    const QString unit = stringField(field, "unit");
    if (!unit.isEmpty()) {
        lines.append("Unit: " + unit);
    }

    return lines.join("\n");
}

bool FieldHelpRegistry::hasHelp(const QString &fieldKey) const {
    return !helpText(fieldKey).isEmpty();
}

QJsonObject FieldHelpRegistry::fieldObject(const QString &fieldKey) const {
    return m_fields.value(fieldKey).toObject();
}
