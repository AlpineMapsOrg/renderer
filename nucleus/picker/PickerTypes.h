/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Lucas Dworschak
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

#include <QList>
#include <QObject>
#include <QString>

namespace nucleus::picker {
enum PickTypes {
    invalid = 0,
    feature = 1,

};

inline PickTypes get_picker_type(int typeValue)
{
    switch (typeValue) {
    case 1:
        return PickTypes::feature;
    default:
        return PickTypes::invalid;
    }
}

struct FeatureProperty {
    Q_GADGET
public:
    FeatureProperty(const FeatureProperty& other)
        : m_key(other.m_key)
        , m_value(other.m_value) {};

    FeatureProperty(QString key, QString value)
        : m_key(key)
        , m_value(value) {};

    QString m_key;
    QString m_value;

    Q_PROPERTY(QString key MEMBER m_key)
    Q_PROPERTY(QString value MEMBER m_value)

    bool operator==(const FeatureProperty&) const = default;
    bool operator!=(const FeatureProperty&) const = default;
};

struct FeatureProperties {
    Q_GADGET
public:
    QString m_title;
    QList<FeatureProperty> m_properties;

    Q_INVOKABLE bool is_valid() { return !m_properties.empty(); }
    const QList<QString> get_list_model() const
    {
        QList<QString> list;
        for (const auto& prop : m_properties) {
            list.append(prop.m_key + ":");
            list.append(prop.m_value);
        }
        return list;
    }

    Q_PROPERTY(QString title MEMBER m_title)
    // in theory it should be possible to expose QList<SomeStruct> to qml for the listview
    // unfortunately this requires quite a bit of code (e.g. assign roles for each attribute, write custom data access classes etc (see QAbstractItemModel))
    // ultimatively i decided that this is too much work for minor benefits (benefit would be to directly access attributes with the name) and returning a
    // simple QList<QString> was much simpler
    //    Q_PROPERTY(QList<FeatureProperty> properties READ properties)

    bool operator==(const FeatureProperties&) const = default;
    bool operator!=(const FeatureProperties&) const = default;
};

} // namespace nucleus::picker
