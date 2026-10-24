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

/// Stands in for the MPM solver. Simulated time moves when a chunk is submitted. A PerFrame
/// run submits its first chunk in advance() and one more per submit_frame(), and is done once
/// the last one is out. A Chained run is busy for one frame per chunk (the solver submits the
/// next chunk from the previous one's work-done callback, about once per frame). GPU time is
/// the chunk's steps times a fixed cost.
class FakeSimulation : public TimeSteppedSimulation {
public:
    float dt = 0.01f;
    float gpu_ms = 0.0f; // per step; 0 = not measured yet
    bool ready = true;
    float time = 0.0f;

    struct Run {
        uint32_t steps;
        uint32_t chunks;
        Submission submission;
    };
    std::vector<Run> runs;
    std::deque<uint32_t> pending; // chunks of the current run not submitted yet
    bool per_frame = false;
    bool chained_busy = false;

    float gpu_ms_this_frame = 0.0f;
    float max_gpu_ms_per_frame = 0.0f;
    float time_at_frame_start = 0.0f;
    std::vector<float> frame_advance; // simulated time per frame

    bool can_advance() const override { return ready; }
    bool is_advancing() const override { return per_frame ? !pending.empty() : chained_busy; }
    void advance(uint32_t steps, uint32_t chunks, Submission submission) override
    {
        REQUIRE(!is_advancing());
        REQUIRE(steps > 0);
        REQUIRE(chunks > 0);
        REQUIRE(chunks <= steps);
        runs.push_back({ steps, chunks, submission });
        pending.clear();
        for (uint32_t i = 0; i < chunks; i++)
            pending.push_back(uint32_t(uint64_t(i + 1) * steps / chunks - uint64_t(i) * steps / chunks));
        per_frame = submission == Submission::PerFrame;
        if (per_frame) {
            submit_next();
        } else {
            time += float(steps) * dt;
            chained_busy = true;
            gpu_ms_this_frame += gpu_ms * float(pending.front());
            pending.pop_front();
        }
    }
    void submit_frame() override
    {
        if (per_frame && !pending.empty())
            submit_next();
    }
    float time_step() const override { return dt; }
    float simulated_time() const override { return time; }
    float gpu_ms_per_step() const override { return gpu_ms; }
    void reset() override { time = 0.0f; }

    void submit_next()
    {
        time += float(pending.front()) * dt;
        gpu_ms_this_frame += gpu_ms * float(pending.front());
        pending.pop_front();
    }

    /// The frame boundary.
    void end_frame()
    {
        max_gpu_ms_per_frame = std::max(max_gpu_ms_per_frame, gpu_ms_this_frame);
        gpu_ms_this_frame = 0.0f;
        frame_advance.push_back(time - time_at_frame_start);
        time_at_frame_start = time;
        if (!per_frame && chained_busy) {
            if (pending.empty()) {
                chained_busy = false;
            } else {
                gpu_ms_this_frame += gpu_ms * float(pending.front()); // the next frame's chunk
                pending.pop_front();
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
        CHECK(simulation.time == 0.0f);
    }

    SECTION("real time follows the wall clock")
    {
        simulation.gpu_ms = 0.1f;
        clock.set_settings(settings_with(SimulationClock::Mode::RealTime));
        clock.play();
        play_for(simulation, 10.0f);
        // At most a frame or two of latency plus less than one step of leftover debt.
        CHECK(simulation.time == Approx(10.0f).margin(0.05f));
        CHECK(clock.measured_speed() == Approx(1.0f).margin(0.05f));
        CHECK_FALSE(clock.behind());
        // Every frame advances, by one frame's worth of steps: 1.67 at 60 fps and dt 10 ms, so
        // one or two - including the frames where one run ends and the next begins.
        for (size_t i = 1; i < simulation.frame_advance.size(); i++) {
            CHECK(simulation.frame_advance[i] >= 0.01f - 1e-5f);
            CHECK(simulation.frame_advance[i] <= 0.02f + 1e-5f);
        }
        for (const auto& run : simulation.runs) {
            CHECK(run.submission == TimeSteppedSimulation::Submission::PerFrame);
            CHECK(run.chunks <= SimulationClock::REAL_TIME_RUN_FRAMES);
        }
    }

    SECTION("real time scales with the speed")
    {
        simulation.gpu_ms = 0.1f;
        for (const float speed : { 0.25f, 2.0f, 4.0f }) {
            FakeSimulation s;
            s.gpu_ms = 0.1f;
            s.clock().set_settings(settings_with(SimulationClock::Mode::RealTime, speed));
            s.clock().play();
            play_for(s, 10.0f);
            CHECK(s.time == Approx(10.0f * speed).margin(0.05f * speed + 0.02f));
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
            CHECK(s.time == Approx(10.0f).margin(0.1f));
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
        // Never more than the budget per frame, and the runs stay bounded (no spiral).
        CHECK(simulation.max_gpu_ms_per_frame <= 4.0f + 1e-4f);
        for (const auto& run : simulation.runs)
            CHECK(run.steps <= 4u * SimulationClock::REAL_TIME_RUN_FRAMES);
    }

    SECTION("real time uses a fractional budget")
    {
        // 6 ms at 1.7 ms per step is 3.5 steps per frame, enough for 1x at dt 5 ms and 60 fps
        // (3.3 steps per frame) - but not if every chunk is rounded down to 3 steps.
        simulation.dt = 0.005f;
        simulation.gpu_ms = 1.7f;
        clock.set_settings(settings_with(SimulationClock::Mode::RealTime, 1.0f, 6.0f));
        clock.play();
        play_for(simulation, 10.0f);
        CHECK(simulation.time == Approx(10.0f).margin(0.1f));
        CHECK_FALSE(clock.behind());
        // A frame may exceed the budget by at most one step.
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
        clock.tick(simulation, 1.0f / 60.0f);
        simulation.end_frame();
        const float before = simulation.time;
        clock.tick(simulation, 5.0f); // the window was dragged for five seconds
        simulation.end_frame();
        play_for(simulation, 1.0f);
        CHECK(simulation.time - before == Approx(1.0f + SimulationClock::MAX_FRAME_SECONDS).margin(0.08f));
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
            CHECK(run.submission == TimeSteppedSimulation::Submission::Chained);
        }
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

    SECTION("a real time run under way finishes after a pause")
    {
        simulation.gpu_ms = 0.1f;
        clock.set_settings(settings_with(SimulationClock::Mode::RealTime));
        clock.play();
        play_for(simulation, 0.5f);
        while (!simulation.is_advancing() || simulation.pending.size() < 3) {
            clock.tick(simulation, 1.0f / 60.0f);
            simulation.end_frame();
        }
        const size_t runs = simulation.runs.size();
        clock.pause();
        play_for(simulation, 0.2f);
        CHECK_FALSE(simulation.is_advancing());
        CHECK(simulation.runs.size() == runs); // finished, but nothing new started
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

    SECTION("a reset drops the debt of the old run")
    {
        simulation.gpu_ms = 1.0f;
        clock.set_settings(settings_with(SimulationClock::Mode::RealTime, 4.0f));
        clock.play();
        play_for(simulation, 2.0f); // behind: debt at its cap
        simulation.reset();
        clock.set_settings(settings_with(SimulationClock::Mode::RealTime, 0.25f));
        play_for(simulation, 4.0f);
        CHECK(simulation.time == Approx(1.0f).margin(0.05f));
    }
}
