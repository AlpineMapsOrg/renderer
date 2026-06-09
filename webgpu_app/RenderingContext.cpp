/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Adam Celarek
 * Copyright (C) 2025 Patrick Komon
 * Copyright (C) 2025 Gerald Kimmersdorfer
 * Copyright (C) 2026 Wendelin Muth
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

#include "nucleus/DataQuerier.h"
#include "nucleus/tile/SchedulerDirector.h"
#include "nucleus/tile/Texture3DScheduler.h"
#include "nucleus/tile/TileLoadService.h"
#include "nucleus/tile/setup.h"
#include "webgpu_engine/Context.h"
#include "webgpu_engine/cloud/CloudRenderer.h"
#include "webgpu_engine/overlay/HeightLinesOverlay.h"
#include "webgpu_engine/overlay/OverlayRenderer.h"
#include "webgpu_engine/overlay/TextureOverlay.h"
#include "webgpu_engine/tile_mesh/TileMeshRenderer.h"

namespace webgpu_app {

RenderingContext::RenderingContext()
{

    using TilePattern = nucleus::tile::TileLoadService::UrlPattern;
    assert(QThread::currentThread() == QCoreApplication::instance()->thread());

#ifdef ALP_ENABLE_THREADING
    m_scheduler_thread = std::make_unique<QThread>();
    m_scheduler_thread->setObjectName("scheduler_thread");
#endif

    m_scheduler_director = std::make_unique<nucleus::tile::SchedulerDirector>();

    //    m->ortho_service.reset(new TileLoadService("https://tiles.bergfex.at/styles/bergfex-osm/", TileLoadService::UrlPattern::ZXY_yPointingSouth,
    //    ".jpeg")); m->ortho_service.reset(new TileLoadService("https://alpinemaps.cg.tuwien.ac.at/tiles/ortho/",
    //    TileLoadService::UrlPattern::ZYX_yPointingSouth, ".jpeg"));
    // m->ortho_service.reset(new TileLoadService("https://maps%1.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/",
    //                                           TileLoadService::UrlPattern::ZYX_yPointingSouth,
    //                                           ".jpeg",
    //                                           {"", "1", "2", "3", "4"}));
    m_aabb_decorator = nucleus::tile::setup::aabb_decorator();
    {
        auto geometry_service
            = std::make_unique<nucleus::tile::TileLoadService>("https://alpinemaps.cg.tuwien.ac.at/tiles/alpine_png/", TilePattern::ZXY, ".png");
        m_geometry_scheduler_holder = nucleus::tile::setup::geometry_scheduler(std::move(geometry_service), m_aabb_decorator, m_scheduler_thread.get());
        m_geometry_scheduler_holder.scheduler->set_gpu_quad_limit(256); // TODO
        m_scheduler_director->check_in("geometry", m_geometry_scheduler_holder.scheduler);
        m_data_querier = std::make_shared<nucleus::DataQuerier>(&m_geometry_scheduler_holder.scheduler->ram_cache());
        auto ortho_service
            = std::make_unique<nucleus::tile::TileLoadService>("https://gataki.cg.tuwien.ac.at/raw/basemap/tiles/", TilePattern::ZYX_yPointingSouth, ".jpeg");
        // auto ortho_service = std::make_unique<nucleus::tile::TileLoadService>("https://maps.wien.gv.at/basemap/bmaporthofoto30cm/normal/google3857/",
        // TilePattern::ZYX_yPointingSouth, ".jpeg"); auto ortho_service =
        // std::make_unique<nucleus::tile::TileLoadService>("https://mapsneu.wien.gv.at/basemap/bmapgelaende/grau/google3857/", TilePattern::ZYX_yPointingSouth,
        // ".jpeg");
        m_ortho_scheduler_holder = nucleus::tile::setup::texture_scheduler(std::move(ortho_service), m_aabb_decorator, m_scheduler_thread.get());
        m_ortho_scheduler_holder.scheduler->set_gpu_quad_limit(256); // TODO
        m_scheduler_director->check_in("ortho", m_ortho_scheduler_holder.scheduler);

        auto cloud_service = std::make_unique<nucleus::tile::TileLoadService>("", TilePattern::ZXY, ".ktx2");
        m_cloud_scheduler_holder = nucleus::tile::setup::texture_scheduler_3d(std::move(cloud_service),
            m_aabb_decorator,
            m_scheduler_thread.get(),
            { .tile_resolution = webgpu_engine::clouds::TILE_RESOLUTION_XY, .max_zoom_level = 10, .gpu_quad_limit = 1024 });
        m_cloud_scheduler_holder.scheduler->set_gpu_quad_limit(webgpu_engine::clouds::LOADED_TILE_LIMIT);
        m_scheduler_director->check_in("cloud", m_cloud_scheduler_holder.scheduler);
    }
    m_geometry_scheduler_holder.scheduler->set_dataquerier(m_data_querier);

    if (QNetworkInformation::loadDefaultBackend() && QNetworkInformation::instance()) {
        QNetworkInformation* n = QNetworkInformation::instance();
        m_geometry_scheduler_holder.scheduler->set_network_reachability(n->reachability());
        m_ortho_scheduler_holder.scheduler->set_network_reachability(n->reachability());
        m_cloud_scheduler_holder.scheduler->set_network_reachability(n->reachability());
        // clang-format off
        connect(n, &QNetworkInformation::reachabilityChanged, m_geometry_scheduler_holder.scheduler.get(), &nucleus::tile::Scheduler::set_network_reachability);
        connect(n, &QNetworkInformation::reachabilityChanged, m_ortho_scheduler_holder.scheduler.get(),    &nucleus::tile::Scheduler::set_network_reachability);
        connect(n, &QNetworkInformation::reachabilityChanged, m_cloud_scheduler_holder.scheduler.get(),    &nucleus::tile::Scheduler::set_network_reachability);
        // clang-format on
    }
#ifdef ALP_ENABLE_THREADING
    qDebug() << "Scheduler thread: " << m_scheduler_thread.get();
    m_scheduler_thread->start();
#endif

    m_clous_manager = std::make_unique<clouds::Manager>();
    connect(m_clous_manager.get(), &clouds::Manager::slot_ready, this, [this](const clouds::TileSetInfo& slot) {
        QString new_url = m_clous_manager->server_url() + "/" + slot.folder + "/tiles/";
        nucleus::utils::thread::async_call(
            m_cloud_scheduler_holder.tile_service.get(), [this, new_url]() { m_cloud_scheduler_holder.tile_service->set_base_url(new_url); });
        nucleus::utils::thread::async_call(m_cloud_scheduler_holder.scheduler.get(), [this]() {
            m_cloud_scheduler_holder.scheduler->clear_full_cache();
            m_cloud_scheduler_holder.scheduler->set_enabled(true);
        });
    });

    m_search_service = std::make_unique<SearchService>();
}

void RenderingContext::initialize(webgpu::Context& ctx)
{
    auto tile_mesh_renderer = std::make_shared<webgpu_engine::TileMeshRenderer>(65, 512);
    tile_mesh_renderer->set_tile_limit(1024);
    auto cloud_renderer = std::make_shared<webgpu_engine::CloudRenderer>();
    // This doesn't really make any sense if you think about it
    cloud_renderer->set_tile_limit(webgpu_engine::clouds::LOADED_TILE_LIMIT);

    m_engine_context = std::make_unique<webgpu_engine::Context>();
    m_engine_context->set_webgpu_ctx(ctx);
    m_engine_context->set_aabb_decorator(m_aabb_decorator);
    m_engine_context->set_tile_mesh_renderer(tile_mesh_renderer);
    m_engine_context->set_cloud_renderer(cloud_renderer);
    auto overlay_renderer = std::make_shared<webgpu_engine::OverlayRenderer>();
    m_engine_context->set_overlay_renderer(overlay_renderer);
    auto track_renderer = std::make_shared<webgpu_engine::TrackRenderer>();
    m_engine_context->set_track_renderer(track_renderer);
    auto atmosphere_renderer = std::make_shared<webgpu_engine::AtmosphereRenderer>();
    m_engine_context->set_atmosphere_renderer(atmosphere_renderer);

    connect(m_geometry_scheduler_holder.scheduler.get(),
        &nucleus::tile::GeometryScheduler::gpu_tiles_updated,
        m_engine_context->tile_mesh_renderer(),
        &webgpu_engine::TileMeshRenderer::update_gpu_tiles_height);
    connect(m_ortho_scheduler_holder.scheduler.get(),
        &nucleus::tile::TextureScheduler::gpu_tiles_updated,
        m_engine_context->tile_mesh_renderer(),
        &webgpu_engine::TileMeshRenderer::update_gpu_tiles_ortho);
    connect(m_cloud_scheduler_holder.scheduler.get(),
        &nucleus::tile::Texture3DScheduler::gpu_tiles_updated,
        m_engine_context->cloud_renderer(),
        &webgpu_engine::CloudRenderer::update_gpu_tiles_cloud);
    nucleus::utils::thread::async_call(m_geometry_scheduler_holder.scheduler.get(), [this]() { m_geometry_scheduler_holder.scheduler->set_enabled(true); });

    // TODO: texture compression
    nucleus::utils::thread::async_call(m_ortho_scheduler_holder.scheduler.get(), [this]() {
        m_ortho_scheduler_holder.scheduler->set_texture_compression_algorithm(nucleus::utils::ColourTexture::Format::Uncompressed_RGBA);
        m_ortho_scheduler_holder.scheduler->set_enabled(true);
    });

    // TODO do we need to connect some destroy signals? in gl app we do this:

    // clang-format off
    //connect(QOpenGLContext::currentContext(), &QOpenGLContext::aboutToBeDestroyed, m->engine_context.get(), &nucleus::EngineContext::destroy);
    //connect(QOpenGLContext::currentContext(), &QOpenGLContext::aboutToBeDestroyed, this,                    &RenderingContext::destroy);
    //connect(QCoreApplication::instance(),     &QCoreApplication::aboutToQuit,      this,                    &RenderingContext::destroy);
    // clang-format on

    m_engine_context->initialise();

    nucleus::utils::thread::async_call(this, [this]() { emit this->initialised(); });
}

void RenderingContext::destroy()
{
    if (!m_geometry_scheduler_holder.scheduler)
        return;

    if (m_engine_context)
        m_engine_context->destroy();

    if (m_scheduler_thread) {
        nucleus::utils::thread::sync_call(m_geometry_scheduler_holder.scheduler.get(), [this]() {
            m_geometry_scheduler_holder.scheduler.reset();
            m_ortho_scheduler_holder.scheduler.reset();
            m_cloud_scheduler_holder.scheduler.reset();
        });
        nucleus::utils::thread::sync_call(m_geometry_scheduler_holder.tile_service.get(), [this]() {
            m_geometry_scheduler_holder.tile_service.reset();
            m_ortho_scheduler_holder.tile_service.reset();
            m_cloud_scheduler_holder.tile_service.reset();
        });
        m_scheduler_thread->quit();
        m_scheduler_thread->wait(500); // msec
        m_scheduler_thread.reset();
    }
}

webgpu_engine::Context* RenderingContext::engine_context() { return m_engine_context.get(); }

nucleus::tile::utils::AabbDecorator* RenderingContext::aabb_decorator() { return m_aabb_decorator.get(); }

nucleus::DataQuerier* RenderingContext::data_querier() { return m_data_querier.get(); }

nucleus::tile::GeometryScheduler* RenderingContext::geometry_scheduler() { return m_geometry_scheduler_holder.scheduler.get(); }

nucleus::tile::TileLoadService* RenderingContext::geometry_tile_load_service() { return m_geometry_scheduler_holder.tile_service.get(); }

nucleus::tile::TextureScheduler* RenderingContext::ortho_scheduler() { return m_ortho_scheduler_holder.scheduler.get(); }

nucleus::tile::Texture3DScheduler* RenderingContext::cloud_scheduler() { return m_cloud_scheduler_holder.scheduler.get(); }

nucleus::tile::SchedulerDirector* RenderingContext::scheduler_director() { return m_scheduler_director.get(); }

nucleus::tile::TileLoadService* RenderingContext::ortho_tile_load_service() { return m_ortho_scheduler_holder.tile_service.get(); }

nucleus::tile::TileLoadService* RenderingContext::cloud_tile_load_service() { return m_cloud_scheduler_holder.tile_service.get(); }

clouds::Manager* RenderingContext::clouds_manager() { return m_clous_manager.get(); }

SearchService* RenderingContext::search_service() { return m_search_service.get(); }

} // namespace webgpu_app
