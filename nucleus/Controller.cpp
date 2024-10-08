/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
 * Copyright (C) 2024 Lucas Dworschak
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

#include "Controller.h"
#include "DataQuerier.h"

#include <QCoreApplication>
#include <QFile>
#include <QNetworkReply>
#ifdef ALP_ENABLE_THREADING
#include <QThread>
#endif

#include "AbstractRenderWindow.h"
#include "nucleus/camera/Controller.h"
#include "nucleus/camera/PositionStorage.h"
#include "nucleus/picker/PickerManager.h"
#include "nucleus/tile_scheduler/LayerAssembler.h"
#include "nucleus/tile_scheduler/QuadAssembler.h"
#include "nucleus/tile_scheduler/RateLimiter.h"
#include "nucleus/tile_scheduler/Scheduler.h"
#include "nucleus/tile_scheduler/SlotLimiter.h"
#include "nucleus/tile_scheduler/TileLoadService.h"
#include "nucleus/tile_scheduler/utils.h"
#include "nucleus/utils/thread.h"
#include "radix/TileHeights.h"

#ifdef ALP_ENABLE_LABELS
#include "nucleus/map_label/MapLabelFilter.h"
#endif

using namespace nucleus::tile_scheduler;
using namespace nucleus::maplabel;
using namespace nucleus::picker;

