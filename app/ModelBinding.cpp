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
    assert(m_complete == false);
    if (!m_qml_target.isValid()) {
        qWarning() << "ModelBinding: QML property invalid!";
        if (m_model)
            qWarning() << "ModelBinding: model target: " << m_model;
        if (m_property.size() > 0)
            qWarning() << "ModelBinding: model property: " << m_property;
        return;
    }
    if (!m_model) {
        qWarning() << "ModelBinding: model target invalid!";
        if (m_qml_target.isValid())
            qWarning() << "ModelBinding: QML target: " << m_qml_target.object() << ", property: " << m_qml_target.property().name();
        if (m_property.size() > 0)
            qWarning() << "ModelBinding: model property: " << m_property;
        return;
    }

    const auto* this_meta = this->metaObject();
    {
        const auto property_chain = m_property.split('.');
        if (property_chain.size() > 2) {
            qWarning() << "ModelBinding: Property " << m_property << " of object " << m_model
                       << " not supported! At most 2 levels are supported (gadget.property).";
        }

        const auto* model_meta = m_model->metaObject();
        if (property_chain.size() == 1) {
            const auto property_index = model_meta->indexOfProperty(m_property.toStdString().c_str());
            if (property_index < 0) {
                qWarning() << "ModelBinding:" << m_property << "not found in object" << m_model << "when binding" << m_qml_target.object()
                           << ", property: " << m_qml_target.property().name() << "!";
                return;
            }
            m_meta_property = model_meta->property(property_index);
        } else {
            const auto gadget_index = model_meta->indexOfProperty(property_chain.at(0).toStdString().c_str());
            if (gadget_index < 0) {
                qWarning() << "ModelBinding:" << property_chain.at(0) << "not found in object" << m_model << "when binding" << m_qml_target.object()
                           << ", property: " << m_qml_target.property().name() << "!";
                return;
            }
            auto gadget_meta_property = model_meta->property(gadget_index);
            if (!(gadget_meta_property.metaType().flags() & QMetaType::TypeFlag::IsGadget)) {
                qWarning() << "ModelBinding: Property " << m_property << " of object " << m_model
                           << " not supported! If two levels are used, the first must be a gadget (gadget.property)."
                           << "If you have an object hierarchy, select the correct object in the target.";
                return;
            }
            auto gadget_meta_object = gadget_meta_property.metaType().metaObject();
            const auto property_index = gadget_meta_object->indexOfProperty(property_chain.at(1).toStdString().c_str());
            if (property_index < 0) {
                qWarning() << "ModelBinding:" << property_chain.at(1) << "not found in gadget " << gadget_meta_object << "of" << m_model << "when binding"
                           << m_qml_target.object() << ", property: " << m_qml_target.property().name() << "!";
                return;
            }
            m_gadget_meta_property = gadget_meta_property;
            m_meta_property = gadget_meta_object->property(property_index);
        }
    }

    m_complete = true;
    if (m_default_value.isValid()) {
        m_qml_target.write(m_default_value);
    } else {
        write_qml();
    }

    const auto write_model_slot = this_meta->indexOfSlot("write_model()");

    connect(m_qml_target.object(), m_qml_target.property().notifySignal(), this, this_meta->method(write_model_slot));
    if (m_receive_model_updates) {
        if (!m_meta_property.hasNotifySignal()) {
            qWarning() << "Property " << m_property << " of object " << m_model << " has no notification signal!";
            return;
        }

        const auto qml_write_slot = this_meta->indexOfSlot("write_qml()");
        connect(m_model, m_meta_property.notifySignal(), this, this_meta->method(qml_write_slot));
    }
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

void ModelBinding::write_qml()
{
    if (!m_complete)
        return;
    m_qml_target.write(read_model());
}

void ModelBinding::write_model()
{
    if (!m_complete)
        return;

    if (m_gadget_meta_property.isValid()) {
        auto gadget = m_gadget_meta_property.read(m_model);
        m_meta_property.writeOnGadget(gadget.data(), m_qml_target.read());
        m_gadget_meta_property.write(m_model, gadget);
    }
    m_meta_property.write(m_model, m_qml_target.read());
}

QVariant ModelBinding::read_model() const
{
    if (m_gadget_meta_property.isValid()) {
        const auto gadget = m_gadget_meta_property.read(m_model);
        return m_meta_property.readOnGadget(gadget.constData());
    }
    return m_meta_property.read(m_model);
}

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
