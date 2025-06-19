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
#include <QCoreApplication>
#include <QMutex>
#include <QOpenGLContext>
#include <QThread>
#include <gl_engine/Context.h>
#include <gl_engine/MapLabels.h>
#include <gl_engine/TextureLayer.h>
#include <gl_engine/TileGeometry.h>
#include <nucleus/DataQuerier.h>
#include <nucleus/camera/Controller.h>
#include <nucleus/camera/PositionStorage.h>
#include <nucleus/map_label/Filter.h>
#include <nucleus/map_label/setup.h>
#include <nucleus/picker/PickerManager.h>
#include <nucleus/tile/GeometryScheduler.h>
#include <nucleus/tile/SchedulerDirector.h>
#include <nucleus/tile/TextureScheduler.h>
#include <nucleus/tile/TileLoadService.h>
#include <nucleus/tile/setup.h>
#include <nucleus/utils/thread.h>

using namespace nucleus::tile;
using namespace nucleus::map_label;
using namespace nucleus::picker;
using nucleus::DataQuerier;

struct RenderingContext::Data {
    // WARNING: gl_engine::Context must be on the rendering thread!!
    mutable QMutex shared_ptr_mutex; // protects the shared_ptr
    std::shared_ptr<gl_engine::Context> engine_context;

    std::unique_ptr<QThread> scheduler_thread;
    // the ones below are on the scheduler thread.
    nucleus::tile::setup::GeometrySchedulerHolder geometry;
    nucleus::tile::setup::TextureSchedulerHolder ortho_texture;
    nucleus::map_label::setup::SchedulerHolder map_label;
    std::shared_ptr<nucleus::DataQuerier> data_querier;
    std::unique_ptr<nucleus::camera::Controller> camera_controller;
    std::shared_ptr<nucleus::map_label::Filter> label_filter;
    std::shared_ptr<nucleus::picker::PickerManager> picker_manager;
    std::shared_ptr<nucleus::tile::utils::AabbDecorator> aabb_decorator;
    std::unique_ptr<nucleus::tile::SchedulerDirector> scheduler_director;
};

