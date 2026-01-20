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

#include "GuiManager.h"
#include "nucleus/event_parameter.h"
#include <QKeyCombination>
#include <QObject>
#include <QPoint>
#include <functional>

namespace nucleus::camera {
    class Controller;
}

namespace webgpu_app {

class InputMapper : public QObject {
    Q_OBJECT

public:
    using ViewportSizeCallback = std::function<glm::vec2()>;

    InputMapper(QObject* parent, nucleus::camera::Controller* camera_controller, GuiManager* gui_manager, ViewportSizeCallback vp_size_callback);
    void on_sdl_event(const SDL_Event& event);

signals:
    void key_pressed(QKeyCombination key);
    void key_released(QKeyCombination key);
    void mouse_moved(nucleus::event_parameter::Mouse mouse);
    void mouse_pressed(nucleus::event_parameter::Mouse mouse);
    void wheel_turned(nucleus::event_parameter::Wheel wheel);
    void touch(nucleus::event_parameter::Touch touch);


private:
    GuiManager* m_gui_manager = nullptr;
    ViewportSizeCallback m_viewport_size_callback;

    nucleus::event_parameter::Mouse m_mouse;
    std::map<SDL_Keycode, Qt::Key> m_keymap;
    std::array<Qt::MouseButton, 6> m_buttonmap; // 5 to cover all SDL mouse buttons
    std::map<int, nucleus::event_parameter::EventPoint> m_touchmap;

    void handle_key_event(const SDL_Event& event);
    void handle_mouse_button_event(const SDL_Event& event);
    void handle_mouse_motion_event(const SDL_Event& event);
    void handle_mouse_wheel_event(const SDL_Event& event);
    void handle_touch_event(const SDL_Event& event);
};

} // namespace webgpu_app
