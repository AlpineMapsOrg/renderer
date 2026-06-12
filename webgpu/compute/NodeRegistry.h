/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Gerald Kimmersdorfer
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

#include "nodes/Node.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <webgpu/base/Context.h>

namespace webgpu_compute {
class NodeRegistry {
public:
    using NodeFactory = std::function<std::unique_ptr<nodes::Node>(webgpu::Context&)>;

    static NodeRegistry& instance();

    void register_node(const std::string& type_name, NodeFactory factory);
    [[nodiscard]] bool is_registered(const std::string& type_name) const;
    [[nodiscard]] std::vector<std::string> get_registered_types() const;

    // Creates a node of the given registered type. Asserts if the type is unknown.
    [[nodiscard]] std::unique_ptr<nodes::Node> create(const std::string& type_name, webgpu::Context& ctx) const;

    // Non-asserting variant for data-driven loading. Returns nullptr if the type is unknown.
    [[nodiscard]] std::unique_ptr<nodes::Node> try_create(const std::string& type_name, webgpu::Context& ctx) const;

private:
    NodeRegistry(); // registers all core compute nodes

    std::unordered_map<std::string, NodeFactory> m_factories;
};

} // namespace webgpu_compute
