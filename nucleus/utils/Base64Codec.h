/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
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

#include <string>
#include <QByteArray>

// A static helper class to encode and decode byte buffer
class Base64Codec {
public:

    static std::string encode(const void* buffer, const size_t len) {
        QByteArray byteArray((char*)buffer, len);
        return byteArray.toBase64().toStdString();
    }

    static bool decode(const std::string& base64Str, void* buffer, const size_t len) {
        QByteArray encoded(base64Str.data(), base64Str.size());
        QByteArray decoded = QByteArray::fromBase64(encoded);
        if (decoded.length() < len) return false; // buffer smaller than decoded data
        memcpy(buffer, decoded.data(), len);
        return true;
    }
};
