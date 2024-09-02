/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Adam Celarek
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QQmlProperty>

class ModelBinding : public QObject, public QQmlParserStatus, public QQmlPropertyValueSource {
    Q_OBJECT
    Q_INTERFACES(QQmlPropertyValueSource)
    QML_ELEMENT
    Q_PROPERTY(QObject* target READ model WRITE set_model NOTIFY model_changed FINAL)
    Q_PROPERTY(QString property READ property WRITE set_property NOTIFY property_changed FINAL)
    Q_PROPERTY(bool receive_model_updates READ receive_model_updates WRITE set_receive_model_updates NOTIFY receive_model_updates_changed FINAL)
    Q_PROPERTY(QVariant default_value READ default_value WRITE set_default_value NOTIFY default_value_changed FINAL)
public:
    explicit ModelBinding(QObject* parent = nullptr);

    // QQmlParserStatus interface
    void classBegin() override;
    void componentComplete() override;

    // QQmlPropertyValueSource interface
    void setTarget(const QQmlProperty&) override;

    QObject* model() const;
    void set_model(QObject* new_model);

    const QString& property() const;
    void set_property(const QString& new_property);

    bool receive_model_updates() const;
    void set_receive_model_updates(bool new_receive_model_updates);

    const QVariant& default_value() const;
    void set_default_value(const QVariant& new_default_value);

private slots:
    void write_qml();
    void write_model();

private:
    QVariant read_model() const;

signals:
    void model_changed(QObject* model);

    void property_changed(const QString& property);

    void receive_model_updates_changed(bool receive_model_updates);

    void default_value_changed(const QVariant& default_value);

private:
    std::function<QVariant(const QVariant&)> m_read_fun;
    std::function<QVariant(const QVariant&, const QVariant&)> m_write_fun;
    QQmlProperty m_qml_target;
    QMetaProperty m_gadget_meta_property;
    QMetaProperty m_meta_property;
    QObject* m_model = nullptr;
    QString m_property;
    QVariant m_default_value;
    bool m_complete = false;
    bool m_receive_model_updates = false;
};