RenderingContext::RenderingContext(QObject* parent)
    : QObject { parent }
    , m(std::make_unique<RenderingContext::Data>())
{
    using TilePattern = nucleus::tile::TileLoadService::UrlPattern;
    assert(QThread::currentThread() == QCoreApplication::instance()->thread());

#ifdef ALP_ENABLE_THREADING
    m->scheduler_thread = std::make_unique<QThread>();
    m->scheduler_thread->setObjectName("scheduler_thread");
#endif

    m->scheduler_director = std::make_unique<nucleus::tile::SchedulerDirector>();

    // m->ortho_service.reset(new TileLoadService("https://tiles.bergfex.at/styles/bergfex-osm/", TileLoadService::UrlPattern::ZXY_yPointingSouth, ".jpeg"));
    // m->ortho_service.reset(new TileLoadService("https://alpinemaps.cg.tuwien.ac.at/tiles/ortho/", TileLoadService::UrlPattern::ZYX_yPointingSouth, ".jpeg"));
    // m->ortho_service.reset(new TileLoadService("https://maps%1.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/",
    //                                           TileLoadService::UrlPattern::ZYX_yPointingSouth,
    //                                           ".jpeg",
    //                                           {"", "1", "2", "3", "4"}));
    m->aabb_decorator = nucleus::tile::setup::aabb_decorator();
    {
        // clang-format off
        auto geometry_service = std::make_unique<TileLoadService>("https://alpinemaps.cg.tuwien.ac.at/tiles/alpine_png/", TilePattern::ZXY, ".png");
        m->geometry = nucleus::tile::setup::geometry_scheduler(std::move(geometry_service), m->aabb_decorator, m->scheduler_thread.get());
        m->scheduler_director->check_in("geometry", m->geometry.scheduler);
        m->data_querier = std::make_shared<DataQuerier>(&m->geometry.scheduler->ram_cache());
        // auto ortho_service = std::make_unique<TileLoadService>("https://gataki.cg.tuwien.ac.at/raw/basemap/tiles/", TilePattern::ZYX_yPointingSouth, ".jpeg");
        auto ortho_service = std::make_unique<TileLoadService>("https://mapsneu.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/", TilePattern::ZYX_yPointingSouth, ".jpeg");
        m->ortho_texture = nucleus::tile::setup::texture_scheduler(std::move(ortho_service), m->aabb_decorator, m->scheduler_thread.get());
        m->scheduler_director->check_in("ortho", m->ortho_texture.scheduler);
        auto map_label_service = std::make_unique<TileLoadService>("https://osm.cg.tuwien.ac.at/vector_tiles/poi_v1/", TilePattern::ZXY_yPointingSouth, "");
        m->map_label = nucleus::map_label::setup::scheduler(std::move(map_label_service), m->aabb_decorator, m->data_querier, m->scheduler_thread.get());
        m->scheduler_director->check_in("map_label", m->map_label.scheduler);
        // clang-format on

        m->scheduler_director->visit([](nucleus::tile::Scheduler* sch) { nucleus::utils::thread::async_call(sch, [sch]() { sch->read_disk_cache(); }); });
    }
    m->map_label.scheduler->set_geometry_ram_cache(&m->geometry.scheduler->ram_cache());
    m->geometry.scheduler->set_dataquerier(m->data_querier);

    m->picker_manager = std::make_shared<PickerManager>();
    m->label_filter = std::make_shared<Filter>();
    if (m->scheduler_thread) {
        m->picker_manager->moveToThread(m->scheduler_thread.get());
        m->label_filter->moveToThread(m->scheduler_thread.get());
    }
    // clang-format off
    connect(m->geometry.scheduler.get(),       &nucleus::tile::GeometryScheduler::gpu_tiles_updated, RenderThreadNotifier::instance(), &RenderThreadNotifier::notify);
    connect(m->ortho_texture.scheduler.get(),  &nucleus::tile::TextureScheduler::gpu_tiles_updated,  RenderThreadNotifier::instance(), &RenderThreadNotifier::notify);
    connect(m->map_label.scheduler.get(),      &nucleus::map_label::Scheduler::gpu_tiles_updated,    RenderThreadNotifier::instance(), &RenderThreadNotifier::notify);
    connect(m->map_label.scheduler.get(),      &nucleus::map_label::Scheduler::gpu_tiles_updated,    m->picker_manager.get(),          &PickerManager::update_quads);
    connect(m->map_label.scheduler.get(),      &nucleus::map_label::Scheduler::gpu_tiles_updated,    m->label_filter.get(),            &Filter::update_quads);
    // clang-format on

    if (QNetworkInformation::loadDefaultBackend() && QNetworkInformation::instance()) {
        QNetworkInformation* n = QNetworkInformation::instance();
        m->geometry.scheduler->set_network_reachability(n->reachability());
        m->ortho_texture.scheduler->set_network_reachability(n->reachability());
        m->map_label.scheduler->set_network_reachability(n->reachability());
        // clang-format off
        connect(n, &QNetworkInformation::reachabilityChanged, m->geometry.scheduler.get(),       &nucleus::tile::Scheduler::set_network_reachability);
        connect(n, &QNetworkInformation::reachabilityChanged, m->ortho_texture.scheduler.get(),  &nucleus::tile::Scheduler::set_network_reachability);
        connect(n, &QNetworkInformation::reachabilityChanged, m->map_label.scheduler.get(),      &nucleus::tile::Scheduler::set_network_reachability);
        // clang-format on
    }
#ifdef ALP_ENABLE_THREADING
    qDebug() << "Scheduler thread: " << m->scheduler_thread.get();
    m->scheduler_thread->start();
#endif
}

RenderingContext::~RenderingContext()
{
    // being killed after app exited -> no event loop
}

RenderingContext* RenderingContext::instance()
{

    static RenderingContext s_instance;
    return &s_instance;
}

