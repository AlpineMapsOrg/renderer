/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Gerald Kimmersdorfer
 * Copyright (C) 2024 Patrick Komon
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

#include "../GpuHashMap.h"
#include "../GpuTileId.h"
#include "../GpuTileStorage.h"
#include "radix/tile.h"
#include <QByteArray>
#include <QObject>
#include <variant>
#include <vector>

namespace webgpu_engine::compute::nodes {

class Node;

using DataType = size_t;

/// datatypes that can be used with nodes have to be declared here.
using Data = std::variant<const std::vector<radix::tile::Id>*,
    const std::vector<QByteArray>*,
    TileStorageTexture*,
    GpuHashMap<radix::tile::Id, uint32_t, GpuTileId>*,
    webgpu::raii::RawBuffer<uint32_t>*,
    const webgpu::raii::TextureWithSampler*,
    const radix::geometry::Aabb<2, double>*,
    glm::uvec2>;

using SocketIndex = size_t;

// Get data type (DataType value) for specific C++ type
// adapted from https://stackoverflow.com/a/52303671
template <typename T, typename VariantT, std::size_t index = 0> static constexpr DataType data_type()
{
    static_assert(std::variant_size_v<VariantT> > index, "Type not found in variant");
    if constexpr (index == std::variant_size_v<VariantT>) {
        return index;
    } else if constexpr (std::is_same_v<std::variant_alternative_t<index, VariantT>, T>) {
        return index;
    } else {
        return data_type<T, VariantT, index + 1>();
    }
}

template <typename T> static constexpr DataType data_type() { return data_type<T, Data>(); }

class Socket {
public:
    enum class FlowDirection {
        INPUT,
        OUTPUT,
    };

protected:
    Socket(Node& node, const std::string& name, DataType type, FlowDirection direction);

public:
    [[nodiscard]] const Node& node() const;
    [[nodiscard]] Node& node();

    [[nodiscard]] const std::string& name() const;
    [[nodiscard]] DataType type() const;

    [[nodiscard]] FlowDirection direction() const { return m_direction; }

private:
    Node* m_node;
    std::string m_name;
    DataType m_type;
    FlowDirection m_direction;
};

class OutputSocket;
class InputSocket;

class InputSocket : public Socket {
    friend class OutputSocket;

public:
    InputSocket(Node& node, const std::string& name, DataType type);

    void connect(OutputSocket& output_socket);

    [[nodiscard]] bool is_socket_connected() const;
    [[nodiscard]] OutputSocket& connected_socket();
    [[nodiscard]] const OutputSocket& connected_socket() const;

    [[nodiscard]] Data get_connected_data();

private:
    OutputSocket* m_connected_socket = nullptr;
};

class OutputSocket : public Socket {
    friend class InputSocket;

    using OutputFunc = std::function<Data()>;

public:
    OutputSocket(Node& node, const std::string& name, DataType type, OutputFunc output_func);

    void connect(InputSocket& input_socket);

    [[nodiscard]] bool is_socket_connected() const;
    [[nodiscard]] std::vector<InputSocket*>& connected_sockets();
    [[nodiscard]] const std::vector<InputSocket*>& connected_sockets() const;

    [[nodiscard]] Data get_data();

private:
    void remove_connected_socket(InputSocket& input_socket);

private:
    OutputFunc m_output_func;
    std::vector<InputSocket*> m_connected_sockets = {};
};

class NodeRunFailureInfo {
public:
    NodeRunFailureInfo() = delete;
    NodeRunFailureInfo(const NodeRunFailureInfo&) = default;

    NodeRunFailureInfo(const Node& node, const std::string& message);

    [[nodiscard]] const Node& node() const;
    [[nodiscard]] const std::string& message() const;

private:
    const Node* m_node;
    std::string m_message;
};

/// Abstract base class for nodes.
///
/// Subclasses usually need to override methods run and get_output_data_impl.
///  - in run, subclass should
///     - get input data from connected nodes via get_input_data(size_t input_socket_index)
///     - do some calculations
///     - save results somewhere (e.g. class member)
///   - in get_output_data_impl
///     - return pointer to result
class Node : public QObject {
    Q_OBJECT

public:
    Node(const std::vector<InputSocket>& input_sockets, const std::vector<OutputSocket>& output_sockets);
    virtual ~Node() = default;

    [[nodiscard]] bool has_input_socket(const std::string& name) const;
    [[nodiscard]] InputSocket& input_socket(const std::string& name);
    [[nodiscard]] const InputSocket& input_socket(const std::string& name) const;

    [[nodiscard]] bool has_output_socket(const std::string& name) const;
    [[nodiscard]] OutputSocket& output_socket(const std::string& name);
    [[nodiscard]] const OutputSocket& output_socket(const std::string& name) const;

    [[nodiscard]] std::vector<InputSocket>& input_sockets();
    [[nodiscard]] const std::vector<InputSocket>& input_sockets() const;

    [[nodiscard]] std::vector<OutputSocket>& output_sockets();
    [[nodiscard]] const std::vector<OutputSocket>& output_sockets() const;

    /// Returns running time of the last execution of this node in ms.
    [[nodiscard]] int get_last_run_duration_in_ms() const;

    [[nodiscard]] bool is_enabled() const;
    void set_enabled(bool enabled);

    [[nodiscard]] bool is_running() const;

public slots:
    void run();

signals:
    void run_started();
    void run_completed();
    void run_failed(NodeRunFailureInfo failed_info);

protected:
    /// Override to implement node behavior.
    /// Call either signal run_completed() or run_failed(info) to indicate completion or failure of run.
    /// Postcondition:
    ///   - get_output_data(output-index) returns result
    virtual void run_impl() = 0;

protected:
    [[nodiscard]] Data get_output_data(const std::string& output_socket_name);
    [[nodiscard]] Data get_input_data(const std::string& input_socket_name);

private:
    std::vector<InputSocket> m_input_sockets;
    std::vector<OutputSocket> m_output_sockets;

    std::chrono::high_resolution_clock::time_point m_last_run_started;
    std::chrono::high_resolution_clock::time_point m_last_run_finished;
    int m_last_run_duration_in_ms = 0;

    bool m_enabled = true;
    bool m_is_running = false;
};

} // namespace webgpu_engine::compute::nodes
