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

#include "Charset.h"

#include <QTextStream>
#include <QStandardPaths>
#include <iostream>

namespace nucleus::maplabel {

void Charset::init()
{
    // load charset from main file (located on server)
    load(":/charset/charset.txt");

    // after that try to load a client side cached charset file
    // merge them and save the updated cache file
//    load(get_charset_cache_path());
//    save();
}

// path to a client side cached charset file
const std::filesystem::path Charset::get_charset_cache_path()
{
    const auto base_path = std::filesystem::path(QStandardPaths::writableLocation(QStandardPaths::CacheLocation).toStdString());
    std::filesystem::create_directories(base_path);

    return  base_path / "charset.txt";
}

//void Charset::save()
//{
//    auto locker = std::scoped_lock(m_file_mutex);

//    QFile file(get_charset_cache_path());
//    const auto open = file.open(QIODeviceBase::OpenModeFlag::WriteOnly | QIODeviceBase::OpenModeFlag::Text);
//    assert(open);

//    QTextStream out(&file);

//    for(auto c : m_all_chars)
//    {
//        out << c;
//    }

//    file.close();
//}

void Charset::load(const std::filesystem::path filepath)
{
     auto locker = std::scoped_lock(m_file_mutex);

    QFile file(filepath);
    const auto open = file.open(QIODeviceBase::OpenModeFlag::ReadOnly | QIODeviceBase::OpenModeFlag::Text);
    if(!open)
    {
        // file doesnt exist (yet)
        return;
    }

    auto chars = QString(file.readAll()).toStdU16String();
    m_all_chars.insert(chars.begin(), chars.end());

    file.close();
}

const std::set<char16_t> Charset::get_all_chars()
{
    return m_all_chars;
}

void Charset::add_chars(std::set<char16_t>& new_chars)
{
    m_all_chars.insert(new_chars.begin(), new_chars.end());
}

bool Charset::is_update_necessary(std::size_t last_size)
{
    return m_all_chars.size() != last_size;
}

std::set<char16_t> Charset::get_char_diff(std::set<char16_t>& compare)
{
    std::set<char16_t> new_chars;

    // if something changed we still have to compare and set the new size
    std::set_difference(m_all_chars.begin(), m_all_chars.end(), compare.begin(), compare.end(), std::inserter(new_chars, new_chars.begin()));

    std::cout << "newwwww: " << m_all_chars.size() <<" "<< compare.size() << std::endl;

    return new_chars;
}

}
