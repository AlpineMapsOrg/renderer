/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2025 Patrick Komon
 * Copyright (C) 2025 Gerald Kimmersdorfer
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

#include "NodeGraphPanel.h"

#include "ImGuiManager.h"
#include "nodes/NodeRendererFactory.h"
#include <IconsFontAwesome5.h>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <algorithm>
#include <imgui.h>
#include <imgui_internal.h>
#include <imnodes.h>
#include <imnodes_internal.h>
#include <qDebug>
#include <webgpu/compute/NodeGraphSerialization.h>
#include <webgpu/compute/NodeRegistry.h>
#include <webgpu/engine/Context.h>

namespace webgpu_app {
namespace nodes = webgpu_compute::nodes;

NodeGraphPanel::NodeGraphPanel(webgpu_engine::Context* context)
    : m_context(context)
    , m_presets({
          { "Snow", ":/graphs/snow.json" },
          { "Avalanche simulation", ":/graphs/avalanche_simulation.json" },
          { "Avalanche simulation (with exports)", ":/graphs/avalanche_simulation_with_exports.json" },
          { "Iterative simulation (WIP)", ":/graphs/iterative_simulation_wip.json" },
      })
{
}

void NodeGraphPanel::ready() { load_preset(":/graphs/avalanche_simulation.json"); }

void NodeGraphPanel::attach_graph(std::unique_ptr<nodes::NodeGraph> graph)
{
    m_owned_graph = std::move(graph);
    m_node_graph = m_owned_graph.get();

    QObject::connect(m_node_graph, &nodes::NodeGraph::run_completed, m_context, [this](webgpu_compute::GraphRunContext) {
        if (!m_pending_first_run_notice.empty()) {
            m_notice_state = { true, std::move(m_pending_first_run_notice) };
            m_pending_first_run_notice.clear();
        }
        m_context->request_redraw();
    });
    QObject::connect(m_node_graph, &nodes::NodeGraph::run_failed, m_context, [this](nodes::GraphRunFailureInfo info) {
        qWarning() << "graph run failed. " << info.node_name() << ": " << info.node_run_failure_info().message();
        m_error_state.text = "Execution of pipeline failed.\n\nNode \"" + info.node_name() + "\" reported \"" + info.node_run_failure_info().message() + "\"";
        m_error_state.should_open = true;
        m_context->request_redraw();
    });

    init(*m_node_graph);
}

void NodeGraphPanel::load_preset(const std::string& resource_path)
{
    QFile file(QString::fromStdString(resource_path));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_error_state.text = "Cannot open preset resource:\n" + resource_path;
        m_error_state.should_open = true;
        return;
    }
    const QByteArray data = file.readAll();
    file.close();
    import_graph_json(data, resource_path);
}

void NodeGraphPanel::new_graph() { attach_graph(std::make_unique<nodes::NodeGraph>()); }

void NodeGraphPanel::import_graph_json(const QByteArray& data, const std::string& source_name)
{
    QJsonParseError parse_err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parse_err);
    if (doc.isNull()) {
        m_error_state.text = "Failed to parse \"" + source_name + "\":\n" + parse_err.errorString().toStdString();
        m_error_state.should_open = true;
        return;
    }
    if (!doc.isObject()) {
        m_error_state.text = "Invalid graph file \"" + source_name + "\": JSON root is not an object";
        m_error_state.should_open = true;
        return;
    }

    auto result = webgpu_compute::nodes::deserialize_node_graph(doc.object(), m_context->webgpu_ctx());
    if (!result) {
        m_error_state.text = "Failed to load \"" + source_name + "\":\n" + result.error();
        m_error_state.should_open = true;
        return;
    }

    const QJsonObject root = doc.object();
    const QJsonObject ui_nodes = root["ui"].toObject()["nodes"].toObject();

    const QString notice = root["first_run_notice"].toString();
    m_pending_first_run_notice = notice.isEmpty() ? std::string{} : notice.toStdString();

    attach_graph(std::move(*result));

    if (!ui_nodes.isEmpty()) {
        for (auto& [name, renderer] : m_node_renderers) {
            const QString key = QString::fromStdString(name);
            if (ui_nodes.contains(key))
                renderer->deserialize_ui(ui_nodes[key].toObject());
        }
        m_force_node_positions_on_next_frame = true;
    } else {
        const ImVec2 center(m_window_size.x * 0.5f, m_window_size.y * 0.5f);
        for (auto& [nodePtr, nr] : m_node_renderers_by_node)
            nr->set_position(center);
        m_force_node_positions_on_next_frame = true;
        m_pending_auto_layout = true;
    }
}

