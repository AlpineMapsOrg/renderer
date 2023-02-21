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

#include <QAbstractProxyModel>

#include "AbstractMapLabelModel.h"

class CameraFilterProxyModel : public QAbstractProxyModel, public AbstractMapLabelModel {
    Q_OBJECT
public:
    explicit CameraFilterProxyModel(QObject* parent = nullptr);

    // public:
    //     [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    //     [[nodiscard]] QModelIndex parent(const QModelIndex& child) const override;
    //     [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    //     [[nodiscard]] int columnCount(const QModelIndex& parent) const override;
    //     [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    //     [[nodiscard]] QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;
    //     [[nodiscard]] QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;
    //     [[nodiscard]] std::vector<MapLabel> data() const override;
};
