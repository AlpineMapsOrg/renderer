#include "gl_engine/UniformBufferObjects.h"
#include "nucleus/avalanche/UIntIdManager.h"
#include <AvalancheReportManager.h>
#include <iostream>
gl_engine::AvalancheReportManager::AvalancheReportManager(
    std::shared_ptr<avalanche::eaws::UIntIdManager> input_uint_id_manager, std::shared_ptr<gl_engine::UniformBuffer<avalanche::eaws::uboEawsReports>> input_ubo_eaws_reports)
    : m_uint_id_manager(input_uint_id_manager)
    , m_ubo_eaws_reports(input_ubo_eaws_reports)
{
    // create instances of uint_id_manager and report_load_service and connect them to methods
    std::make_unique<avalanche::eaws::ReportLoadService>();
    QObject::connect(this, &gl_engine::AvalancheReportManager::report_requested, &m_report_load_service, &avalanche::eaws::ReportLoadService::load_from_tu_wien);
    QObject::connect(&m_report_load_service, &avalanche::eaws::ReportLoadService::load_from_TU_Wien_finished, this, &gl_engine::AvalancheReportManager::receive_report_from_server);

    // Write zero-vectors to ubo with avalanche reports. THis means no report available
    std::fill(m_ubo_eaws_reports->data.reports, m_ubo_eaws_reports->data.reports + 1000, glm::uvec4(0, 0, 0, 0));
    m_ubo_eaws_reports->update_gpu_data();

    // Let uint_id_manager load all regions from server and then let it trigger an update of reports
    QObject::connect(m_uint_id_manager.get(), &avalanche::eaws::UIntIdManager::loaded_all_regions, this, &gl_engine::AvalancheReportManager::request_report_from_server);
    m_uint_id_manager->load_all_regions_from_server();
}

void gl_engine::AvalancheReportManager::request_report_from_server(tl::expected<uint, QString> result)
{
    // get latest report from server this must be connected to report loader
    emit report_requested(m_uint_id_manager->get_date());

    // If no regions were obtained from server, print error message we obtained from parsing server results.
    if (!result)
        std::cout << "\n" << result.error().toStdString();
}

