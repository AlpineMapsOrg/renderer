/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Manuel Eiweck
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

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <webgpu/compute/TimeSteppedSimulation.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <vector>

using namespace webgpu_compute;
using Catch::Approx;

namespace {

/// Stands in for the MPM solver. Simulated time moves when a chunk is submitted.
/// Accuracy (advance()): busy for one frame per chunk (the solver chains the next chunk from the
/// previous one's work-done callback, about once per frame). Real time (advance_frame()): the
/// chunk goes out at once; it keeps the state before it, and shown_time() is what the renderer
/// would draw with the interpolation the clock set. GPU time is the steps times a fixed cost.
/// A reset takes effect with the next run, as in the solver.
class FakeSimulation : public TimeSteppedSimulation {
public:
    float dt = 0.01f;
    float gpu_ms = 0.0f; // per step; 0 = not measured yet
    bool ready = true;
    bool refuse_frames = false; // the GPU is too far behind
    float time = 0.0f;

    struct Run {
        uint32_t steps;
        uint32_t chunks;
    };
    std::vector<Run> runs; // accuracy runs
    std::deque<uint32_t> chained; // chunks of the accuracy run still to come, one per frame
    bool chained_busy = false;

    bool frame_run_open = false;
    uint32_t frame_runs_closed = 0;
    std::vector<uint32_t> frame_chunks; // steps of every real-time chunk
    float chunk_start = 0.0f; // the states the renderer blends between
    float chunk_end = 0.0f;
    bool previous_valid = false;
    float interpolation = 1.0f;
    bool reset_pending = false;

    float gpu_ms_this_frame = 0.0f;
    float max_gpu_ms_per_frame = 0.0f;
    float time_at_frame_start = 0.0f;
    float shown_at_frame_start = 0.0f;
    std::vector<float> frame_advance; // simulated time per frame
    std::vector<float> shown_advance; // drawn time per frame

    bool can_advance() const override { return ready; }
    bool is_advancing() const override { return chained_busy || frame_run_open; }
    void advance(uint32_t steps, uint32_t chunks) override
    {
        REQUIRE(!is_advancing());
        REQUIRE(steps > 0);
        REQUIRE(chunks > 0);
        REQUIRE(chunks <= steps);
        apply_pending_reset();
        runs.push_back({ steps, chunks });
        chained.clear();
        for (uint32_t i = 0; i < chunks; i++)
            chained.push_back(uint32_t(uint64_t(i + 1) * steps / chunks - uint64_t(i) * steps / chunks));
        time += float(steps) * dt;
        previous_valid = false;
        chained_busy = true;
        gpu_ms_this_frame += gpu_ms * float(chained.front());
        chained.pop_front();
    }
    bool advance_frame(uint32_t steps, bool end_run) override
    {
        if (refuse_frames || chained_busy || (!frame_run_open && steps == 0))
            return false;
        if (steps > 0) {
            if (!frame_run_open)
                previous_valid = !apply_pending_reset();
            else
                previous_valid = true;
            chunk_start = time;
            time += float(steps) * dt;
            chunk_end = time;
            gpu_ms_this_frame += gpu_ms * float(steps);
            frame_chunks.push_back(steps);
        }
        frame_run_open = !end_run;
        if (end_run)
            frame_runs_closed++;
        return true;
    }
    void set_interpolation(float alpha) override { interpolation = alpha; }
    float time_step() const override { return dt; }
    float simulated_time() const override { return time; }
    float gpu_ms_per_step() const override { return gpu_ms; }
    void reset() override { reset_pending = true; }

    bool apply_pending_reset()
    {
        if (!reset_pending)
            return false;
        reset_pending = false;
        time = 0.0f;
        return true;
    }
    float shown_time() const { return previous_valid ? chunk_start + interpolation * (chunk_end - chunk_start) : time; }

