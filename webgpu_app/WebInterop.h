/*****************************************************************************
 * weBIGeo
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
#pragma once

#include <QObject>
#include <emscripten.h>
#include <glm/glm.hpp>
#include <webgpu/webgpu.h>

#define JS_MAX_TOUCHES 3 // also needs changes in WebInterop.cpp and the shell and global_touch_event!

// NOTE: We switched from emscripten bindings to ccall because those can be called asynchronously.
// Otherwise we ended up with issues when functions are called from js event loop inside the WASM core.
// https://github.com/weBIGeo/webigeo/issues/25
extern "C" {
EMSCRIPTEN_KEEPALIVE
void global_file_uploaded(const char* filename, const char* tag);
}

// The WebInterop class acts as bridge between the C++ code and the JavaScript code.
// It maps the exposed Javascript functions to signals on the singleton which can be used in our QObjects.
class WebInterop : public QObject {
    Q_OBJECT

public:

    // Deleted copy constructor and copy assignment operator
    WebInterop(const WebInterop&) = delete;
    WebInterop& operator=(const WebInterop&) = delete;

    // Static method to get the instance of the class
    static WebInterop& instance() {
        static WebInterop _instance;
        return _instance;
    }

    static void _mouse_button_event(int button, int action, int mods, double xpos, double ypos);
    static void _mouse_position_event(int button, double xpos, double ypos);

    static void _file_uploaded(const char* filename, const char* tag);

    void open_file_dialog(const std::string& filter, const std::string& tag);

    glm::uvec2 get_body_size();

signals:
    void body_size_changed(glm::uvec2 size);

    void file_uploaded(const std::string& filename, const std::string& tag);

private:
    // Private constructor
    WebInterop();
};

