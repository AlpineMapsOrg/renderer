/*****************************************************************************
 * Alpine Terrain Renderer
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

#include "ModelBinding.h"

#include <QQmlProperty>

ModelBinding::ModelBinding(QObject* parent)
    : QObject { parent }
{
}

void ModelBinding::classBegin() { }

void ModelBinding::componentComplete()
{
    if (!m_qml_target.isValid()) {
        qWarning() << "m_qml_target invalid!";
        return;
    }

    const auto* this_meta = this->metaObject();

    const auto write_model_slot = this_meta->indexOfSlot("write_model()");
    connect(m_qml_target.object(), m_qml_target.property().notifySignal(), this, this_meta->method(write_model_slot));

    const auto* model_meta = m_model->metaObject();
    const auto model_property_index = model_meta->indexOfProperty(m_property.toStdString().c_str());
    if (model_property_index < 0) {
        qWarning() << "Property " << m_property << " of object " << m_model << " not found!";
        return;
    }
    m_model_meta_property = m_model->metaObject()->property(model_property_index);
    if (m_receive_model_updates) {
        if (!m_model_meta_property.hasNotifySignal()) {
            qWarning() << "Property " << m_property << " of object " << m_model << " has no notification signal!";
            return;
        }

        const auto qml_write_slot = this_meta->indexOfSlot("write_qml()");
        connect(m_model, m_model_meta_property.notifySignal(), this, this_meta->method(qml_write_slot));
    }

    if (m_default_value.isValid()) {
        m_qml_target.write(m_default_value);
    } else {
        write_qml();
    }
    m_complete = true;
}

void ModelBinding::setTarget(const QQmlProperty& property) { m_qml_target = property; }

QObject* ModelBinding::model() const { return m_model; }

void ModelBinding::set_model(QObject* new_model)
{
    if (m_model == new_model)
        return;
    m_model = new_model;
    emit model_changed(m_model);
}

const QString& ModelBinding::property() const { return m_property; }

void ModelBinding::set_property(const QString& new_property)
{
    if (m_property == new_property)
        return;
    m_property = new_property;
    emit property_changed(m_property);
}

void ModelBinding::write_qml() { m_qml_target.write(m_model_meta_property.read(m_model)); }

void ModelBinding::write_model() { m_model_meta_property.write(m_model, m_qml_target.read()); }

const QVariant& ModelBinding::default_value() const { return m_default_value; }

void ModelBinding::set_default_value(const QVariant& new_default_value)
{
    if (m_default_value == new_default_value)
        return;
    m_default_value = new_default_value;
    emit default_value_changed(m_default_value);
}

bool ModelBinding::receive_model_updates() const { return m_receive_model_updates; }

void ModelBinding::set_receive_model_updates(bool new_receive_model_updates)
{
    if (m_receive_model_updates == new_receive_model_updates)
        return;
    m_receive_model_updates = new_receive_model_updates;
    emit receive_model_updates_changed(m_receive_model_updates);
}
