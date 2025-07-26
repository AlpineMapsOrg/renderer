/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Joerg Christian Reiher
 * Copyright (C) 2025 Adam Celarek
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

#include <QObject>
#include <nucleus/avalanche/ReportLoadService.h>
#include <nucleus/avalanche/eaws.h>
// Should be a member of the window.cpp and handles everything related to avalnche reports , especially updates ubo with currently selected report

namespace gl_engine {
template <typename T> class UniformBuffer;

class AvalancheReportManager : public QObject {
    Q_OBJECT
signals:
    void report_requested(QDate report_date);
public slots:
    void request_report_from_server(tl::expected<uint, QString> result);

    // recieves data from TU wien server, and writes it to ubo ob gpu
    void receive_report_from_server(tl::expected<std::vector<nucleus::avalanche::ReportTUWien>, QString> data_from_server);

public:
    AvalancheReportManager();
    ~AvalancheReportManager() = default;
    void set_ubo_eaws_reports(std::shared_ptr<gl_engine::UniformBuffer<nucleus::avalanche::uboEawsReports>> m_ubo_eaws_reports);

private:
    nucleus::avalanche::ReportLoadService m_report_load_service;
    std::shared_ptr<nucleus::avalanche::UIntIdManager> m_uint_id_manager;
    std::shared_ptr<gl_engine::UniformBuffer<nucleus::avalanche::uboEawsReports>> m_ubo_eaws_reports;
};
} // namespace gl_engine
