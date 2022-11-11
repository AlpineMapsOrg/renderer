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

#include "nucleus/camera/Definition.h"
#include "nucleus/Tile.h"
#include "nucleus/camera/NearPlaneAdjuster.h"
#include "nucleus/camera/Adapter.h"

#include <QSignalSpy>
#include <QTest>

class camera_Adapter_and_NearPlaneAdjuster : public QObject {
    Q_OBJECT
private slots:
    void adapter()
    {
        camera::Definition cam { { 100, 0, 0 }, { 0, 0, 0 } };
        camera::Adapter cam_adapter(&cam);
        QSignalSpy worldViewSpy(&cam_adapter, &camera::Adapter::worldViewChanged);
        QSignalSpy projectionSpy(&cam_adapter, &camera::Adapter::projectionChanged);
        QSignalSpy worldProjectionSpy(&cam_adapter, &camera::Adapter::worldViewProjectionChanged);
        cam_adapter.update();
        QVERIFY(worldViewSpy.isValid());
        QVERIFY(projectionSpy.isValid());
        QVERIFY(worldProjectionSpy.isValid());
        worldViewSpy.wait(1);
        projectionSpy.wait(1);
        worldProjectionSpy.wait(1);
        QCOMPARE(worldViewSpy.count(), 1); // adapter should send out camera transformation on startup.
        QCOMPARE(projectionSpy.count(), 1);
        QCOMPARE(worldProjectionSpy.count(), 1);

        worldViewSpy.clear();
        projectionSpy.clear();
        worldProjectionSpy.clear();
        cam_adapter.setNearPlane(2);
        worldViewSpy.wait(1);
        projectionSpy.wait(1);
        worldProjectionSpy.wait(1);
        QCOMPARE(worldViewSpy.count(), 0);
        QCOMPARE(projectionSpy.count(), 1);
        QCOMPARE(worldProjectionSpy.count(), 1);

        worldViewSpy.clear();
        projectionSpy.clear();
        worldProjectionSpy.clear();
        cam_adapter.move({ 1, 1, 1 });
        worldViewSpy.wait(1);
        projectionSpy.wait(1);
        worldProjectionSpy.wait(1);
        QCOMPARE(worldViewSpy.count(), 1);
        QCOMPARE(projectionSpy.count(), 0);
        QCOMPARE(worldProjectionSpy.count(), 1);

        worldViewSpy.clear();
        projectionSpy.clear();
        worldProjectionSpy.clear();
        cam_adapter.orbit({ 1, 1, 1 }, {33, 45});
        worldViewSpy.wait(1);
        projectionSpy.wait(1);
        worldProjectionSpy.wait(1);
        QCOMPARE(worldViewSpy.count(), 1);
        QCOMPARE(projectionSpy.count(), 0);
        QCOMPARE(worldProjectionSpy.count(), 1);
    }

    void nearPlaneAdjuster_adding_removing()
    {
        camera::Definition cam { { 100, 0, 0 }, { 0, 0, 0 } };
        camera::Adapter cam_adapter(&cam);
        camera::NearPlaneAdjuster near_plane_adjuster;

        connect(&cam_adapter, &camera::Adapter::positionChanged, &near_plane_adjuster, &camera::NearPlaneAdjuster::changeCameraPosition);
        cam_adapter.update();


        QSignalSpy spy(&near_plane_adjuster, &camera::NearPlaneAdjuster::nearPlaneChanged);
        spy.wait(1);
        QCOMPARE(spy.count(), 0);

        near_plane_adjuster.addTile(std::make_shared<Tile>(
            tile::Id { 0, {} },
            tile::SrsAndHeightBounds { { -0.5, -0.5, -0.5 }, { 0.5, 0.5, 0.5 } },
            Raster<uint16_t> {},
            QImage {}));
        spy.wait(1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.front().front().toDouble(), float(100 - std::sqrt(3.0) / 2));

        spy.clear();
        near_plane_adjuster.addTile(std::make_shared<Tile>(
            tile::Id { 1, {} },
            tile::SrsAndHeightBounds { { -100.5, -0.5, -0.5 }, { -99.5, 0.5, 0.5 } },
            Raster<uint16_t> {},
            QImage {}));
        spy.wait(1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.front().front().toDouble(), float(100 - std::sqrt(3.0) / 2));

        spy.clear();
        near_plane_adjuster.removeTile(tile::Id { 0, {} });
        spy.wait(1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.front().front().toDouble(), float(200 - std::sqrt(3.0) / 2));
    }

    void nearPlaneAdjuster_camera_update()
    {
        camera::Definition cam { { 100, 0, 0 }, { 0, 0, 0 } };
        camera::Adapter cam_adapter(&cam);
        camera::NearPlaneAdjuster near_plane_adjuster;

        connect(&cam_adapter, &camera::Adapter::positionChanged, &near_plane_adjuster, &camera::NearPlaneAdjuster::changeCameraPosition);
        cam_adapter.update();

        near_plane_adjuster.addTile(std::make_shared<Tile>(
            tile::Id { 0, {} },
            tile::SrsAndHeightBounds { { -0.5, -0.5, -0.5 }, { 0.5, 0.5, 0.5 } },
            Raster<uint16_t> {},
            QImage {}));
        near_plane_adjuster.addTile(std::make_shared<Tile>(
            tile::Id { 1, {} },
            tile::SrsAndHeightBounds { { -100.5, -0.5, -0.5 }, { -99.5, 0.5, 0.5 } },
            Raster<uint16_t> {},
            QImage {}));

        QSignalSpy spy(&near_plane_adjuster, &camera::NearPlaneAdjuster::nearPlaneChanged);
        cam_adapter.move({-50, 0, 0});
        spy.wait(1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.front().front().toDouble(), float(50 - std::sqrt(3.0) / 2));

        spy.clear();
        cam_adapter.move({-50, 0, 0});
        spy.wait(1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.front().front().toDouble(), float(0.1));
    }
};

QTEST_MAIN(camera_Adapter_and_NearPlaneAdjuster)
#include "camera_Adapter_and_NearPlaneAdjuster.moc"
