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

constexpr float near_plane_adjustment_factor = 0.8;

class camera_Controller_and_NearPlaneAdjuster : public QObject {
    Q_OBJECT
private slots:
    void adapter()
    {
        camera::Controller cam_adapter(camera::Definition{ { 100, 0, 0 }, { 0, 0, 0 } });
        QSignalSpy worldProjectionSpy(&cam_adapter, &camera::Controller::definition_changed);
        cam_adapter.update();
        QVERIFY(worldProjectionSpy.isValid());
        worldProjectionSpy.wait(1);
        QCOMPARE(worldProjectionSpy.count(), 1);

        worldProjectionSpy.clear();
        cam_adapter.set_near_plane(2);
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
        camera::Controller cam_adapter(camera::Definition{ { 100, 0, 100 }, { 0, 0, 0 } });
        camera::NearPlaneAdjuster near_plane_adjuster;

        connect(&cam_adapter, &camera::Controller::definition_changed, &near_plane_adjuster, &camera::NearPlaneAdjuster::update_camera);
        cam_adapter.update();


        QSignalSpy spy(&near_plane_adjuster, &camera::NearPlaneAdjuster::near_plane_changed);
        spy.wait(1);
        QCOMPARE(spy.count(), 0);

        near_plane_adjuster.add_tile(std::make_shared<Tile>(
            tile::Id { 0, {} },
            tile::SrsAndHeightBounds { { -0.5, -0.5, -0.5 }, { 0.5, 0.5, 10.0 } },
            Raster<uint16_t> {},
            QImage {}));
        spy.wait(1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.front().front().toDouble(), float(90 * near_plane_adjustment_factor));

        spy.clear();
        near_plane_adjuster.add_tile(std::make_shared<Tile>(
            tile::Id { 1, {} },
            tile::SrsAndHeightBounds { { -100.5, -0.5, -0.5 }, { -99.5, 0.5, 20.0 } },
            Raster<uint16_t> {},
            QImage {}));
        spy.wait(1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.front().front().toDouble(), float(80.0 * near_plane_adjustment_factor));

        spy.clear();
        near_plane_adjuster.remove_tile(tile::Id { 0, {} });
        spy.wait(1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.front().front().toDouble(), float(80 * near_plane_adjustment_factor));

        near_plane_adjuster.add_tile(std::make_shared<Tile>(
            tile::Id { 0, {} },
            tile::SrsAndHeightBounds { { -0.5, -0.5, -0.5 }, { 0.5, 0.5, 10.0 } },
            Raster<uint16_t> {},
            QImage {}));

        spy.clear();
        near_plane_adjuster.remove_tile(tile::Id { 1, {} });
        spy.wait(1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.front().front().toDouble(), float(90 * near_plane_adjustment_factor));

    }

    void nearPlaneAdjuster_camera_update()
    {
        camera::Controller cam_adapter(camera::Definition{ { 100, 0, 100 }, { 0, 0, 0 } });
        camera::NearPlaneAdjuster near_plane_adjuster;

        connect(&cam_adapter, &camera::Controller::definition_changed, &near_plane_adjuster, &camera::NearPlaneAdjuster::update_camera);
        cam_adapter.update();

        near_plane_adjuster.add_tile(std::make_shared<Tile>(
            tile::Id { 0, {} },
            tile::SrsAndHeightBounds { { -0.5, -0.5, -0.5 }, { 0.5, 0.5, 10.0 } },
            Raster<uint16_t> {},
            QImage {}));
        near_plane_adjuster.add_tile(std::make_shared<Tile>(
            tile::Id { 1, {} },
            tile::SrsAndHeightBounds { { -100.5, -0.5, -0.5 }, { -99.5, 0.5, 20.0 } },
            Raster<uint16_t> {},
            QImage {}));

        QSignalSpy spy(&near_plane_adjuster, &camera::NearPlaneAdjuster::near_plane_changed);
        cam_adapter.move({-50, 0, -50});
        spy.wait(1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.front().front().toDouble(), float(30.0 * near_plane_adjustment_factor));

        spy.clear();
        cam_adapter.move({-50, 0, 10});
        spy.wait(1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.front().front().toDouble(), float(40.0 * near_plane_adjustment_factor));
    }
};

QTEST_MAIN(camera_Controller_and_NearPlaneAdjuster)
#include "camera_Controller_and_NearPlaneAdjuster.moc"
