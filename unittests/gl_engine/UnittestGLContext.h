/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
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

#pragma once

#include <QOffscreenSurface>
#include <QOpenGLWindow>

class UnittestGLContext
{
public:
    static void initialise();
    UnittestGLContext(UnittestGLContext const&) = delete;
    UnittestGLContext(UnittestGLContext const&&) = delete;
    void operator=(UnittestGLContext const&) = delete;
    void operator=(UnittestGLContext const&&) = delete;

private:
    UnittestGLContext();

    QOpenGLContext m_context;
    QOffscreenSurface surface;
};