    /// The frame boundary.
    void end_frame()
    {
        max_gpu_ms_per_frame = std::max(max_gpu_ms_per_frame, gpu_ms_this_frame);
        gpu_ms_this_frame = 0.0f;
        frame_advance.push_back(time - time_at_frame_start);
        time_at_frame_start = time;
        shown_advance.push_back(shown_time() - shown_at_frame_start);
        shown_at_frame_start = shown_time();
        if (chained_busy) {
            if (chained.empty()) {
                chained_busy = false;
            } else {
                gpu_ms_this_frame += gpu_ms * float(chained.front()); // the next frame's chunk
                chained.pop_front();
            }
        }
    }
};

/// Plays `seconds` of wall time at a fixed frame rate.
void play_for(FakeSimulation& simulation, float seconds, float frame_seconds = 1.0f / 60.0f)
{
    const int frames = int(std::lround(seconds / frame_seconds));
    for (int i = 0; i < frames; i++) {
        simulation.clock().tick(simulation, frame_seconds);
        simulation.end_frame();
    }
}

SimulationClock::Settings settings_with(SimulationClock::Mode mode, float speed = 1.0f, float budget_ms = 6.0f)
{
    SimulationClock::Settings settings;
    settings.mode = mode;
    settings.speed = speed;
    settings.budget_ms = budget_ms;
    return settings;
}

} // namespace