namespace nucleus {
Controller::Controller(AbstractRenderWindow* render_window)
    : m_render_window(render_window)
{
    qRegisterMetaType<nucleus::event_parameter::Touch>();
    qRegisterMetaType<nucleus::event_parameter::Mouse>();
    qRegisterMetaType<nucleus::event_parameter::Wheel>();

    m_render_window->set_quad_limit(512); // must be same as scheduler, dynamic resizing is not supported atm

    {
        auto terrain_service
            = std::make_unique<TileLoadService>("https://alpinemaps.cg.tuwien.ac.at/tiles/alpine_png/", TileLoadService::UrlPattern::ZXY, ".png");
        //    m_ortho_service.reset(new TileLoadService("https://tiles.bergfex.at/styles/bergfex-osm/", TileLoadService::UrlPattern::ZXY_yPointingSouth,
        //    ".jpeg")); m_ortho_service.reset(new TileLoadService("https://alpinemaps.cg.tuwien.ac.at/tiles/ortho/",
        //    TileLoadService::UrlPattern::ZYX_yPointingSouth, ".jpeg"));
        // m_ortho_service.reset(new TileLoadService("https://maps%1.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/",
        //                                           TileLoadService::UrlPattern::ZYX_yPointingSouth,
        //                                           ".jpeg",
        //                                           {"", "1", "2", "3", "4"}));
        auto ortho_service
            = std::make_unique<TileLoadService>("https://gataki.cg.tuwien.ac.at/raw/basemap/tiles/", TileLoadService::UrlPattern::ZYX_yPointingSouth, ".jpeg");
#ifdef ALP_ENABLE_LABELS
        auto vectortile_service = std::make_unique<TileLoadService>(
            "https://osm.cg.tuwien.ac.at/vector_tiles/poi_v1/", nucleus::tile_scheduler::TileLoadService::UrlPattern::ZXY_yPointingSouth, "");
#endif

        QFile file(":/map/height_data.atb");
        const auto open = file.open(QIODeviceBase::OpenModeFlag::ReadOnly);
        assert(open);
        Q_UNUSED(open);
        const QByteArray data = file.readAll();
        const auto decorator = nucleus::tile_scheduler::utils::AabbDecorator::make(TileHeights::deserialise(data));

        m_scheduler
            = nucleus::tile_scheduler::setup::monolithic(std::move(terrain_service), std::move(ortho_service), std::move(vectortile_service), decorator);
        m_render_window->set_aabb_decorator(decorator);
    }
    m_data_querier = std::make_shared<DataQuerier>(&m_scheduler.scheduler->ram_cache());
    m_scheduler.scheduler->set_dataquerier(m_data_querier);

    m_camera_controller = std::make_unique<nucleus::camera::Controller>(
        nucleus::camera::PositionStorage::instance()->get("grossglockner"), m_render_window->depth_tester(), m_data_querier.get());
    m_picker_manager = std::make_shared<PickerManager>();
    m_label_filter = std::make_shared<MapLabelFilter>();
    if (m_scheduler.thread) {
        m_picker_manager->moveToThread(m_scheduler.thread.get());
        m_label_filter->moveToThread(m_scheduler.thread.get());
    }

    if (QNetworkInformation::loadDefaultBackend() && QNetworkInformation::instance()) {
        QNetworkInformation* n = QNetworkInformation::instance();
        m_scheduler.scheduler->set_network_reachability(n->reachability());
        connect(n, &QNetworkInformation::reachabilityChanged, m_scheduler.scheduler.get(), &Scheduler::set_network_reachability);
    }

    connect(m_render_window, &AbstractRenderWindow::update_camera_requested, m_camera_controller.get(), &nucleus::camera::Controller::update_camera_request);
    connect(m_render_window, &AbstractRenderWindow::gpu_ready_changed, m_scheduler.scheduler.get(), &Scheduler::set_enabled);

    // NOTICE ME!!!! READ THIS, IF YOU HAVE TROUBLES WITH SIGNALS NOT REACHING THE QML RENDERING THREAD!!!!111elevenone
    // In Qt the rendering thread goes to sleep (at least until Qt 6.5, See RenderThreadNotifier).
    // At the time of writing, an additional connection from tile_ready and tile_expired to the notifier is made.
    // this only works if ALP_ENABLE_THREADING is on, i.e., the tile scheduler is on an extra thread. -> potential issue on webassembly
    connect(m_camera_controller.get(), &nucleus::camera::Controller::definition_changed, m_scheduler.scheduler.get(), &Scheduler::update_camera);
    connect(m_camera_controller.get(), &nucleus::camera::Controller::definition_changed, m_render_window, &AbstractRenderWindow::update_camera);

    connect(m_scheduler.scheduler.get(), &Scheduler::gpu_quads_updated, m_render_window, &AbstractRenderWindow::update_gpu_quads);
    connect(m_scheduler.scheduler.get(), &Scheduler::gpu_quads_updated, m_render_window, &AbstractRenderWindow::update_requested);
    connect(m_scheduler.scheduler.get(), &Scheduler::gpu_quads_updated, m_picker_manager.get(), &PickerManager::update_quads);

    connect(m_picker_manager.get(), &PickerManager::pick_requested, m_render_window, &AbstractRenderWindow::pick_value);
    connect(m_render_window, &AbstractRenderWindow::value_picked, m_picker_manager.get(), &PickerManager::eval_pick);
#ifdef ALP_ENABLE_LABELS
    connect(m_scheduler.scheduler.get(), &Scheduler::gpu_quads_updated, m_label_filter.get(), &MapLabelFilter::update_quads);
    connect(m_label_filter.get(), &MapLabelFilter::filter_finished, m_render_window, &AbstractRenderWindow::update_labels);
    connect(m_label_filter.get(), &MapLabelFilter::filter_finished, m_render_window, &AbstractRenderWindow::update_requested);
#endif
}

Controller::~Controller()
{
    nucleus::utils::thread::sync_call(m_scheduler.scheduler.get(), [this]() { m_scheduler = {}; });
#ifdef ALP_ENABLE_THREADING
    m_scheduler.thread->quit();
    m_scheduler.thread->wait(500); // msec
#endif
};

camera::Controller* Controller::camera_controller() const
{
    return m_camera_controller.get();
}

PickerManager* Controller::picker_manager() const { return m_picker_manager.get(); }

Scheduler* Controller::tile_scheduler() const { return m_scheduler.scheduler.get(); }

#ifdef ALP_ENABLE_LABELS
maplabel::MapLabelFilter* Controller::label_filter() const { return m_label_filter.get(); }
#endif

} // namespace nucleus
