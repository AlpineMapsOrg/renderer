#include "TileScheduler.h"
#include "nucleus/tile_scheduler/utils.h"
#include "sherpa/TileHeights.h"


TileScheduler::TileScheduler() {
    TileHeights h;
    h.emplace({ 0, { 0, 0 } }, { 100, 4000 });
    set_aabb_decorator(tile_scheduler::AabbDecorator::make(std::move(h)));
}

//TileScheduler::~TileScheduler() = default;

const tile_scheduler::AabbDecoratorPtr& TileScheduler::aabb_decorator() const
{
    return m_aabb_decorator;
}

void TileScheduler::set_aabb_decorator(const tile_scheduler::AabbDecoratorPtr& new_aabb_decorator)
{
    m_aabb_decorator = new_aabb_decorator;
}

void TileScheduler::print_debug_info() const
{
}

void TileScheduler::send_debug_scheduler_stats() const
{
    const auto text = QString("Scheduler: %1 tiles in transit, %2 height, %3 ortho tiles in main cache, %4 tiles on gpu")
                          .arg(numberOfTilesInTransit())
                          .arg(numberOfWaitingHeightTiles())
                          .arg(numberOfWaitingOrthoTiles())
                          .arg(gpuTiles().size());
    emit debug_scheduler_stats_updated(text);
}
