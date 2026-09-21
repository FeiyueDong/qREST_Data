#ifndef FIELD_HELP_REGISTRY_H
#define FIELD_HELP_REGISTRY_H

#include <QJsonObject>
#include <QObject>
#include <QString>

class FieldHelpRegistry : public QObject {
    Q_OBJECT

public:
    explicit FieldHelpRegistry(QObject *parent = nullptr);

    Q_INVOKABLE QString label(const QString &fieldKey,
                              const QString &fallback = QString()) const;
    Q_INVOKABLE QString helpText(const QString &fieldKey) const;
    Q_INVOKABLE bool hasHelp(const QString &fieldKey) const;

private:
    [[nodiscard]] QJsonObject fieldObject(const QString &fieldKey) const;

    QJsonObject m_fields;
};

#endif // FIELD_HELP_REGISTRY_H
