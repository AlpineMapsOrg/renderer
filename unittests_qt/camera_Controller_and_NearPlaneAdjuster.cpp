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
#include "nucleus/camera/Controller.h"

#include <QSignalSpy>
#include <QTest>

class camera_Controller_and_NearPlaneAdjuster : public QObject {
    Q_OBJECT
private slots:
    void adapter()
    {
        camera::Controller cam_adapter(camera::Definition{ { 100, 0, 0 }, { 0, 0, 0 } });
        QSignalSpy worldProjectionSpy(&cam_adapter, &camera::Controller::definitionChanged);
        cam_adapter.update();
        QVERIFY(worldProjectionSpy.isValid());
        worldProjectionSpy.wait(1);
        QCOMPARE(worldProjectionSpy.count(), 1);

        worldProjectionSpy.clear();
        cam_adapter.setNearPlane(2);
        worldProjectionSpy.wait(1);
        QCOMPARE(worldProjectionSpy.count(), 1);

        worldProjectionSpy.clear();
        cam_adapter.move({ 1, 1, 1 });
        worldProjectionSpy.wait(1);
        QCOMPARE(worldProjectionSpy.count(), 1);

        worldProjectionSpy.clear();
        cam_adapter.orbit({ 1, 1, 1 }, {33, 45});
        worldProjectionSpy.wait(1);
        QCOMPARE(worldProjectionSpy.count(), 1);
    }

    void nearPlaneAdjuster_adding_removing()
    {
        camera::Controller cam_adapter(camera::Definition{ { 100, 0, 0 }, { 0, 0, 0 } });
        camera::NearPlaneAdjuster near_plane_adjuster;

        connect(&cam_adapter, &camera::Controller::definitionChanged, &near_plane_adjuster, &camera::NearPlaneAdjuster::updateCamera);
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
        camera::Controller cam_adapter(camera::Definition{ { 100, 0, 0 }, { 0, 0, 0 } });
        camera::NearPlaneAdjuster near_plane_adjuster;

        connect(&cam_adapter, &camera::Controller::definitionChanged, &near_plane_adjuster, &camera::NearPlaneAdjuster::updateCamera);
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
        QCOMPARE(spy.front().front().toDouble(), float(1.0));
    }
};

QTEST_MAIN(camera_Controller_and_NearPlaneAdjuster)
#include "camera_Controller_and_NearPlaneAdjuster.moc"
