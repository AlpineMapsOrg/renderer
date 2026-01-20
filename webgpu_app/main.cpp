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

#include "TerrainRenderer.h"
#include "util/error_logging.h"
#include <QCoreApplication>

void print_logo()
{
    const char* logo = R"(
%4        _    .  ,   .           .    *                             *         .
%4    *  / \_ *  / \_     %1   .        ___ %2___%3 ___    .    %4       /\'__       
%4      /    \  /    \,   %1__ __ _____| _ )%2_ _%3/ __|___ ___ %4 .   _/  /  \  *'.
%4 .   /\/\  /\/ :' __ \_ %1\ V  V / -_) _ \%2| |%3 (_ / -_) _ \%4  _^/  ^/    `--.
%4    /    \/  \  _/  \-'\%1 \_/\_/\___|___/%2___%3\___\___\___/%4 /.' ^_   \_   .'\ 
==============================================================================
)";
    QString formatted_logo = QString(logo).remove(0, 1);
    formatted_logo = formatted_logo.arg("\033[36m").arg("\033[38;5;245m").arg("\033[0m").arg("\033[38;5;245m");
    std::cout << formatted_logo.toStdString();
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifndef __EMSCRIPTEN__
    print_logo();
#endif
    // Init QCoreApplication is necessary as it declares the current thread
    // as a Qt-Thread. Otherwise functionalities like QTimers wouldnt work.
    // It basically initializes the Qt environment
    QCoreApplication app(argc, argv);

    // Set custom logging handler for Qt
    qInstallMessageHandler(qt_logging_callback);

    webgpu_app::TerrainRenderer renderer;
    renderer.start();
    // NOTE: Please be aware that for WEB-Deployment renderer.start() is non-blocking!!
    // So Code at this point will run after initialization
    return 0;
}
