#include "gl_engine/UniformBufferObjects.h"
#include <AvalancheReportManager.h>
gl_engine::AvalancheReportManager::AvalancheReportManager(
    std::shared_ptr<avalanche::eaws::UIntIdManager> input_uint_id_manager, std::shared_ptr<gl_engine::UniformBuffer<avalanche::eaws::uboEawsReports>> input_ubo_eaws_reports)
    : m_uint_id_manager(input_uint_id_manager)
    , m_ubo_eaws_reports(input_ubo_eaws_reports)
{
    // create instances of uint_id_manager and report_load_service and connect them to methods
    std::make_unique<avalanche::eaws::ReportLoadService>();
    QObject::connect(this, &gl_engine::AvalancheReportManager::latest_report_report_requested, &m_report_load_service, &avalanche::eaws::ReportLoadService::load_latest_TU_Wien);
    QObject::connect(&m_report_load_service, &avalanche::eaws::ReportLoadService::load_latest_TU_Wien_finished, this, &gl_engine::AvalancheReportManager::receive_latest_reports_from_server);

    // Write zero-vectors to ubo with avalanche reports. THis means no report available
    std::fill(m_ubo_eaws_reports->data.reports, m_ubo_eaws_reports->data.reports + 1000, glm::uvec4(0, 0, 0, 0));
    m_ubo_eaws_reports->update_gpu_data();

    // Let uint_id_manager load all regions from server and then let it trigger an update of reports
    QObject::connect(m_uint_id_manager.get(), &avalanche::eaws::UIntIdManager::loaded_all_regions, this, &gl_engine::AvalancheReportManager::request_latest_reports_from_server);
    m_uint_id_manager->load_all_regions_from_server();
}

void gl_engine::AvalancheReportManager::request_latest_reports_from_server()
{
    // get latest report from server this must be connected to report loader
    emit latest_report_report_requested();
}

// Slot: Gets activated by member reprot load service when reports are available
void gl_engine::AvalancheReportManager::receive_latest_reports_from_server(tl::expected<std::vector<avalanche::eaws::ReportTUWien>, QString> data_from_server)
{
    // Fill array with zero vectors
    std::fill(m_ubo_eaws_reports->data.reports, m_ubo_eaws_reports->data.reports + 1000, glm::uvec4(0, 0, 0, 0));

    // If error occured during fetching reports send ubp with zeros to gpu
    if (!data_from_server.has_value()) {
        m_ubo_eaws_reports->update_gpu_data();
        return;
    }

    // if reports arrived as expected write them to ubo object
    std::vector<avalanche::eaws::ReportTUWien> reports = data_from_server.value();
    for (const avalanche::eaws::ReportTUWien& report : reports) {
        uint idx = m_uint_id_manager->convert_region_id_to_internal_id(report.region_id);
        m_ubo_eaws_reports->data.reports[idx] = glm::uvec4(1, report.border, report.rating_lo, report.rating_hi); // first index = 1 means report for this region available
    }

    // send updated ubo object to gpu
    m_ubo_eaws_reports->update_gpu_data();
    return;
}
