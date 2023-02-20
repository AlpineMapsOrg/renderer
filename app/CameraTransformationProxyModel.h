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

#include <QIdentityProxyModel>

#include "nucleus/camera/Definition.h"

class CameraTransformationProxyModel : public QIdentityProxyModel {
    Q_OBJECT

public:
    CameraTransformationProxyModel();
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    nucleus::camera::Definition camera() const;
    void set_camera(const nucleus::camera::Definition& new_camera);

signals:
    void camera_changed();

private:
    nucleus::camera::Definition m_camera;
    Q_PROPERTY(nucleus::camera::Definition camera READ camera WRITE set_camera NOTIFY camera_changed)
};
