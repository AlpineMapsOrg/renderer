/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Adam Celarek
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

#include "RenderingContext.h"

#include "RenderThreadNotifier.h"
#include <QThread>
#include <gl_engine/MapLabelManager.h>
#include <nucleus/DataQuerier.h>
#include <nucleus/camera/Controller.h>
#include <nucleus/camera/PositionStorage.h>
#include <nucleus/map_label/MapLabelFilter.h>
#include <nucleus/map_label/setup.h>
#include <nucleus/picker/PickerManager.h>
#include <nucleus/tile_scheduler/OldScheduler.h>
#include <nucleus/tile_scheduler/TileLoadService.h>
#include <nucleus/utils/thread.h>

using namespace nucleus::tile_scheduler;
using namespace nucleus::maplabel;
using namespace nucleus::picker;
using nucleus::DataQuerier;

struct RenderingContext::Data {
    nucleus::map_label::setup::SchedulerHolder map_label;
};

RenderingContext::RenderingContext(QObject* parent)
    : QObject { parent }
    , m(std::make_unique<RenderingContext::Data>())
{
    using TilePattern = nucleus::tile_scheduler::TileLoadService::UrlPattern;
    assert(QThread::currentThread() == QCoreApplication::instance()->thread());

    auto terrain_service = std::make_unique<TileLoadService>("https://alpinemaps.cg.tuwien.ac.at/tiles/alpine_png/", TilePattern::ZXY, ".png");
    //    m_ortho_service.reset(new TileLoadService("https://tiles.bergfex.at/styles/bergfex-osm/", TileLoadService::UrlPattern::ZXY_yPointingSouth,
    //    ".jpeg")); m_ortho_service.reset(new TileLoadService("https://alpinemaps.cg.tuwien.ac.at/tiles/ortho/",
    //    TileLoadService::UrlPattern::ZYX_yPointingSouth, ".jpeg"));
    // m_ortho_service.reset(new TileLoadService("https://maps%1.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/",
    //                                           TileLoadService::UrlPattern::ZYX_yPointingSouth,
    //                                           ".jpeg",
    //                                           {"", "1", "2", "3", "4"}));
    auto ortho_service
        = std::make_unique<TileLoadService>("https://gataki.cg.tuwien.ac.at/raw/basemap/tiles/", TileLoadService::UrlPattern::ZYX_yPointingSouth, ".jpeg");
    auto vectortile_service = std::make_unique<TileLoadService>("https://osm.cg.tuwien.ac.at/vector_tiles/poi_v1/", TilePattern::ZXY_yPointingSouth, "");
    m_aabb_decorator = nucleus::tile_scheduler::setup::aabb_decorator();
    m_scheduler
        = nucleus::tile_scheduler::setup::monolithic(std::move(terrain_service), std::move(ortho_service), std::move(vectortile_service), m_aabb_decorator);
    m_data_querier = std::make_shared<DataQuerier>(&m_scheduler.scheduler->ram_cache());
    m->map_label = nucleus::map_label::setup::scheduler(std::make_unique<TileLoadService>("https://osm.cg.tuwien.ac.at/vector_tiles/poi_v1/", TilePattern::ZXY_yPointingSouth, ""),
        m_aabb_decorator,
        m_data_querier,
        m_scheduler.thread.get());
    m->map_label.scheduler->set_geometry_ram_cache(&m_scheduler.scheduler->ram_cache());

    m_scheduler.scheduler->set_dataquerier(m_data_querier);

    m_picker_manager = std::make_shared<PickerManager>();
    m_label_filter = std::make_shared<MapLabelFilter>();
    if (m_scheduler.thread) {
        m_picker_manager->moveToThread(m_scheduler.thread.get());
        m_label_filter->moveToThread(m_scheduler.thread.get());
    }
    connect(m_scheduler.scheduler.get(), &OldScheduler::gpu_quads_updated, m_picker_manager.get(), &PickerManager::update_quads);
    connect(m->map_label.scheduler.get(), &nucleus::map_label::Scheduler::gpu_quads_updated, m_label_filter.get(), &MapLabelFilter::update_quads);

    connect(m_scheduler.scheduler.get(), &nucleus::tile_scheduler::OldScheduler::gpu_quads_updated, RenderThreadNotifier::instance(), &RenderThreadNotifier::notify);
    connect(m->map_label.scheduler.get(), &nucleus::map_label::Scheduler::gpu_quads_updated, RenderThreadNotifier::instance(), &RenderThreadNotifier::notify);

    if (QNetworkInformation::loadDefaultBackend() && QNetworkInformation::instance()) {
        QNetworkInformation* n = QNetworkInformation::instance();
        m_scheduler.scheduler->set_network_reachability(n->reachability());
        connect(n, &QNetworkInformation::reachabilityChanged, m_scheduler.scheduler.get(), &OldScheduler::set_network_reachability);
    }
}

