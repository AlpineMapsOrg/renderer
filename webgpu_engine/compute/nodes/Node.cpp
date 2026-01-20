/*****************************************************************************
 * weBIGeo
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

#include "Node.h"

#include <QDebug>

namespace webgpu_engine::compute::nodes {

Socket::Socket(Node& node, const std::string& name, DataType type, FlowDirection direction)
    : m_node(&node)
    , m_name(name)
    , m_type(type)
    , m_direction(direction)
{
}

const std::string& Socket::name() const { return m_name; }
DataType Socket::type() const { return m_type; }
const Node& Socket::node() const { return *m_node; }
Node& Socket::node() { return *m_node; }

InputSocket::InputSocket(Node& node, const std::string& name, DataType type)
    : Socket(node, name, type, Socket::FlowDirection::INPUT)
{
}

void InputSocket::connect(OutputSocket& output_socket)
{
    assert(type() == output_socket.type());
    if (is_socket_connected()) {
        m_connected_socket->remove_connected_socket(*this);
    }
    m_connected_socket = &output_socket;
    output_socket.m_connected_sockets.push_back(this);
}
bool InputSocket::is_socket_connected() const { return m_connected_socket != nullptr; }
OutputSocket& InputSocket::connected_socket() { return *m_connected_socket; }
const OutputSocket& InputSocket::connected_socket() const { return *m_connected_socket; }

Data InputSocket::get_connected_data()
{
    assert(m_connected_socket != nullptr);
    return m_connected_socket->get_data();
}

OutputSocket::OutputSocket(Node& node, const std::string& name, DataType type, OutputFunc output_func)
    : Socket(node, name, type, Socket::FlowDirection::OUTPUT)
    , m_output_func(output_func)
{
}

void OutputSocket::connect(InputSocket& input_socket)
{
    assert(type() == input_socket.type());
    m_connected_sockets.push_back(&input_socket);
    input_socket.m_connected_socket = this;
}
bool OutputSocket::is_socket_connected() const { return !m_connected_sockets.empty(); }
std::vector<InputSocket*>& OutputSocket::connected_sockets() { return m_connected_sockets; }
const std::vector<InputSocket*>& OutputSocket::connected_sockets() const { return m_connected_sockets; }

Data OutputSocket::get_data()
{
    Data output = m_output_func();
    assert(output.index() == type()); // implementation returned correct type
    return output;
}

void OutputSocket::remove_connected_socket(InputSocket& input_socket)
{
    auto it = std::find(m_connected_sockets.begin(), m_connected_sockets.end(), &input_socket);
    assert(it != m_connected_sockets.end());
    m_connected_sockets.erase(it);
}

NodeRunFailureInfo::NodeRunFailureInfo(const Node& node, const std::string& message)
    : m_node(&node)
    , m_message(message)
{
}

const std::string& NodeRunFailureInfo::message() const { return m_message; }

const Node& NodeRunFailureInfo::node() const { return *m_node; }

Node::Node(const std::vector<InputSocket>& input_sockets, const std::vector<OutputSocket>& output_sockets)
    : m_input_sockets(input_sockets)
    , m_output_sockets(output_sockets)
{

    connect(this, &webgpu_engine::compute::nodes::Node::run_started, this, [this]() { this->m_last_run_started = std::chrono::high_resolution_clock::now(); });
    connect(
        this, &webgpu_engine::compute::nodes::Node::run_completed, this, [this]() { this->m_last_run_finished = std::chrono::high_resolution_clock::now(); });
    connect(this, &webgpu_engine::compute::nodes::Node::run_completed, this, [this]() {
        if (is_enabled()) {
            m_last_run_duration_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(m_last_run_finished - m_last_run_started).count();
            m_is_running = false;
            qDebug() << " node execution took " << this->m_last_run_duration_in_ms << "ms";
        }
    });
}

void Node::run()
{
    if (m_enabled) {
        emit run_started();
        m_is_running = true;
        run_impl();
    } else {
        emit run_completed();
    }
}

bool Node::has_input_socket(const std::string& name) const
{
    const auto it = std::find_if(m_input_sockets.begin(), m_input_sockets.end(), [&name](const InputSocket& socket) { return socket.name() == name; });
    return it != m_input_sockets.end();
}

InputSocket& Node::input_socket(const std::string& name)
{
    const auto it = std::find_if(m_input_sockets.begin(), m_input_sockets.end(), [&name](const InputSocket& socket) { return socket.name() == name; });
    if (it == m_input_sockets.end()) {
        // TODO throw runtime error
        qWarning() << "input socket with name '" << name << "' not found";
    }
    return *it;
}

const InputSocket& Node::input_socket(const std::string& name) const
{
    const auto it = std::find_if(m_input_sockets.begin(), m_input_sockets.end(), [&name](const InputSocket& socket) { return socket.name() == name; });
    if (it == m_input_sockets.end()) {
        // TODO throw runtime error
        qFatal() << "input socket with name '" << name << "' not found";
    }
    return *it;
}

bool Node::has_output_socket(const std::string& name) const
{
    const auto it = std::find_if(m_output_sockets.begin(), m_output_sockets.end(), [&name](const OutputSocket& socket) { return socket.name() == name; });
    return it != m_output_sockets.end();
}

OutputSocket& Node::output_socket(const std::string& name)
{
    const auto it = std::find_if(m_output_sockets.begin(), m_output_sockets.end(), [&name](const OutputSocket& socket) { return socket.name() == name; });
    if (it == m_output_sockets.end()) {
        // TODO throw runtime error
        qFatal() << "output socket with name '" << name << "' not found";
    }
    return *it;
}

const OutputSocket& Node::output_socket(const std::string& name) const
{
    const auto it = std::find_if(m_output_sockets.begin(), m_output_sockets.end(), [&name](const OutputSocket& socket) { return socket.name() == name; });
    if (it == m_output_sockets.end()) {
        qFatal() << "output socket with name '" << name << "' not found";
    }
    return *it;
}

std::vector<InputSocket>& Node::input_sockets() { return m_input_sockets; }

const std::vector<InputSocket>& Node::input_sockets() const { return m_input_sockets; }

std::vector<OutputSocket>& Node::output_sockets() { return m_output_sockets; }

const std::vector<OutputSocket>& Node::output_sockets() const { return m_output_sockets; }

Data Node::get_output_data(const std::string& output_socket_name)
{
    if (!has_output_socket(output_socket_name)) {
        qFatal() << "output socket with name '" << output_socket_name << "' not found";
        return {};
    }
    return output_socket(output_socket_name).get_data();
}

Data Node::get_input_data(const std::string& input_socket_name)
{
    if (!has_input_socket(input_socket_name)) {
        qFatal() << "input socket with name '" << input_socket_name << "' not found";
        return {};
    }
    Data result = input_socket(input_socket_name).connected_socket().get_data();
    return result;
}

int Node::get_last_run_duration_in_ms() const { return m_last_run_duration_in_ms; }

bool Node::is_enabled() const { return m_enabled; }

void Node::set_enabled(bool enabled) { m_enabled = enabled; }

bool Node::is_running() const { return m_is_running; }

} // namespace webgpu_engine::compute::nodes