#include <unordered_set>
// Slot: Gets activated by member report load service when reports are available
void gl_engine::AvalancheReportManager::receive_report_from_server(tl::expected<std::vector<avalanche::eaws::ReportTUWien>, QString> data_from_server)
{
    // Fill array with initial vectors
    std::fill(m_ubo_eaws_reports->data.reports, m_ubo_eaws_reports->data.reports + 1000, glm::ivec4(-1, 0, 0, 0));

    // If error occured during fetching reports send ubo with zeros to gpu
    if (!data_from_server.has_value()) {
        m_ubo_eaws_reports->update_gpu_data();
        return;
    }

    // Debug: Example: Report contains region ID AT-02-02-00. List of region only contains AT-02-02-01, AT-02-02-02
    // The report ending in 00 is thus applied to the offical subregions of AT-02-02;
    // So far all regions that have this problem were collected in a set. A solution for this problem must be found .
    std::unordered_set<QString> wrongRegions({ QString("AT-02-01-00"),
        QString("AT-02-02-00"),
        QString("AT-02-03-00"),
        QString("AT-02-04-00"),
        QString("AT-02-05-00"),
        QString("AT-02-06-00"),
        QString("AT-02-07-00"),
        QString("AT-02-08-00"),
        QString("AT-02-09-00"),
        QString("AT-02-10-00"),
        QString("AT-02-11-00"),
        QString("AT-02-12-00"),
        QString("AT-02-13-00"),
        QString("AT-02-14-00"),
        QString("AT-02-15-00"),
        QString("AT-02-16-00"),
        QString("AT-02-17-00"),
        QString("AT-02-18-00"),
        QString("AT-02-19-00"),
        QString("AT-03-01-00"),
        QString("AT-03-02-00"),
        QString("AT-03-03-00"),
        QString("AT-03-04-00"),
        QString("AT-03-05-00"),
        QString("AT-03-06-00"),
        QString("AT-04-01-00"),
        QString("AT-04-02-00"),
        QString("AT-04-03-00"),
        QString("AT-04-04-00"),
        QString("AT-04-05-00"),
        QString("AT-04-06-00"),
        QString("AT-04-07-00"),
        QString("AT-04-08-00"),
        QString("AT-04-09-00"),
        QString("AT-05-01-00"),
        QString("AT-05-02-00"),
        QString("AT-05-03-00"),
        QString("AT-05-04-00"),
        QString("AT-05-05-00"),
        QString("AT-05-06-00"),
        QString("AT-05-07-00"),
        QString("AT-05-08-00"),
        QString("AT-05-09-00"),
        QString("AT-05-10-00"),
        QString("AT-05-11-00"),
        QString("AT-05-12-00"),
        QString("AT-05-13-00"),
        QString("AT-05-14-00"),
        QString("AT-05-15-00"),
        QString("AT-05-16-00"),
        QString("AT-05-17-00"),
        QString("AT-05-18-00"),
        QString("AT-05-19-00"),
        QString("AT-05-20-00"),
        QString("AT-05-21-00"),
        QString("AT-06-01-00"),
        QString("AT-06-02-00"),
        QString("AT-06-03-00"),
        QString("AT-06-05-00"),
        QString("AT-06-06-00"),
        QString("AT-06-07-00"),
        QString("AT-06-08-00"),
        QString("AT-06-09-00"),
        QString("AT-06-10-00"),
        QString("AT-06-11-00"),
        QString("AT-06-12-00"),
        QString("AT-06-13-00"),
        QString("AT-06-14-00"),
        QString("AT-06-15-00"),
        QString("AT-06-16-00"),
        QString("AT-06-17-00"),
        QString("AT-06-18-00"),
        QString("AT-07-01-00"),
        QString("AT-07-03-00"),
        QString("AT-07-05-00"),
        QString("AT-07-06-00"),
        QString("AT-07-07-00"),
        QString("AT-07-08-00"),
        QString("AT-07-09-00"),
        QString("AT-07-10-00"),
        QString("AT-07-11-00"),
        QString("AT-07-12-00"),
        QString("AT-07-13-00"),
        QString("AT-07-15-00"),
        QString("AT-07-16-00"),
        QString("AT-07-18-00"),
        QString("AT-07-19-00"),
        QString("AT-07-20-00"),
        QString("AT-07-21-00"),
        QString("AT-07-22-00"),
        QString("AT-07-23-00"),
        QString("AT-07-24-00"),
        QString("AT-07-25-00"),
        QString("AT-07-26-00"),
        QString("AT-07-27-00"),
        QString("AT-07-28-00"),
        QString("AT-07-29-00"),
        QString("AT-08-01-00"),
        QString("AT-08-02-00"),
        QString("AT-08-04-00"),
        QString("AT-08-06-00") });

    // if reports arrived as expected write them to ubo object
    std::vector<avalanche::eaws::ReportTUWien> reports = data_from_server.value();
    for (const avalanche::eaws::ReportTUWien& report : reports) {

        // Correct region id if it has invalid format: That means create new sub regions with same forecast
        std::vector<QString> new_region_ids;
        if (wrongRegions.contains(report.region_id)) {
            // remove last three digits
            QString parent_region_id = report.region_id;
            parent_region_id = parent_region_id.left(parent_region_id.length() - 3);

            // check if report region contains subregions, report then applies for all subregions
            uint i = 1;
            QString new_region_id = parent_region_id + QString("-01");
            while (m_uint_id_manager->contains(new_region_id)) {
                new_region_ids.push_back(new_region_id);
                i++;
                new_region_id = i < 10 ? parent_region_id + QString("-0") + QString::number(i) : parent_region_id + QString("-") + QString::number(i);
            }

            // If no children were found use correct region_id
            if (new_region_ids.empty()) {
                new_region_ids.push_back(parent_region_id);
            }

        } else {
            new_region_ids.push_back(report.region_id);
        }

        // Write (corrected) region(s) to ubo
        for (QString new_region_id : new_region_ids) {
            uint idx = m_uint_id_manager->convert_region_id_to_internal_id(new_region_id);
            m_ubo_eaws_reports->data.reports[idx] = glm::ivec4(report.unfavorable, report.border, report.rating_lo, report.rating_hi);
        }
    }

    // send updated ubo object to gpu
    m_ubo_eaws_reports->update_gpu_data();
    return;
}
