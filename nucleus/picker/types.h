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

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

namespace nucleus::picker {
Q_NAMESPACE
enum class FeatureType {
    Invalid = 0,
    PointOfInterest = 1,
};
Q_ENUM_NS(FeatureType)

inline FeatureType feature_type(int typeValue)
{
    switch (typeValue) {
    case 1:
        return FeatureType::PointOfInterest;
    default:
        return FeatureType::Invalid;
    }
}

struct Feature {
    Q_GADGET
public:
    QString title;
    FeatureType type;
    QHash<QString, QString> properties;

    Q_INVOKABLE bool is_valid() { return !properties.empty(); }
    [[nodiscard]] QList<QString> get_list_model() const
    {
        QList<QString> list;
        for (const auto& [key, value] : properties.asKeyValueRange()) {
            list.append(key + ":");
            list.append(value);
        }
        return list;
    }

    Q_PROPERTY(QString title MEMBER title)
    Q_PROPERTY(FeatureType type MEMBER type)
    Q_PROPERTY(QHash<QString, QString> properties MEMBER properties)
    // in theory it should be possible to expose QList<SomeStruct> to qml for the listview
    // unfortunately this requires quite a bit of code (e.g. assign roles for each attribute, write custom data access classes etc (see QAbstractItemModel))
    // ultimatively i decided that this is too much work for minor benefits (benefit would be to directly access attributes with the name) and returning a
    // simple QList<QString> was much simpler

    bool operator==(const Feature&) const = default;
    bool operator!=(const Feature&) const = default;
};

} // namespace nucleus::picker
