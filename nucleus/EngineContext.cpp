/*****************************************************************************
 * Alpine Terrain Renderer
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

#include "EngineContext.h"

using namespace nucleus;

EngineContext* EngineContext::s_self = nullptr;

EngineContext::EngineContext() { }

void EngineContext::set_singleton(EngineContext* context)
{
    assert(context);
    assert(s_self == nullptr); // called several times if this fails.
    s_self = context;
}

EngineContext::~EngineContext() { assert(m_initialised == m_destroyed); }

void EngineContext::initialise()
{
    assert(!m_initialised);
    internal_initialise();
    m_initialised = true;
    emit initialised();
}

void EngineContext::destroy()
{
    assert(m_initialised);
    assert(!m_destroyed);
    internal_destroy();
    m_destroyed = true;
}

EngineContext& EngineContext::instance()
{
    assert(s_self); // EngineContext must be initialised from a subclass using set_singleton once, e.g., from the constructor
    return *s_self;
}

bool EngineContext::is_alive() const { return m_initialised && !m_destroyed; }
