/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
 * Copyright (C) 2022 Adam Celarek <family name at cg tuwien ac at>
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

#include <QGuiApplication>
#include <catch2/catch_session.hpp>

int main( int argc, char* argv[] ) {
    int argc_qt = 1;
    QGuiApplication app = {argc_qt, argv};
    QCoreApplication::setOrganizationName("AlpineMaps.org");
#ifdef __ANDROID__
    std::vector<char*> argv_2;
    for (int i = 0; i < argc; ++i) {
        argv_2.push_back(argv[i]);
    }
    std::array<char, 20> logcat_switch = {"-o %debug"};
    argv_2.push_back(logcat_switch.data());
    int argc_2 = argc + 1;
    int result = Catch::Session().run( argc_2, argv_2.data() );
#else
    int result = Catch::Session().run( argc, argv );
#endif
    return result;
}
