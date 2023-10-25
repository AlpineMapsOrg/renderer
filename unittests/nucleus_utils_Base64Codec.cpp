/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2022 Adam Celarek
 * Copyright (C) 2023 Jakob Lindner
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

#include <QThread>
#include <catch2/catch_test_macros.hpp>

#include <limits.h>
#include "nucleus/utils/Base64Codec.h"


TEST_CASE("nucleus/utils/Base64Codec")
{
    SECTION("encode/decode binary")
    {
        char data[UCHAR_MAX];
        for (int i = 0; i < UCHAR_MAX; i++) data[i] = i;    // data now contains all possible byte values
        std::string res = Base64Codec::encode((void*)&data, UCHAR_MAX);
        char decoded[UCHAR_MAX];
        bool result = Base64Codec::decode(res, (void*)&decoded, UCHAR_MAX);
        CHECK(result == true);
        int okay_cnt = 0;
        for (int i = 0; i < UCHAR_MAX; i++) {
            if (decoded[i] == data[i]) okay_cnt++;
        }
        CHECK(okay_cnt == UCHAR_MAX);
    }

}