void NodeGraphPanel::render_open_dialog()
{
    std::vector<std::string> open_paths;
    if (ImGuiManager::FilePicker("open_graph_dialog", "Load Graph", "Graph files{.json}", m_open_dialog_wants_open, open_paths)) {
        QFile file(QString::fromStdString(open_paths[0]));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QByteArray data = file.readAll();
            file.close();
            import_graph_json(data, open_paths[0]);
        } else {
            m_error_state.text = "Cannot open file:\n" + open_paths[0];
            m_error_state.should_open = true;
        }
    }
    m_open_dialog_wants_open = false;
}

void NodeGraphPanel::init(nodes::NodeGraph& node_graph)
{
    m_node_renderers.clear();
    m_node_renderers_by_node.clear();
    m_links.clear();

    m_node_graph = &node_graph;
    auto& nodes = m_node_graph->get_nodes();
    for (auto& [name, node] : nodes) {
        auto renderer = NodeRendererFactory::create(name, *node.get());
        m_node_renderers.emplace(name, std::move(renderer));
        m_node_renderers_by_node.emplace(node.get(), m_node_renderers.at(name).get());
    }

    rebuild_socket_id_maps();
    rebuild_links();
}

static std::string type_to_default_name(const std::string& type_name)
{
    // "ComputeSnowNode" -> "compute_snow_node"
    std::string result;
    for (size_t i = 0; i < type_name.size(); ++i) {
        if (i > 0 && std::isupper((unsigned char)type_name[i]))
            result += '_';
        result += (char)std::tolower((unsigned char)type_name[i]);
    }
    return result;
}

static std::string generate_node_name(const std::string& type_base, const webgpu_compute::nodes::NodeGraph* graph)
{
    for (int n = 1;; ++n) {
        std::string candidate = type_base + "_" + std::to_string(n);
        if (!graph->exists_node(candidate))
            return candidate;
    }
}

