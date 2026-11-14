/*****************************************************************************
 * AlpineMaps.org Renderer
 * Copyright (C) 2026 Markus Rampp

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

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <nucleus/camera/Definition.h>
#include <nucleus/camera/RoutePreviewAnimation.h>

#include <glm/glm.hpp>
#include <cmath>

namespace {

// straight 1 km route descending towards +x at 1000 m altitude
std::vector<glm::dvec3> straight_route()
{
    return { { 0.0, 0.0, 1000.0 / 0.7 }, { 1000.0 / 0.7, 0.0, 1000.0 / 0.7 } };
}

// deterministic clock for the animation tests: tick() advances virtual time
class FakeClock {
public:
    double seconds() const { return m_seconds; }
    void tick(double seconds) { m_seconds += seconds; }

private:
    double m_seconds = 0;
};

} // namespace

TEST_CASE("RoutePreview advances along the route")
{
    using namespace nucleus::camera;
    FakeClock clock;
    RoutePreviewAnimation preview(straight_route(), [&clock] { return clock.seconds(); });

    Definition camera;
    const auto first = preview.update(camera, nullptr);
    REQUIRE(first.has_value());
    // starts at the route start, 30 m above
    CHECK(first->position().x < 10.0);

    // each update consumes the virtual time ticked since the previous one;
    // 0.0625 s at the default 24 m/s advances exactly 1.5 m along +x
    double last_x = first->position().x;
    for (int i = 0; i < 20; ++i) {
        clock.tick(0.0625);
        const auto frame = preview.update(camera, nullptr);
        REQUIRE(frame.has_value());
        CHECK_THAT(frame->position().x, Catch::Matchers::WithinAbs(last_x + 1.5, 1e-9));
        last_x = frame->position().x;
    }
    CHECK(preview.progress_fraction() > 0.0);
    CHECK(preview.progress_fraction() < 1.0);
}

TEST_CASE("RoutePreview paused and dragging hold the position")
{
    using namespace nucleus::camera;
    RoutePreviewAnimation preview(straight_route());
    preview.update({}, nullptr);

    preview.pause(true);
    const auto frozen = preview.update({}, nullptr);
    REQUIRE(frozen.has_value());
    const double frozen_x = frozen->position().x;
    const auto frozen2 = preview.update({}, nullptr);
    CHECK(frozen2->position().x == frozen_x);

    preview.pause(false);
    preview.set_dragging(true);
    const auto dragged = preview.update({}, nullptr);
    CHECK(dragged->position().x == frozen_x);
    CHECK(preview.paused()); // dragging counts as paused
}

TEST_CASE("RoutePreview seek jumps to the given fraction")
{
    using namespace nucleus::camera;
    RoutePreviewAnimation preview(straight_route());
    preview.seek(0.5);
    const auto frame = preview.update({}, nullptr);
    REQUIRE(frame.has_value());
    CHECK_THAT(frame->position().x, Catch::Matchers::WithinAbs(500.0 / 0.7, 20.0));
    CHECK(preview.progress_fraction() > 0.45);
    CHECK(preview.progress_fraction() < 0.55);
}

TEST_CASE("RoutePreview orbit rotates without moving")
{
    using namespace nucleus::camera;
    RoutePreviewAnimation preview(straight_route());
    preview.seek(0.5);
    const auto before = preview.update({}, nullptr);
    REQUIRE(before.has_value());

    preview.pause(true);
    preview.orbit(30.0, 10.0);
    const auto after = preview.update({}, nullptr);
    REQUIRE(after.has_value());

    CHECK(glm::length(after->position() - before->position()) < 0.001); // position unchanged
    CHECK(glm::length(after->z_axis() - before->z_axis()) > 0.01);      // orientation changed
}

TEST_CASE("RoutePreview auto-pauses at the route end")
{
    using namespace nucleus::camera;
    RoutePreviewAnimation preview(straight_route());
    preview.seek(1.0);
    const auto end_frame = preview.update({}, nullptr);
    REQUIRE(end_frame.has_value());
    const double end_x = end_frame->position().x;
    for (int i = 0; i < 30; ++i) {
        const auto frame = preview.update({}, nullptr);
        REQUIRE(frame.has_value());
        CHECK(frame->position().x == end_x); // stays at the end
    }
    // still active: the caller decides when to stop the preview
    CHECK(preview.progress_fraction() > 0.99);
}

TEST_CASE("RoutePreviewAnimation speed setting")
{
    using namespace nucleus::camera;
    FakeClock clock;
    RoutePreviewAnimation preview(straight_route(), [&clock] { return clock.seconds(); });
    preview.set_speed(48.0);
    CHECK(preview.speed() == 48.0);

    preview.update({}, nullptr); // consume the constructor time
    clock.tick(0.0625);
    const auto frame_a = preview.update({}, nullptr);
    clock.tick(0.0625);
    const auto frame_b = preview.update({}, nullptr);
    const double advanced = std::abs(frame_b->position().x - frame_a->position().x);
    // at 48 m/s one 0.0625 s frame advances 3 m along +x
    CHECK_THAT(advanced, Catch::Matchers::WithinAbs(3.0, 1e-9));
}

#ifdef QT_GUI_LIB
#include <nucleus/camera/Controller.h>

TEST_CASE("Controller route preview reports the actual speed")
{
    using namespace nucleus::camera;
    Controller controller(Definition {}, nullptr, nullptr);

    std::vector<double> reported_speeds;
    bool active_reported = false;
    QObject::connect(&controller, &Controller::route_preview_changed,
        [&](double, bool, bool active, double speed) {
            reported_speeds.push_back(speed);
            active_reported = active;
        });

    const std::vector<glm::dvec3> route = { { 0, 0, 1000 }, { 1000, 0, 900 } };
    controller.start_route_preview(route);
    controller.advance_camera();
    REQUIRE(active_reported);
    REQUIRE_FALSE(reported_speeds.empty());
    // the default must be the "normal" speed (24 m/s), not slow
    CHECK(reported_speeds.back() == 24.0);

    controller.set_route_preview_speed(48.0);
    controller.advance_camera();
    CHECK(reported_speeds.back() == 48.0);

    // arrow-key path goes through the same slot
    controller.set_route_preview_speed(12.0);
    controller.advance_camera();
    CHECK(reported_speeds.back() == 12.0);

    controller.stop_route_preview();
}
#endif
