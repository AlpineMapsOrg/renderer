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

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <vector>
#include <webgpu/base/RenderResourceRegistry.h>

using webgpu::PipelineRegistration;
using webgpu::RenderResourceRegistry;

// No shaders or bind group layouts are registered, so recreate_all() only runs the pipeline
// factories and never touches the (null) device.
TEST_CASE("webgpu::RenderResourceRegistry pipeline registrations")
{
    RenderResourceRegistry registry;
    int calls_a = 0;
    int calls_b = 0;
    const auto count = [](int& calls) { return [&calls](WGPUDevice, const RenderResourceRegistry&) { calls++; }; };

    SECTION("a live registration is called by recreate_all")
    {
        PipelineRegistration a = registry.register_pipeline(count(calls_a));
        registry.recreate_all(nullptr);
        registry.recreate_all(nullptr);
        CHECK(calls_a == 2);
    }

    SECTION("a destroyed registration is no longer called")
    {
        PipelineRegistration a = registry.register_pipeline(count(calls_a));
        {
            PipelineRegistration b = registry.register_pipeline(count(calls_b));
            registry.recreate_all(nullptr);
        }
        registry.recreate_all(nullptr);
        CHECK(calls_a == 2);
        CHECK(calls_b == 1);
    }

    SECTION("reset() unregisters")
    {
        PipelineRegistration a = registry.register_pipeline(count(calls_a));
        a.reset();
        registry.recreate_all(nullptr);
        CHECK(calls_a == 0);
    }

    SECTION("moving a registration keeps exactly one registered factory")
    {
        PipelineRegistration a = registry.register_pipeline(count(calls_a));
        PipelineRegistration moved = std::move(a);
        a.reset(); // moved-from handle must not unregister the factory
        registry.recreate_all(nullptr);
        CHECK(calls_a == 1);

        std::vector<PipelineRegistration> handles;
        handles.push_back(std::move(moved));
        registry.recreate_all(nullptr);
        CHECK(calls_a == 2);

        handles.clear();
        registry.recreate_all(nullptr);
        CHECK(calls_a == 2);
    }

    SECTION("assigning a new registration unregisters the old one")
    {
        PipelineRegistration a = registry.register_pipeline(count(calls_a));
        a = registry.register_pipeline(count(calls_b));
        registry.recreate_all(nullptr);
        CHECK(calls_a == 0);
        CHECK(calls_b == 1);
    }

    SECTION("factories run in registration order")
    {
        std::vector<char> order;
        PipelineRegistration b = registry.register_pipeline([&order](WGPUDevice, const RenderResourceRegistry&) { order.push_back('b'); });
        PipelineRegistration a = registry.register_pipeline([&order](WGPUDevice, const RenderResourceRegistry&) { order.push_back('a'); });
        CHECK(order.empty()); // no device yet, so nothing runs on registration
        registry.recreate_all(nullptr);
        CHECK(order == std::vector<char> { 'b', 'a' });
    }
}

TEST_CASE("webgpu::PipelineRegistration may outlive its registry")
{
    int calls = 0;
    PipelineRegistration registration;
    {
        RenderResourceRegistry registry;
        registration = registry.register_pipeline([&calls](WGPUDevice, const RenderResourceRegistry&) { calls++; });
        registry.recreate_all(nullptr);
    }
    registration.reset(); // must not touch the destroyed registry
    CHECK(calls == 1);
}
