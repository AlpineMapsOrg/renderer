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

#include "WebInterop.h"
#include <emscripten.h>
#include <emscripten/em_asm.h>
#include <emscripten/html5.h>
#include <qdebug.h>

void global_file_uploaded(const char* filename, const char* tag) { WebInterop::_file_uploaded(filename, tag); }

void WebInterop::_file_uploaded(const char* filename, const char* tag)
{
    std::string filename_str(filename);
    std::string tag_str(tag);
    qDebug() << "File uploaded: " << filename_str << " with tag: " << tag_str;
    emit WebInterop::instance().file_uploaded(filename_str, tag_str);
}

void WebInterop::open_file_dialog(const std::string& filter, const std::string& tag)
{
    EM_ASM_({ eminstance.hacks.uploadFileWithDialog(UTF8ToString($0), UTF8ToString($1)); }, filter.c_str(), tag.c_str());
}

glm::uvec2 WebInterop::get_body_size()
{
    double w, h;
    emscripten_get_element_css_size("body", &w, &h);
    return glm::uvec2(w, h);
}

static EM_BOOL _body_size_changed([[maybe_unused]] int event_type, [[maybe_unused]] const EmscriptenUiEvent* event, [[maybe_unused]] void* user_data)
{
    // NOTE: We could debounce this event, as it gets called rather often which means
    // the swapchain will be recreated very often
    emit WebInterop::instance().body_size_changed(WebInterop::instance().get_body_size());
    return 0;
}

WebInterop::WebInterop()
{
    // Setup _web_display_size_changed to be called when the window is resized
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, 0, _body_size_changed);
}