RenderingContext::~RenderingContext()
{
    nucleus::utils::thread::sync_call(m_scheduler.scheduler.get(), [this]() {
        m_scheduler.scheduler = {};
        m_camera_controller = {};
        m_label_filter = {};
        m_picker_manager = {};
    });
    nucleus::utils::thread::sync_call(m_scheduler.ortho_service.get(), [this]() {
        m_scheduler.ortho_service = {};
        m_scheduler.terrain_service = {};
        m_scheduler.vector_service = {};
    });
    if (m_scheduler.thread) {
        m_scheduler.thread->quit();
        m_scheduler.thread->wait(500); // msec
    }
}

RenderingContext* RenderingContext::instance()
{

    static RenderingContext s_instance;
    return &s_instance;
}

void RenderingContext::initialise()
{
    QMutexLocker locker(&m_shared_ptr_mutex);
    if (m_engine_context)
        return;

    m_engine_context = std::make_shared<gl_engine::Context>();

    // labels
    m_engine_context->set_map_label_manager(std::make_unique<gl_engine::MapLabelManager>());
    connect(m_label_filter.get(), &MapLabelFilter::filter_finished, m_engine_context->map_label_manager(), &gl_engine::MapLabelManager::update_labels);
    nucleus::utils::thread::async_call(m->map_label.scheduler.get(), [this]() { m->map_label.scheduler->set_enabled(true); });

    auto* render_thread = QThread::currentThread();
    connect(render_thread, &QThread::finished, m_engine_context.get(), &nucleus::EngineContext::destroy);

    nucleus::utils::thread::async_call(this, [this]() { emit this->initialised(); });
    // nucleus::utils::thread::async_call(m_scheduler.scheduler.get(), [this]() { m_scheduler.scheduler->set_enabled(true); }); // after moving tile manager
    // to gl_engine::Context.
}

const std::shared_ptr<gl_engine::Context>& RenderingContext::engine_context() const
{
    QMutexLocker locker(&m_shared_ptr_mutex);
    return m_engine_context;
}

std::shared_ptr<nucleus::tile_scheduler::utils::AabbDecorator> RenderingContext::aabb_decorator() const
{
    QMutexLocker locker(&m_shared_ptr_mutex);
    return m_aabb_decorator;
}

std::shared_ptr<nucleus::DataQuerier> RenderingContext::data_querier() const
{
    QMutexLocker locker(&m_shared_ptr_mutex);
    return m_data_querier;
}

OldScheduler* RenderingContext::scheduler() const
{
    QMutexLocker locker(&m_shared_ptr_mutex);
    return m_scheduler.scheduler.get();
}

std::shared_ptr<nucleus::picker::PickerManager> RenderingContext::picker_manager() const
{
    QMutexLocker locker(&m_shared_ptr_mutex);
    return m_picker_manager;
}

std::shared_ptr<nucleus::maplabel::MapLabelFilter> RenderingContext::label_filter() const
{
    QMutexLocker locker(&m_shared_ptr_mutex);
    return m_label_filter;
}

nucleus::map_label::Scheduler* RenderingContext::map_label_scheduler() const
{
    QMutexLocker locker(&m_shared_ptr_mutex);
    return m->map_label.scheduler.get();
}