void NodeGraphPanel::render_add_node_popup()
{
    static const char* POPUP_ID = "Add Node";

    if (m_open_add_node_request) {
        if (m_registered_node_types.empty())
            m_registered_node_types = webgpu_compute::NodeRegistry::instance().get_registered_types();
        ImGui::OpenPopup(POPUP_ID);
        m_open_add_node_request = false;
    }

    if (!m_open_add_node_modal)
        ImGui::SetNextWindowPos(m_add_node_popup_pos, ImGuiCond_Appearing);
    const bool open = m_open_add_node_modal ? ImGui::BeginPopupModal(POPUP_ID, nullptr, ImGuiWindowFlags_AlwaysAutoResize) : ImGui::BeginPopup(POPUP_ID);
    if (!open)
        return;

    const auto& types = m_registered_node_types;
    ImGui::SetNextItemWidth(260.0f);
    if (ImGui::BeginCombo("Type", types[m_add_node_selected_idx].c_str())) {
        for (int i = 0; i < (int)types.size(); ++i) {
            const bool sel = (i == m_add_node_selected_idx);
            if (ImGui::Selectable(types[i].c_str(), sel))
                m_add_node_selected_idx = i;
            if (sel)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    const bool add_pressed = ImGui::Button("Add") || ImGui::IsKeyPressed(ImGuiKey_Enter, false);
    if (m_open_add_node_modal) {
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
    }

    if (add_pressed) {
        const std::string type_name = types[m_add_node_selected_idx];
        const std::string name = generate_node_name(type_to_default_name(type_name), m_node_graph);
        auto node = webgpu_compute::NodeRegistry::instance().try_create(type_name, m_context->webgpu_ctx());
        if (node) {
            m_node_graph->add_node(name, std::move(node));
            auto renderer = NodeRendererFactory::create(name, m_node_graph->get_node(name));
            ImVec2 pan = ImNodes::EditorContextGetPanning();
            ImVec2 pos;
            if (!m_open_add_node_modal)
                pos = ImVec2(m_add_node_popup_pos.x - m_canvas_origin.x - pan.x, m_add_node_popup_pos.y - m_canvas_origin.y - pan.y);
            else
                pos = ImVec2(m_window_size.x * 0.5f - m_canvas_origin.x - pan.x, m_window_size.y * 0.5f - m_canvas_origin.y - pan.y);
            renderer->set_position(pos);
            m_node_renderers_by_node.emplace(&m_node_graph->get_node(name), renderer.get());
            m_node_renderers.emplace(name, std::move(renderer));
            rebuild_socket_id_maps();
            m_force_node_positions_on_next_frame = true;
        }
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
    if (!ImGui::IsPopupOpen(POPUP_ID))
        m_open_add_node_modal = false;
}

QByteArray NodeGraphPanel::export_graph_json() const
{
    QJsonObject root = webgpu_compute::nodes::serialize_node_graph(*m_node_graph);

    QJsonObject ui_nodes;
    for (const auto& [name, renderer] : m_node_renderers)
        ui_nodes[QString::fromStdString(name)] = renderer->serialize_ui();
    QJsonObject ui;
    ui["nodes"] = ui_nodes;
    root["ui"] = ui;

    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

void NodeGraphPanel::render_save_dialog()
{
    std::vector<std::string> save_paths;
    if (ImGuiManager::FilePicker("save_graph_dialog",
            "Save Graph",
            "Graph files{.json}",
            m_save_dialog_wants_open,
            save_paths,
            false,
            ".",
            ImGuiManager::FilePickerMode::Save,
            "graph.json")) {
        QFile file(QString::fromStdString(save_paths[0]));
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(export_graph_json());
            file.close();
            ImGuiManager::finalize_save(save_paths[0]);
        } else {
            m_error_state.text = "Failed to open file for writing:\n" + save_paths[0];
            m_error_state.should_open = true;
        }
    }
    m_save_dialog_wants_open = false;
}

void NodeGraphPanel::calculate_window_size()
{
    if (!ImGui::GetCurrentContext()) {
        m_window_size = ImVec2(0.0f, 0.0f);
        return;
    }
    m_window_size = ImGui::GetIO().DisplaySize;
    m_window_size.x -= 430;
}

void NodeGraphPanel::calculate_auto_layout()
{
    m_target_layout.clear();

    // Step 1: Identify root nodes (no inputs) and enqueue them at x = 0
    std::queue<std::pair<NodeRenderer*, int>> queue;
    for (auto& [name, nr] : m_node_renderers) {
        nodes::Node* node = nr->get_node();
        if (node->input_sockets().empty()) {
            int x = 0;
            m_target_layout[node] = ImVec2(x, 0);
            queue.emplace(nr.get(), x);
        }
    }

    // Step 2: Perform BFS to assign x-layer positions based on the maximum distance from root nodes
    while (!queue.empty()) {
        auto [nr, current_x] = queue.front();
        queue.pop();

        nodes::Node* current_node = nr->get_node();
        const auto& outputs = current_node->output_sockets();
        for (const auto& os : outputs) {
            for (auto* conn : os.connected_sockets()) {
                nodes::Node* target_node = &conn->node();
                NodeRenderer* target = m_node_renderers_by_node[target_node];

                int x = current_x + 1;
                m_target_layout[target_node] = ImVec2(x, 0);
                queue.emplace(target, x);
            }
        }
    }

    // Step 3: Group nodes by x-layer and assign sequential y-indices
    std::unordered_map<int, std::vector<NodeRenderer*>> x_buckets;
    x_buckets.reserve(m_node_renderers.size());

    for (auto& [name, nr] : m_node_renderers) {
        int x = (int)m_target_layout[nr->get_node()].x;
        x_buckets[x].push_back(nr.get());
    }

    for (auto& [x, vec] : x_buckets) {
        int y = 0;
        for (auto* r : vec)
            m_target_layout[r->get_node()] = ImVec2((float)x, (float)y++);
    }

    // Step 4: Compute pixel layout. Determine column widths, x-offsets, and center vertically
    std::vector<int> cols;
    cols.reserve(x_buckets.size());
    for (auto& kv : x_buckets)
        cols.push_back(kv.first);
    std::sort(cols.begin(), cols.end());

    std::unordered_map<int, float> col_width;
    col_width.reserve(cols.size());

    for (int cx : cols) {
        float wmax = 0.f;
        for (NodeRenderer* r : x_buckets[cx]) {
            ImVec2 sz = r->get_size();
            wmax = std::max(wmax, sz.x);
        }
        col_width[cx] = wmax;
    }

    std::unordered_map<int, float> col_x_offset;
    col_x_offset.reserve(cols.size());

    float x_cursor = 0.f;
    for (size_t i = 0; i < cols.size(); ++i) {
        int cx = cols[i];
        if (i == 0)
            col_x_offset[cx] = x_cursor;
        else
            col_x_offset[cx] = (x_cursor += col_width[cols[i - 1]] + m_initial_node_spacing.x);
    }

    float frame_height = 0.f;
    for (int cx : cols) {
        float hsum = 0.f;
        for (NodeRenderer* r : x_buckets[cx]) {
            ImVec2 sz = r->get_size();
            hsum += sz.y + m_initial_node_spacing.y;
        }
        if (!x_buckets[cx].empty())
            hsum -= m_initial_node_spacing.y;
        frame_height = std::max(frame_height, hsum);
    }

    for (int cx : cols) {
        float column_height = 0.f;
        for (NodeRenderer* r : x_buckets[cx])
            column_height += r->get_size().y + m_initial_node_spacing.y;

        if (!x_buckets[cx].empty())
            column_height -= m_initial_node_spacing.y;

        float y_cursor = (frame_height - column_height) * 0.5f;

        for (NodeRenderer* r : x_buckets[cx]) {
            ImVec2 sz = r->get_size();
            m_target_layout[r->get_node()] = ImVec2(col_x_offset[cx], y_cursor);
            y_cursor += sz.y + m_initial_node_spacing.y;
        }
    }

    center_target_layout();
}

void NodeGraphPanel::recenter_graph()
{
    m_target_layout.clear();
    for (auto& [nodePtr, nr] : m_node_renderers_by_node)
        m_target_layout[nodePtr] = nr->get_position();
    center_target_layout();
    for (auto& [nodePtr, pos] : m_target_layout)
        m_node_renderers_by_node[nodePtr]->set_position(pos);
    m_force_node_positions_on_next_frame = true;
}

void NodeGraphPanel::reset_graph_layout()
{
    calculate_auto_layout();
    for (auto& [nodePtr, pos] : m_target_layout)
        m_node_renderers_by_node[nodePtr]->set_position(pos);
    m_force_node_positions_on_next_frame = true;
}

void NodeGraphPanel::center_target_layout()
{
    // Get AABB of target layout
    ImVec4 aabb(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (auto& [nodePtr, pos] : m_target_layout) {
        ImVec2 s = m_node_renderers_by_node[nodePtr]->get_size();
        aabb.x = std::min(aabb.x, pos.x); // minX
        aabb.y = std::min(aabb.y, pos.y); // minY
        aabb.z = std::max(aabb.z, pos.x + s.x); // maxX
        aabb.w = std::max(aabb.w, pos.y + s.y); // maxY
    }
    float graph_width = aabb.z - aabb.x;
    float graph_height = aabb.w - aabb.y;
    float offset_x = (m_window_size.x - graph_width) * 0.5f - aabb.x;
    float offset_y = (m_window_size.y - graph_height) * 0.5f - aabb.y;
    // Apply offset to target layout
    for (auto& [nodePtr, pos] : m_target_layout) {
        pos.x += offset_x;
        pos.y += offset_y;
    }
}

void NodeGraphPanel::push_style()
{
    // Always use transparent grid background
    ImNodes::PushColorStyle(ImNodesCol_GridBackground, IM_COL32(50, 50, 50, 0));

    if (m_render_mode == GraphRenderingMode::Default) {
        ImNodes::PushColorStyle(ImNodesCol_GridLine, IM_COL32(200, 200, 200, 40)); // light gray
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg)); // default ImGui bg

    } else if (m_render_mode == GraphRenderingMode::Transparent) {
        ImNodes::PushColorStyle(ImNodesCol_GridLine, IM_COL32(200, 200, 200, 40));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // fully transparent

    } else if (m_render_mode == GraphRenderingMode::White) {
        ImNodes::PushColorStyle(ImNodesCol_GridLine, IM_COL32(200, 200, 200, 40));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    }
}

void NodeGraphPanel::pop_style()
{
    ImGui::PopStyleColor(); // ImGui window background
    ImNodes::PopColorStyle(); // Grid line
    ImNodes::PopColorStyle(); // Grid background
}

void NodeGraphPanel::draw()
{
    if (m_pending_preset_path) {
        load_preset(*m_pending_preset_path);
        m_pending_preset_path.reset();
        m_context->request_redraw();
    }

    ImGuiManager::FloatingToggleButton("###ToggleGraphRenderer", ICON_FA_NETWORK_WIRED, "Toggle compute graph editor", &m_editor_visible);
    render_error_modal();
    render_first_run_notice_modal();
    render_save_dialog();
    render_open_dialog();

    if (!m_editor_visible)
        return;

    calculate_window_size();

    if (m_pending_auto_layout) {
        const bool all_measured = std::all_of(m_node_renderers.begin(), m_node_renderers.end(), [](const auto& kv) { return kv.second->is_size_known(); });
        if (all_measured) {
            calculate_auto_layout();
            for (auto& [nodePtr, pos] : m_target_layout)
                m_node_renderers_by_node[nodePtr]->set_position(pos);
            m_force_node_positions_on_next_frame = true;
            m_pending_auto_layout = false;
        }
    }

    push_style();

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(m_window_size, ImGuiCond_Always);

    ImGui::Begin("Compute Graph Editor",
        nullptr,
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus
            | ImGuiWindowFlags_NoTitleBar);

    render_menu();

    m_canvas_origin = ImGui::GetCursorScreenPos();
    if (m_use_small_font && ImGuiManager::s_node_font)
        ImGui::PushFont(ImGuiManager::s_node_font);
    ImNodes::BeginNodeEditor();

    // NOTE: This is a hack to disable interactions when the cursor is inside the settings panel.
    // which is sadly necessary because imnodes doesnt respect the Imguis z order
    {
        ImGuiWindow* settings_win = ImGui::FindWindowByName("Node Settings");
        if (settings_win && settings_win->Rect().Contains(ImGui::GetIO().MousePos)) {
            GImNodes->MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
            GImNodes->LeftMouseClicked = false;
            GImNodes->LeftMouseDragging = false;
        }
    }

    // draw nodes
    const bool apply_positions = m_force_node_positions_on_next_frame;
    m_force_node_positions_on_next_frame = false;
    for (auto& [name, node_renderer] : m_node_renderers) {
        node_renderer->render(apply_positions);
    }

    // draw links
    for (size_t i = 0; i < m_links.size(); ++i) {
        const auto& [input_attr_id, output_attr_id] = m_links[i];
        if (auto it = m_input_socket_by_id.find(input_attr_id); it != m_input_socket_by_id.end()) {
            const ImU32 color = NodeRenderer::pin_color_for_type(it->second->type());
            ImNodes::PushColorStyle(ImNodesCol_Link, color);
            ImNodes::PushColorStyle(ImNodesCol_LinkHovered, color);
            ImNodes::PushColorStyle(ImNodesCol_LinkSelected, color);
            ImNodes::Link(int(i), input_attr_id, output_attr_id);
            ImNodes::PopColorStyle();
            ImNodes::PopColorStyle();
            ImNodes::PopColorStyle();
        } else {
            ImNodes::Link(int(i), input_attr_id, output_attr_id);
        }
    }

    ImNodes::MiniMap(0.1f, ImNodesMiniMapLocation_BottomRight);
    ImNodes::EndNodeEditor();
    if (m_use_small_font && ImGuiManager::s_node_font)
        ImGui::PopFont();

    int start_attr_id, end_attr_id;
    if (ImNodes::IsLinkCreated(&start_attr_id, &end_attr_id)) {
        nodes::OutputSocket* output_socket = nullptr;
        nodes::InputSocket* input_socket = nullptr;

        if (m_output_socket_by_id.count(start_attr_id) && m_input_socket_by_id.count(end_attr_id)) {
            output_socket = m_output_socket_by_id.at(start_attr_id);
            input_socket = m_input_socket_by_id.at(end_attr_id);
        } else if (m_output_socket_by_id.count(end_attr_id) && m_input_socket_by_id.count(start_attr_id)) {
            output_socket = m_output_socket_by_id.at(end_attr_id);
            input_socket = m_input_socket_by_id.at(start_attr_id);
        }

        if (output_socket && input_socket && output_socket->type() == input_socket->type()) {
            input_socket->connect(*output_socket);
            m_node_graph->connect_node_signals_and_slots();
            rebuild_links();
        }
    }

    ImGui::End();

    pop_style();

    render_settings_panel();

    for (auto& [name, node_renderer] : m_node_renderers) {
        node_renderer->render_dialogs();
    }

    poll_keyboard_shortcuts();
    render_add_node_popup();
}

void NodeGraphPanel::poll_keyboard_shortcuts()
{
    if (!ImGui::GetIO().WantTextInput) {
        const bool alt = ImGui::GetIO().KeyAlt;
        const bool shift = ImGui::GetIO().KeyShift;
        if (alt && ImGui::IsKeyPressed(ImGuiKey_M, false))
            m_render_mode = static_cast<GraphRenderingMode>((static_cast<int>(m_render_mode) + 1) % 3);
        if (alt && ImGui::IsKeyPressed(ImGuiKey_F, false))
            reset_graph_layout();
        if (alt && ImGui::IsKeyPressed(ImGuiKey_C, false))
            recenter_graph();
        if (shift && ImGui::IsKeyPressed(ImGuiKey_R, false))
            m_node_graph->run();
        if (ImGui::IsKeyPressed(ImGuiKey_Delete))
            delete_selected_nodes();
        if (shift && ImGui::IsKeyPressed(ImGuiKey_A, false)) {
            m_add_node_popup_pos = ImGui::GetMousePos();
            m_open_add_node_modal = false;
            m_open_add_node_request = true;
        }
    }
}

void NodeGraphPanel::delete_selected_nodes()
{
    const int num_selected = ImNodes::NumSelectedNodes();
    if (num_selected == 0)
        return;

    std::vector<int> selected_ids(num_selected);
    ImNodes::GetSelectedNodes(selected_ids.data());

    std::vector<std::string> to_delete;
    for (int node_id : selected_ids) {
        for (auto& [name, renderer] : m_node_renderers) {
            if (renderer->get_node_id() == node_id) {
                to_delete.push_back(name);
                break;
            }
        }
    }

    for (const auto& name : to_delete) {
        auto it = m_node_renderers.find(name);
        if (it == m_node_renderers.end())
            continue;
        m_node_renderers_by_node.erase(it->second->get_node());
        m_node_renderers.erase(it);
        m_node_graph->remove_node(name);
    }

    rebuild_socket_id_maps();
    rebuild_links();
    if (!m_node_graph->get_nodes().empty())
        m_node_graph->connect_node_signals_and_slots();
}

void NodeGraphPanel::rename_selected_node(const std::string& old_name, const std::string& new_name)
{
    m_node_graph->rename_node(old_name, new_name);

    auto it = m_node_renderers.find(old_name);
    auto renderer = std::move(it->second);
    renderer->rename(new_name);
    m_node_renderers.erase(it);
    m_node_renderers.emplace(new_name, std::move(renderer));

    rebuild_socket_id_maps();
    rebuild_links();

    m_rename_current_node = new_name;
}

void NodeGraphPanel::rebuild_links()
{
    m_links.clear();
    auto& nodes = m_node_graph->get_nodes();
    for (auto& [name, node_renderer] : m_node_renderers) {
        const auto& node = *nodes.at(name).get();
        for (const auto& input_socket : node.input_sockets()) {
            if (!input_socket.is_socket_connected())
                continue;
            const int input_attr_id = node_renderer->get_input_socket_id(input_socket.name());
            const nodes::Node& connected_node = input_socket.connected_socket().node();
            const NodeRenderer* connected_renderer = m_node_renderers_by_node.at(&connected_node);
            const int output_attr_id = connected_renderer->get_output_socket_id(input_socket.connected_socket().name());
            m_links.emplace_back(input_attr_id, output_attr_id);
        }
    }
}

void NodeGraphPanel::rebuild_socket_id_maps()
{
    m_input_socket_by_id.clear();
    m_output_socket_by_id.clear();
    auto& nodes = m_node_graph->get_nodes();
    for (auto& [name, node_renderer] : m_node_renderers) {
        auto& node = *nodes.at(name).get();
        for (auto& socket : node.input_sockets())
            m_input_socket_by_id[node_renderer->get_input_socket_id(socket.name())] = &socket;
        for (auto& socket : node.output_sockets())
            m_output_socket_by_id[node_renderer->get_output_socket_id(socket.name())] = &socket;
    }
}

void NodeGraphPanel::render_error_modal()
{
    if (m_error_state.should_open) {
        ImGui::OpenPopup("Error");
        m_error_state.should_open = false;
    }

    // Always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushTextWrapPos(30.0f * ImGui::GetFontSize());
        ImGui::Text("%s", m_error_state.text.c_str());
        ImGui::PopTextWrapPos();

        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void NodeGraphPanel::render_first_run_notice_modal()
{
    if (m_notice_state.should_open) {
        ImGui::OpenPopup("first_run_notice");
        m_notice_state.should_open = false;
    }

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 0));

    if (ImGui::BeginPopupModal("first_run_notice", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
        ImGui::TextWrapped("%s", m_notice_state.text.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        const float button_width = 150.0f;
        ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - button_width + ImGui::GetStyle().WindowPadding.x);
        if (ImGui::Button("OK", ImVec2(button_width, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void NodeGraphPanel::render_menu()
{
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem(ICON_FA_FILE "  New Graph"))
                new_graph();
            ImGui::Separator();
            if (ImGui::BeginMenu(ICON_FA_FOLDER_OPEN "  Load Preset")) {
                for (const auto& preset : m_presets) {
                    if (ImGui::MenuItem(preset.name.c_str()))
                        m_pending_preset_path = preset.resource_path;
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem(ICON_FA_FILE_IMPORT "  Load from File..."))
                m_open_dialog_wants_open = true;
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_SAVE "  Save to File..."))
                m_save_dialog_wants_open = true;
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Graph")) {
            if (ImGui::MenuItem(ICON_FA_PLAY "  Run Full Graph", "Shift+R"))
                m_node_graph->run();
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_PLUS "  Add Node", "Shift+A")) {
                m_open_add_node_modal = true;
                m_open_add_node_request = true;
            }
            if (ImGui::MenuItem(ICON_FA_TRASH "  Delete Selected", "Del", false, ImNodes::NumSelectedNodes() > 0))
                delete_selected_nodes();
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_TH "  Apply Auto-Layout", "Alt+F"))
                reset_graph_layout();
            if (ImGui::MenuItem(ICON_FA_COMPRESS_ARROWS_ALT "  Recenter Graph", "Alt+C"))
                recenter_graph();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem(ICON_FA_ADJUST "  Toggle Background Mode", "Alt+M")) {
                m_render_mode = static_cast<GraphRenderingMode>((static_cast<int>(m_render_mode) + 1) % 3);
            }
            ImGui::Separator();
            const char* mode_name = m_render_mode == GraphRenderingMode::Default ? "Default"
                : m_render_mode == GraphRenderingMode::Transparent               ? "Transparent"
                                                                                 : "White";
            ImGui::Text("Current Mode: %s", mode_name);
            ImGui::Separator();
            ImGui::MenuItem(ICON_FA_FONT "  Small Font", nullptr, &m_use_small_font);
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

NodeRenderer* NodeGraphPanel::find_selected_node_renderer() const
{
    if (ImNodes::NumSelectedNodes() != 1)
        return nullptr;
    int node_id;
    ImNodes::GetSelectedNodes(&node_id);
    for (const auto& [name, renderer] : m_node_renderers) {
        if (renderer->get_node_id() == node_id)
            return renderer.get();
    }
    return nullptr;
}

void NodeGraphPanel::render_settings_panel()
{
    NodeRenderer* selected = find_selected_node_renderer();

    constexpr float panel_width = 430.0f;
    constexpr float panel_margin = 11.0f;
    constexpr float panel_margin_top = 26.0f;
    ImGui::SetNextWindowPos({ m_window_size.x - panel_width - panel_margin, panel_margin + panel_margin_top }, ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints({ panel_width, 0 }, { panel_width, m_window_size.y - panel_margin * 2 });
    ImGui::Begin("Node Settings",
        nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar);

    if (selected) {
        // Sync buffer when a different node is selected
        const std::string& raw_name = selected->get_name();
        if (m_rename_current_node != raw_name) {
            m_rename_current_node = raw_name;
            strncpy(m_rename_buf, raw_name.c_str(), sizeof(m_rename_buf) - 1);
            m_rename_buf[sizeof(m_rename_buf) - 1] = '\0';
        }

        const std::string buf_str(m_rename_buf);
        const bool is_valid = !buf_str.empty() && (buf_str == raw_name || !m_node_graph->exists_node(buf_str));

        if (!is_valid)
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.55f, 0.1f, 0.1f, 1.0f));

        const float input_width = std::max(80.0f, ImGui::CalcTextSize(m_rename_buf).x + 16.0f);
        ImGui::SetNextItemWidth(input_width);
        const bool changed = ImGui::InputText("##nodename", m_rename_buf, sizeof(m_rename_buf));

        if (!is_valid)
            ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(40, 70, 120, 200));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(40, 70, 120, 200));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(40, 70, 120, 200));
        ImGui::SmallButton(selected->get_node()->get_type_name().c_str());
        ImGui::PopStyleColor(3);

        if (changed) {
            // Read m_rename_buf AFTER InputText has written the new value into it
            const std::string new_name(m_rename_buf);
            const bool new_valid = !new_name.empty() && (new_name == raw_name || !m_node_graph->exists_node(new_name));
            if (new_valid && new_name != raw_name)
                rename_selected_node(raw_name, new_name);
        }

        ImGui::Separator();
        bool enabled = selected->get_node()->is_enabled();
        if (ImGui::Checkbox("Enabled", &enabled))
            selected->get_node()->set_enabled(enabled);
        if (selected->has_settings()) {
            ImGui::Separator();
            selected->render_settings_content();
        }
    } else {
        ImGui::TextDisabled("Select a node to view settings.");
    }

    ImGui::End();
}

} // namespace webgpu_app
