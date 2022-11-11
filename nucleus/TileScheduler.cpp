#include "TileScheduler.h"


//TileScheduler::TileScheduler() = default;

//TileScheduler::~TileScheduler() = default;

const tile_scheduler::AabbDecoratorPtr& TileScheduler::aabb_decorator() const
{
    return m_aabb_decorator;
}

void TileScheduler::set_aabb_decorator(const tile_scheduler::AabbDecoratorPtr& new_aabb_decorator)
{
    m_aabb_decorator = new_aabb_decorator;
}
