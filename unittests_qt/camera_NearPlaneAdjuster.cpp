/*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2022 Adam Celarek
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

#include "alpine_renderer/Camera.h"
#include "alpine_renderer/camera/NearPlaneAdjuster.h"
#include "alpine_renderer/camera/Adapter.h"

#include <QSignalSpy>
#include <QTest>

class camera_Adapter_and_NearPlaneAdjuster : public QObject {
    Q_OBJECT
private slots:
    void adapter()
    {
        Camera cam { { 100, 0, 0 }, { 0, 0, 0 } };
        camera::Adapter cam_adapter(&cam);
        QSignalSpy spy(&cam_adapter, &camera::Adapter::cameraTransformationChanged);
        spy.wait(1);
        QCOMPARE(spy.count(), 1);
    }

    void interface()
    {
        Camera cam { { 100, 0, 0 }, { 0, 0, 0 } };
        camera::Adapter cam_adapter(&cam);
        camera::NearPlaneAdjuster near_plane_adjuster;

        connect(&cam_adapter, &camera::Adapter::cameraTransformationChanged, &near_plane_adjuster, &camera::NearPlaneAdjuster::changeCameraTransformation);

//        QSignalSpy spy(&cam_adapter, &camera::Adapter::cameraTransformationChanged);

    }
};

QTEST_MAIN(camera_Adapter_and_NearPlaneAdjuster)
#include "qtest_camera_NearPlaneAdjuster.moc"
