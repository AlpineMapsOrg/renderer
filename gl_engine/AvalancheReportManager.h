#ifndef AVALANCHEREPORTMANAGER_H
#define AVALANCHEREPORTMANAGER_H
#include <QObject>
#include <nucleus/avalanche/ReportLoadService.h>
#include <nucleus/avalanche/eaws.h>
// Should be a member of the window.cpp and handles everything related to avalnche reports , especially updates ubo with currently selected report

namespace gl_engine {
class AvalancheReportManager : public QObject {
    Q_OBJECT
signals:
    void latest_report_report_requested();
public slots:
    void request_latest_reports_from_server();

    // recieves data from TU wien server, and writes it to ubo ob gpu
    void receive_latest_reports_from_server(tl::expected<std::vector<avalanche::eaws::ReportTUWien>, QString> data_from_server);

public:
    AvalancheReportManager(std::shared_ptr<avalanche::eaws::UIntIdManager> input_uint_id_manager, std::shared_ptr<gl_engine::UniformBuffer<avalanche::eaws::uboEawsReports>> input_ubo_eaws_reports);
    ~AvalancheReportManager() = default;

private:
    avalanche::eaws::ReportLoadService m_report_load_service;
    std::shared_ptr<avalanche::eaws::UIntIdManager> m_uint_id_manager;
    std::shared_ptr<gl_engine::UniformBuffer<avalanche::eaws::uboEawsReports>> m_ubo_eaws_reports;

public:
    bool operator==(const AvalancheReportManager& rhs)
    {
        return (m_ubo_eaws_reports.get() == rhs.m_ubo_eaws_reports.get() && m_uint_id_manager == rhs.m_uint_id_manager && m_report_load_service == rhs.m_report_load_service);
    }
};
} // namespace gl_engine
#endif // AVALANCHEREPORTMANAGER_H
