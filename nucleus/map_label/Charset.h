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

#pragma once

#include <set>
#include <QFile>

#include <mutex>

namespace nucleus::maplabel {

/**
 * this class saves all chars that are neccessary for rendering in its set and a client side cache file
 * Used by VectorTileManager and LabelFactory
 * VectorTileManager calls add_chars() method with all the new chars it finds
 * when new tiles are loaded by MapLabelManager, the labelfactory checks if any new chars exists (using the is_update_necessary() method)
 * if new chars do exist it gets them using get_char_diff() and adds them to the character texture
 * simultaneously, when get_char_diff() is called this class saves the current snapshot to the client side cache location
 * -> this way, at the next startup it already creates the texture with all new chars
 */
class Charset
{
public:
    // this class is currently used as a singleton
    // probably better in the future if this class is instead passed to the neccesary files directly...
    static Charset& get_instance()
    {
        // instantiate on first use
        static Charset instance;
        return instance;
    }
    // remove copy constructors (for singleton)
    Charset(Charset const&) = delete;
    void operator=(Charset const&) = delete;


    // saves the current set into the client side-cached file
    // ideally only called internally by get_char_diff
    // if you want to explicitly save the current charset make sure to not do it too often (e.g. every frame)
//    void save();

    const std::set<char16_t> get_all_chars();

    // should only be called from VectorTileManager -> only source that adds chars to the list (other than init)
    // if other sources exist you would need to also update new_chars with the contents of m_all_chars
    void add_chars(std::set<char16_t>& new_chars);

    // quick and cheap check whether or not the char set has changed
    bool is_update_necessary(std::size_t last_size);

    // This method should only be called if is_update_necessary() returns true
    // returns a set of chars which are not present in compare set but are in the m_all_chars set
    // essentially the chars in compare set are already in the shader useable bitmap
    // and m_all_chars set are all the chars which have been encountered while parsing vector tiles
    std::set<char16_t> get_char_diff(std::set<char16_t>& compare);

private:
    // private constructor for singleton
    Charset() {init();}

    // initializes m_all_chars with server and client side-cached charset
    void init();

    // loads the given file and adds all chars to the set (called by init)
    void load(const std::filesystem::path filepath);
    // path to client side cache
    const std::filesystem::path get_charset_cache_path();

    mutable std::mutex m_file_mutex;

    std::set<char16_t> m_all_chars;
};

} // namespace nucleus::maplabel
