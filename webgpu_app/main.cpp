/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2024 Patrick Komon
 * Copyright (C) 2024 Gerald Kimmersdorfer
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

#include <GLFW/glfw3.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#endif

#include <GLFW/glfw3.h>
#include <QCoreApplication>
#include "TerrainRenderer.h"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    // Init QCoreApplication seems to be necessary as it declares the current thread
    // as a Qt-Thread. Otherwise functionalities like QTimers wouldnt work.
    // It basically initializes the Qt environment
    QCoreApplication app(argc, argv);

    TerrainRenderer renderer;
    renderer.start();
    // NOTE: Please be aware that for WEB-Deployment renderer.start() is non-blocking!!
    // So Code at this point will run after initialization and the first frame.
    return 0;
}
