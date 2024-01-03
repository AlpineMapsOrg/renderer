/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2024 Lucas Dworschak
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

#include "CharUtils.h"

namespace nucleus {

// utf-8 characters in strings are encoded using multiple bytes
// while iterating a string only one byte is shown
// we therefore need a method to catch those cases and and convert a string into a list of integers (where unicode characters are represented as one singular integer)
const std::vector<int> CharUtils::string_to_unicode_int_list(std::string text)
{
    std::vector<int> safe_chars;
    bool special_char = false;

    // utf-8 decoding explanation: https://dl.acm.org/doi/pdf/10.17487/RFC3629 (page 4)
    int unicode2_test = 0b11000000; // next unicode char contains 2 bytes
    int unicode2_mask = 0b00011111; // how many bytes are usable
    int unicode3_test = 0b11100000; // next unicode char contains 3 bytes
    int unicode3_mask = 0b00001111; // how many bytes are usable
    int unicode4_test = 0b11110000; // next unicode char contains 4 bytes
    int unicode4_mask = 0b00000111; // how many bytes are usable

    int unicode_rest_test = 0b10000000;
    int unicode_rest_mask = 0b00111111;

    short bytes_in_unicode = 0;
    constexpr int unicode_shift = 6;
    int current_unicode_char_index = 0;

    for (int i = 0; i < text.size(); i++) {
        int char_index = int(text[i]);

        // we have a unicode char -> that stretches multiple bytes
        if (char_index < 0) {
            if ((char_index & unicode4_test) == unicode4_test) {
                // next unicode char contains 4 bytes
                bytes_in_unicode = 3;
                current_unicode_char_index = ((char_index & unicode4_mask) << (unicode_shift * bytes_in_unicode));
            } else if ((char_index & unicode3_test) == unicode3_test) {
                // next unicode char contains 3 bytes
                bytes_in_unicode = 2;
                current_unicode_char_index = ((char_index & unicode3_mask) << (unicode_shift * bytes_in_unicode));
            } else if ((char_index & unicode2_test) == unicode2_test) {
                // next unicode char contains 2 bytes
                bytes_in_unicode = 1;
                current_unicode_char_index = ((char_index & unicode2_mask) << unicode_shift);
            } else if ((char_index & unicode_rest_test) == unicode_rest_test) {
                // we are within the unicode bytes and have to parse the rest with the rest_mask
                bytes_in_unicode--;
                current_unicode_char_index |= ((char_index & unicode_rest_mask) << (unicode_shift * bytes_in_unicode));
            }

            if (bytes_in_unicode == 0) {
                // we are finished parsing the current unicode char
                safe_chars.push_back(current_unicode_char_index);
            }
        } else {
            // normal "ascii" character -> can be used directly
            safe_chars.push_back(char_index);
        }
    }

    return std::move(safe_chars);
}

} // namespace nucleus