void RenderingContext::initialise()
{
    QMutexLocker locker(&m->shared_ptr_mutex);
    if (m->engine_context)
        return;

    m->engine_context = std::make_shared<gl_engine::Context>();
    // standard tiles
    m->engine_context->set_tile_geometry(std::make_shared<gl_engine::TileGeometry>(65));
    m->engine_context->set_ortho_layer(std::make_shared<gl_engine::TextureLayer>(512));
    m->engine_context->tile_geometry()->set_tile_limit(2048);
    m->engine_context->tile_geometry()->set_aabb_decorator(m->aabb_decorator);
    m->engine_context->set_aabb_decorator(m->aabb_decorator);
    m->engine_context->ortho_layer()->set_tile_limit(1024);

    nucleus::utils::thread::async_call(m->geometry.scheduler.get(), [this]() { m->geometry.scheduler->set_enabled(true); });
    const auto texture_compression = gl_engine::Texture::compression_algorithm();
    nucleus::utils::thread::async_call(m->ortho_texture.scheduler.get(), [this, texture_compression]() {
        m->ortho_texture.scheduler->set_texture_compression_algorithm(texture_compression);
        m->ortho_texture.scheduler->set_enabled(true);
    });

    // labels
    m->engine_context->set_map_label_manager(std::make_unique<gl_engine::MapLabels>(m->aabb_decorator));
    connect(m->label_filter.get(), &Filter::filter_finished, m->engine_context->map_label_manager(), &gl_engine::MapLabels::update_labels);
    nucleus::utils::thread::async_call(m->map_label.scheduler.get(), [this]() { m->map_label.scheduler->set_enabled(true); });

    // clang-format off
    connect(m->geometry.scheduler.get(),        &nucleus::tile::GeometryScheduler::gpu_tiles_updated,   m->engine_context->tile_geometry(), &gl_engine::TileGeometry::update_gpu_tiles);
    connect(m->ortho_texture.scheduler.get(),   &nucleus::tile::TextureScheduler::gpu_tiles_updated,    m->engine_context->ortho_layer(),   &gl_engine::TextureLayer::update_gpu_tiles);

    connect(QOpenGLContext::currentContext(), &QOpenGLContext::aboutToBeDestroyed, m->engine_context.get(), &nucleus::EngineContext::destroy);
    connect(QOpenGLContext::currentContext(), &QOpenGLContext::aboutToBeDestroyed, this,                    &RenderingContext::destroy);
    connect(QCoreApplication::instance(),     &QCoreApplication::aboutToQuit,      this,                    &RenderingContext::destroy);
    // clang-format on

    m->engine_context->initialise();
    nucleus::utils::thread::async_call(this, [this]() { emit this->initialised(); });
}

void RenderingContext::destroy()
{
    QMutexLocker locker(&m->shared_ptr_mutex);
    if (!m->geometry.scheduler)
        return;
    if (m->scheduler_thread) {
        nucleus::utils::thread::sync_call(m->geometry.scheduler.get(), [this]() {
            m->geometry.scheduler.reset();
            m->camera_controller.reset();
            m->label_filter.reset();
            m->picker_manager.reset();
            m->map_label.scheduler.reset();
            m->ortho_texture.scheduler.reset();
            m->scheduler_director.reset();
        });
        nucleus::utils::thread::sync_call(m->geometry.tile_service.get(), [this]() {
            m->geometry.tile_service.reset();
            m->map_label.tile_service.reset();
            m->ortho_texture.tile_service.reset();
        });
        m->scheduler_thread->quit();
        m->scheduler_thread->wait(500); // msec
        m->scheduler_thread.reset();
    }
}

std::shared_ptr<gl_engine::Context> RenderingContext::engine_context() const
{
    QMutexLocker locker(&m->shared_ptr_mutex);
    return m->engine_context;
}

std::shared_ptr<nucleus::tile::utils::AabbDecorator> RenderingContext::aabb_decorator() const
{
    QMutexLocker locker(&m->shared_ptr_mutex);
    return m->aabb_decorator;
}

std::shared_ptr<nucleus::DataQuerier> RenderingContext::data_querier() const
{
    QMutexLocker locker(&m->shared_ptr_mutex);
    return m->data_querier;
}

GeometryScheduler* RenderingContext::geometry_scheduler() const
{
    QMutexLocker locker(&m->shared_ptr_mutex);
    return m->geometry.scheduler.get();
}

std::shared_ptr<nucleus::picker::PickerManager> RenderingContext::picker_manager() const
{
    QMutexLocker locker(&m->shared_ptr_mutex);
    return m->picker_manager;
}

std::shared_ptr<nucleus::map_label::Filter> RenderingContext::label_filter() const
{
    QMutexLocker locker(&m->shared_ptr_mutex);
    return m->label_filter;
}

nucleus::map_label::Scheduler* RenderingContext::map_label_scheduler() const
{
    QMutexLocker locker(&m->shared_ptr_mutex);
    return m->map_label.scheduler.get();
}

nucleus::tile::TextureScheduler* RenderingContext::ortho_scheduler() const
{
    QMutexLocker locker(&m->shared_ptr_mutex);
    return m->ortho_texture.scheduler.get();
}

SchedulerDirector* RenderingContext::scheduler_director() const
{
    QMutexLocker locker(&m->shared_ptr_mutex);
    return m->scheduler_director.get();
}
