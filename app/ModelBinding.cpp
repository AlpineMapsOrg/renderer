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

#include "ModelBinding.h"

#include <QQmlProperty>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

namespace {

std::function<QVariant(const QVariant&)> read_fun_for(const QMetaType& t, const QString& member)
{
    switch (t.id()) {
    case QMetaType::Type::QVector2D:
        if (member == "x")
            return [](const QVariant& p) -> QVariant { return p.value<QVector2D>().x(); };
        if (member == "y")
            return [](const QVariant& p) -> QVariant { return p.value<QVector2D>().y(); };
        return {};
    case QMetaType::Type::QVector3D:
        if (member == "x")
            return [](const QVariant& p) -> QVariant { return p.value<QVector3D>().x(); };
        if (member == "y")
            return [](const QVariant& p) -> QVariant { return p.value<QVector3D>().y(); };
        if (member == "z")
            return [](const QVariant& p) -> QVariant { return p.value<QVector3D>().z(); };
        return {};
    case QMetaType::Type::QVector4D:
        if (member == "x")
            return [](const QVariant& p) -> QVariant { return p.value<QVector4D>().x(); };
        if (member == "y")
            return [](const QVariant& p) -> QVariant { return p.value<QVector4D>().y(); };
        if (member == "z")
            return [](const QVariant& p) -> QVariant { return p.value<QVector4D>().z(); };
        if (member == "w")
            return [](const QVariant& p) -> QVariant { return p.value<QVector4D>().w(); };
        return {};
    default:
        return {};
    }
    return {};
}

template <typename Vec> std::function<QVariant(const QVariant&, const QVariant&)> write_fun_for_vec_x()
{
    return [](const QVariant& p, const QVariant& v) -> QVariant {
        auto d = p.value<Vec>();
        d.setX(v.toFloat());
        return d;
    };
}

template <typename Vec> std::function<QVariant(const QVariant&, const QVariant&)> write_fun_for_vec_y()
{
    return [](const QVariant& p, const QVariant& v) -> QVariant {
        auto d = p.value<Vec>();
        d.setY(v.toFloat());
        return d;
    };
}

template <typename Vec> std::function<QVariant(const QVariant&, const QVariant&)> write_fun_for_vec_z()
{
    return [](const QVariant& p, const QVariant& v) -> QVariant {
        auto d = p.value<Vec>();
        d.setZ(v.toFloat());
        return d;
    };
}

template <typename Vec> std::function<QVariant(const QVariant&, const QVariant&)> write_fun_for_vec_w()
{
    return [](const QVariant& p, const QVariant& v) -> QVariant {
        auto d = p.value<Vec>();
        d.setW(v.toFloat());
        return d;
    };
}

template <typename Vec> std::function<QVariant(const QVariant&, const QVariant&)> write_fun_for_vec(const QString& member)
{
    if (member == "x")
        return write_fun_for_vec_x<Vec>();
    if (member == "y")
        return write_fun_for_vec_y<Vec>();
    if (member == "z")
        return write_fun_for_vec_z<Vec>();
    if (member == "w")
        return write_fun_for_vec_w<Vec>();
    return {};
}

std::function<QVariant(const QVariant&, const QVariant&)> write_fun_for(const QMetaType& t, const QString& member)
{
    if (t.id() == QMetaType::Type::QVector2D && member == "x")
        return write_fun_for_vec_x<QVector2D>();
    if (t.id() == QMetaType::Type::QVector2D && member == "y")
        return write_fun_for_vec_y<QVector2D>();

    if (t.id() == QMetaType::Type::QVector3D && member == "x")
        return write_fun_for_vec_x<QVector3D>();
    if (t.id() == QMetaType::Type::QVector3D && member == "y")
        return write_fun_for_vec_y<QVector3D>();
    if (t.id() == QMetaType::Type::QVector3D && member == "z")
        return write_fun_for_vec_z<QVector3D>();

    if (t.id() == QMetaType::Type::QVector4D && member == "x")
        return write_fun_for_vec_x<QVector4D>();
    if (t.id() == QMetaType::Type::QVector4D && member == "y")
        return write_fun_for_vec_y<QVector4D>();
    if (t.id() == QMetaType::Type::QVector4D && member == "z")
        return write_fun_for_vec_z<QVector4D>();
    if (t.id() == QMetaType::Type::QVector4D && member == "w")
        return write_fun_for_vec_w<QVector4D>();
    return {};
}
} // namespace

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
        if (property_chain.size() > 3) {
            qWarning() << "ModelBinding: Property " << m_property << " of object " << m_model
                       << " not supported! At most 3 levels are supported (property[.xyzw], and gadget.property[.xyzw]).";
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
            const auto l0_index = model_meta->indexOfProperty(property_chain.at(0).toStdString().c_str());
            if (l0_index < 0) {
                qWarning() << "ModelBinding:" << property_chain.at(0) << "not found in object" << m_model << "when binding" << m_qml_target.object()
                           << ", property: " << m_qml_target.property().name() << "!";
                return;
            }
            auto l0_meta_property = model_meta->property(l0_index);
            using TypeFlag = QMetaType::TypeFlag;
            if (l0_meta_property.metaType().flags() & (TypeFlag::IsPointer | TypeFlag::PointerToGadget | TypeFlag::PointerToQObject | TypeFlag::IsQmlList)) {
                qWarning() << "ModelBinding:" << l0_meta_property.metaType() << "not supported for" << property_chain.at(0) << "of" << m_model << "when binding"
                           << m_qml_target.object() << ", property: " << m_qml_target.property().name() << "!";
                return;
            }
            QString remaining_address = "";
            if (l0_meta_property.metaType().flags() & QMetaType::TypeFlag::IsGadget) {
                auto gadget_meta_object = l0_meta_property.metaType().metaObject();
                const auto property_index = gadget_meta_object->indexOfProperty(property_chain.at(1).toStdString().c_str());

                if (property_index < 0) {
                    qWarning() << "ModelBinding:" << property_chain.at(1) << "not found in gadget " << gadget_meta_object << "of" << m_model << "when binding"
                               << m_qml_target.object() << ", property: " << m_qml_target.property().name() << "!";
                    return;
                }
                m_gadget_meta_property = l0_meta_property;
                m_meta_property = gadget_meta_object->property(property_index);
                if (property_chain.size() == 3)
                    remaining_address = property_chain.at(2);
            } else {
                m_meta_property = l0_meta_property;
                if (property_chain.size() == 2) {
                    remaining_address = property_chain.at(1);
                } else {
                    qWarning() << "ModelBinding: property " << m_property << "of" << m_model << "not supported, when binding" << m_qml_target.object()
                               << ", property: " << m_qml_target.property().name() << "! Too unexpected chain length.";
                    return;
                }
            }
            if (remaining_address.size() > 0) {
                m_read_fun = read_fun_for(m_meta_property.metaType(), remaining_address);
                m_write_fun = write_fun_for(m_meta_property.metaType(), remaining_address);
                if (!(m_read_fun && m_write_fun)) {
                    qWarning() << "ModelBinding: property " << m_property << "of" << m_model << "not supported, when binding" << m_qml_target.object()
                               << ", property: " << m_qml_target.property().name() << "! No read / write function exists.";
                }
            }
        }
    }

    m_complete = true;
    if (m_default_value.isValid()) {
        m_qml_target.write(m_default_value);
        write_model(); // overwrite default value from model.
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

    const auto qml_value = m_qml_target.read();
    if (m_gadget_meta_property.isValid()) {
        auto gadget = m_gadget_meta_property.read(m_model);
        if (m_write_fun) {
            auto prop = m_meta_property.readOnGadget(gadget.constData());
            prop = m_write_fun(prop, qml_value);
            m_meta_property.writeOnGadget(gadget.data(), prop);
        } else {
            m_meta_property.writeOnGadget(gadget.data(), qml_value);
        }
        m_gadget_meta_property.write(m_model, gadget);
        return;
    }
    if (m_write_fun) {
        auto prop = m_meta_property.read(m_model);
        prop = m_write_fun(prop, qml_value);
        m_meta_property.write(m_model, prop);
    } else {
        m_meta_property.write(m_model, qml_value);
    }
}

QVariant ModelBinding::read_model() const
{
    if (m_gadget_meta_property.isValid()) {
        const auto gadget = m_gadget_meta_property.read(m_model);
        const auto prop = m_meta_property.readOnGadget(gadget.constData());
        if (m_read_fun)
            return m_read_fun(prop);
        return prop;
    }
    const auto prop = m_meta_property.read(m_model);
    if (m_read_fun)
        return m_read_fun(prop);
    return prop;
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