TEST_CASE("webgpu_compute::SimulationClock")
{
    FakeSimulation simulation;
    auto& clock = simulation.clock();

    SECTION("does nothing while paused")
    {
        clock.set_settings(settings_with(SimulationClock::Mode::RealTime));
        play_for(simulation, 1.0f);
        CHECK(simulation.runs.empty());
        CHECK(simulation.frame_chunks.empty());
        CHECK(simulation.time == 0.0f);
    }

    SECTION("real time follows the wall clock, one chunk per frame")
    {
        simulation.gpu_ms = 0.1f;
        clock.set_settings(settings_with(SimulationClock::Mode::RealTime));
        clock.play();
        play_for(simulation, 10.0f);
        // The simulation is at most one step ahead of the wall clock.
        CHECK(simulation.time >= 10.0f - 0.02f);
        CHECK(simulation.time <= 10.0f + 0.011f);
        CHECK(clock.measured_speed() == Approx(1.0f).margin(0.05f));
        CHECK_FALSE(clock.behind());
        CHECK(simulation.runs.empty());
        // A run is closed every REAL_TIME_RUN_CHUNKS chunks, for the overlay and diagnostics.
        CHECK(simulation.frame_runs_closed == simulation.frame_chunks.size() / SimulationClock::REAL_TIME_RUN_CHUNKS);
    }

    SECTION("interpolation draws every frame one frame further")
    {
        // 1.67 steps per frame at 60 fps and dt 10 ms: the simulation advances by one or two
        // steps a frame, the drawn state by exactly one frame's worth.
        simulation.gpu_ms = 0.1f;
        clock.set_settings(settings_with(SimulationClock::Mode::RealTime));
        clock.play();
        play_for(simulation, 5.0f);
        for (size_t i = 10; i < simulation.shown_advance.size(); i++)
            CHECK(simulation.shown_advance[i] == Approx(1.0f / 60.0f).margin(1e-4f));
        // The drawn time never passes the simulation.
        CHECK(simulation.shown_time() <= simulation.time + 1e-6f);
    }

    SECTION("interpolation moves the picture also when a step is longer than a frame")
    {
        // 0.1x: 1.67 ms of simulated time per frame, 10 ms per step - a step every six frames.
        simulation.gpu_ms = 0.1f;
        clock.set_settings(settings_with(SimulationClock::Mode::RealTime, 0.1f));
        clock.play();
        play_for(simulation, 5.0f);
        size_t still_simulation = 0;
        for (size_t i = 10; i < simulation.frame_advance.size(); i++) {
            still_simulation += simulation.frame_advance[i] == 0.0f ? 1u : 0u;
            CHECK(simulation.shown_advance[i] == Approx(0.1f / 60.0f).margin(1e-4f));
        }
        CHECK(still_simulation > simulation.frame_advance.size() / 2); // the simulation itself mostly stands still
    }

    SECTION("without interpolation every frame shows the latest state")
    {
        simulation.gpu_ms = 0.1f;
        auto settings = settings_with(SimulationClock::Mode::RealTime, 0.1f);
        settings.interpolate = false;
        clock.set_settings(settings);
        clock.play();
        play_for(simulation, 2.0f);
        CHECK(simulation.interpolation == 1.0f);
        CHECK(simulation.shown_time() == simulation.time);
        CHECK(simulation.time == Approx(0.2f).margin(0.011f));
    }

    SECTION("real time scales with the speed")
    {
        for (const float speed : { 0.25f, 2.0f, 4.0f }) {
            FakeSimulation s;
            s.gpu_ms = 0.1f;
            s.clock().set_settings(settings_with(SimulationClock::Mode::RealTime, speed));
            s.clock().play();
            play_for(s, 10.0f);
            CHECK(s.time == Approx(10.0f * speed).margin(0.03f));
            CHECK(s.clock().measured_speed() == Approx(speed).margin(0.05f * speed + 0.02f));
        }
    }

    SECTION("speed is clamped to the slider range")
    {
        simulation.gpu_ms = 0.1f;
        clock.set_settings(settings_with(SimulationClock::Mode::RealTime, 100.0f));
        clock.play();
        play_for(simulation, 5.0f);
        CHECK(simulation.time == Approx(5.0f * SimulationClock::MAX_SPEED).margin(0.1f));
    }

    SECTION("real time does not depend on the frame rate")
    {
        for (const float fps : { 20.0f, 30.0f, 60.0f, 144.0f }) {
            FakeSimulation s;
            s.gpu_ms = 0.1f;
            s.clock().set_settings(settings_with(SimulationClock::Mode::RealTime));
            s.clock().play();
            play_for(s, 10.0f, 1.0f / fps);
            CHECK(s.time == Approx(10.0f).margin(0.03f));
        }
    }

    SECTION("real time falls behind instead of exceeding the budget")
    {
        // 1 ms per step and a 4 ms budget: 4 steps per frame, 2.4 s of simulation per second
        // at dt 10 ms and 60 fps. Ask for 4x: the GPU cannot, so the simulation slows down.
        simulation.gpu_ms = 1.0f;
        clock.set_settings(settings_with(SimulationClock::Mode::RealTime, 4.0f, 4.0f));
        clock.play();
        play_for(simulation, 10.0f);
        CHECK(clock.steps_per_frame_budget() == Approx(4.0f));
        CHECK(clock.behind());
        CHECK(clock.measured_speed() == Approx(2.4f).margin(0.1f));
        CHECK(simulation.time == Approx(24.0f).margin(0.5f));
        // At most one step over the budget per frame (the fractional carry), and no catching up.
        CHECK(simulation.max_gpu_ms_per_frame <= 5.0f + 1e-4f);
        // The picture still moves evenly, at the speed the GPU manages.
        for (size_t i = 120; i < simulation.shown_advance.size(); i++)
            CHECK(simulation.shown_advance[i] == Approx(0.04f).margin(0.011f));
    }

    SECTION("real time uses a fractional budget")
    {
        // 6 ms at 1.7 ms per step is 3.5 steps per frame, enough for 1x at dt 5 ms and 60 fps
        // (3.3 steps per frame) - but not if every frame is rounded down to 3 steps.
        simulation.dt = 0.005f;
        simulation.gpu_ms = 1.7f;
        clock.set_settings(settings_with(SimulationClock::Mode::RealTime, 1.0f, 6.0f));
        clock.play();
        play_for(simulation, 10.0f);
        CHECK(simulation.time == Approx(10.0f).margin(0.02f));
        CHECK_FALSE(clock.behind());
        CHECK(simulation.max_gpu_ms_per_frame <= 6.0f + 1.7f + 1e-4f);
    }

    SECTION("the real time budget uses the best cost since the reset")
    {
        // Small per-frame chunks measure slow steps (the GPU clocks down); the budget keeps the
        // best cost it has seen, until a reset starts a new flow.
        simulation.gpu_ms = 0.5f;
        clock.set_settings(settings_with(SimulationClock::Mode::RealTime, 1.0f, 6.0f));
        clock.play();
        play_for(simulation, 1.0f);
        CHECK(clock.steps_per_frame_budget() == Approx(12.0f));
        simulation.gpu_ms = 2.0f;
        play_for(simulation, 30.0f);
        CHECK(clock.steps_per_frame_budget() == Approx(12.0f));
        simulation.reset();
        play_for(simulation, 1.0f);
        CHECK(clock.steps_per_frame_budget() == Approx(3.0f));
    }

    SECTION("behind has hysteresis")
    {
        // Just below the threshold: behind. Then a target that the same speed reaches within
        // the hysteresis band stays behind until it is clearly reached.
        simulation.gpu_ms = 1.0f;
        clock.set_settings(settings_with(SimulationClock::Mode::RealTime, 2.7f, 4.0f)); // can do 2.4x
        clock.play();
        play_for(simulation, 3.0f);
        REQUIRE(clock.behind());
        auto settings = clock.settings();
        settings.speed = 2.6f; // 2.4 / 2.6 = 0.92: inside the band, stays behind
        clock.set_settings(settings);
        play_for(simulation, 3.0f);
        CHECK(clock.behind());
        settings.speed = 2.0f; // reached
        clock.set_settings(settings);
        play_for(simulation, 3.0f);
        CHECK_FALSE(clock.behind());
    }

    SECTION("a stalled frame counts as at most MAX_FRAME_SECONDS")
    {
        simulation.gpu_ms = 0.01f;
        clock.set_settings(settings_with(SimulationClock::Mode::RealTime, 1.0f, 30.0f));
        clock.play();
        play_for(simulation, 0.5f);
        const float before = simulation.time;
        clock.tick(simulation, 5.0f); // the window was dragged for five seconds
        simulation.end_frame();
        play_for(simulation, 1.0f);
        CHECK(simulation.time - before == Approx(1.0f + SimulationClock::MAX_FRAME_SECONDS).margin(0.02f));
    }

    SECTION("a GPU that refuses chunks does not make the simulation race afterwards")
    {
        simulation.gpu_ms = 1.0f;
        clock.set_settings(settings_with(SimulationClock::Mode::RealTime, 1.0f, 6.0f));
        clock.play();
        play_for(simulation, 1.0f);
        simulation.refuse_frames = true;
        const float before = simulation.time;
        play_for(simulation, 1.0f);
        CHECK(simulation.time == before);
        simulation.refuse_frames = false;
        play_for(simulation, 1.0f / 60.0f);
        CHECK(simulation.time - before <= 7.0f * simulation.dt + 1e-5f); // one frame's budget, plus the carry
    }

    SECTION("accuracy runs back to back at the budget")
    {
        simulation.gpu_ms = 0.5f; // 6 ms budget: 12 steps per chunk
        clock.set_settings(settings_with(SimulationClock::Mode::Accuracy));
        clock.play();
        play_for(simulation, 2.0f);
        REQUIRE(simulation.runs.size() > 10);
        CHECK(clock.steps_per_submit() == 12u);
        for (const auto& run : simulation.runs) {
            CHECK(run.steps == 24u); // max(2 chunks, MIN_STEPS_PER_RUN)
            CHECK(run.chunks == 2u);
        }
        CHECK(simulation.frame_chunks.empty());
        CHECK(simulation.interpolation == 1.0f);
        // Independent of the wall clock: far faster than real time on this GPU.
        CHECK(clock.measured_speed() > 3.0f);
    }

    SECTION("accuracy without a timing uses the manual step counts")
    {
        auto settings = settings_with(SimulationClock::Mode::Accuracy, 1.0f, 0.0f);
        settings.manual_steps_per_submit = 3;
        settings.manual_steps_per_run = 9;
        clock.set_settings(settings);
        clock.play();
        play_for(simulation, 0.5f);
        REQUIRE_FALSE(simulation.runs.empty());
        for (const auto& run : simulation.runs) {
            CHECK(run.steps == 9u);
            CHECK(run.chunks == 3u);
        }
    }

    SECTION("the chunk size has hysteresis")
    {
        simulation.gpu_ms = 0.1f; // 6 ms budget: 60 steps, tolerance 60 / 8 = 7
        clock.set_settings(settings_with(SimulationClock::Mode::Accuracy));
        clock.play();
        play_for(simulation, 0.5f);
        REQUIRE(clock.steps_per_submit() == 60u);
        simulation.gpu_ms = 0.105f; // 57 steps: within the tolerance, keep 60
        play_for(simulation, 0.5f);
        CHECK(clock.steps_per_submit() == 60u);
        simulation.gpu_ms = 0.125f; // 48 steps: outside, resize
        play_for(simulation, 0.5f);
        CHECK(clock.steps_per_submit() == 48u);
    }

    SECTION("switching from real time to accuracy closes the open run first")
    {
        simulation.gpu_ms = 0.1f;
        clock.set_settings(settings_with(SimulationClock::Mode::RealTime));
        clock.play();
        while (!simulation.frame_run_open) {
            clock.tick(simulation, 1.0f / 60.0f);
            simulation.end_frame();
        }
        clock.set_settings(settings_with(SimulationClock::Mode::Accuracy));
        play_for(simulation, 0.5f); // the fake REQUIREs that no run is under way
        CHECK_FALSE(simulation.runs.empty());
    }

    SECTION("a pause closes the open real time run")
    {
        simulation.gpu_ms = 0.1f;
        clock.set_settings(settings_with(SimulationClock::Mode::RealTime));
        clock.play();
        while (!simulation.frame_run_open) {
            clock.tick(simulation, 1.0f / 60.0f);
            simulation.end_frame();
        }
        const size_t chunks = simulation.frame_chunks.size();
        const float shown = simulation.shown_time();
        clock.pause();
        play_for(simulation, 0.2f);
        CHECK_FALSE(simulation.is_advancing());
        CHECK(simulation.frame_chunks.size() == chunks); // closed without simulating further
        CHECK(simulation.shown_time() == shown); // and the picture holds still
        CHECK(clock.step(simulation)); // so a Step is not blocked
    }

    SECTION("pauses when the simulation cannot advance")
    {
        clock.set_settings(settings_with(SimulationClock::Mode::Accuracy));
        clock.play();
        simulation.ready = false;
        play_for(simulation, 0.1f);
        CHECK_FALSE(clock.playing());
        CHECK(simulation.runs.empty());
    }

    SECTION("step runs once while paused")
    {
        clock.set_settings(settings_with(SimulationClock::Mode::RealTime));
        CHECK(clock.step(simulation));
        CHECK(simulation.runs.size() == 1u);
        CHECK_FALSE(clock.step(simulation)); // still busy
        play_for(simulation, 0.5f);
        CHECK(simulation.runs.size() == 1u); // and it did not start playing
    }

    SECTION("real time continues smoothly from where a step left the simulation")
    {
        simulation.gpu_ms = 0.1f;
        clock.set_settings(settings_with(SimulationClock::Mode::RealTime));
        REQUIRE(clock.step(simulation));
        play_for(simulation, 0.5f);
        const float stepped = simulation.time;
        clock.play();
        play_for(simulation, 2.0f);
        CHECK(simulation.time - stepped == Approx(2.0f).margin(0.03f));
    }

    SECTION("a reset starts the presentation over")
    {
        simulation.gpu_ms = 1.0f;
        clock.set_settings(settings_with(SimulationClock::Mode::RealTime, 4.0f));
        clock.play();
        play_for(simulation, 2.0f);
        simulation.reset(); // takes effect with the next run, within REAL_TIME_RUN_CHUNKS frames
        clock.set_settings(settings_with(SimulationClock::Mode::RealTime, 0.25f));
        play_for(simulation, 4.0f);
        CHECK(simulation.time == Approx(1.0f).margin(0.05f));
        CHECK(simulation.shown_time() <= simulation.time + 1e-6f);
    }
}
