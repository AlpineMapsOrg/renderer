/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
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

#include <QAbstractListModel>

#include "AbstractMapLabelModel.h"

namespace nucleus::map_label {
class MapLabelModel : public QAbstractListModel, public AbstractMapLabelModel {
    Q_OBJECT

public:
    explicit MapLabelModel(QObject* parent = nullptr);

public:
    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] std::vector<MapLabel> data() const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

private:
    std::vector<MapLabel> m_labels;
};

}
