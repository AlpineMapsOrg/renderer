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

#include "InputMapper.h"
#include "nucleus/camera/Controller.h"
#include <QDebug>

namespace webgpu_app {

InputMapper::InputMapper(QObject* parent, nucleus::camera::Controller* camera_controller, GuiManager* gui_manager, ViewportSizeCallback vp_size_callback)
    : QObject(parent)
    , m_gui_manager(gui_manager)
    , m_viewport_size_callback(vp_size_callback)
{
    m_keymap[SDLK_a] = Qt::Key_A;
    m_keymap[SDLK_b] = Qt::Key_B;
    m_keymap[SDLK_c] = Qt::Key_C;
    m_keymap[SDLK_d] = Qt::Key_D;
    m_keymap[SDLK_e] = Qt::Key_E;
    m_keymap[SDLK_f] = Qt::Key_F;
    m_keymap[SDLK_g] = Qt::Key_G;
    m_keymap[SDLK_h] = Qt::Key_H;
    m_keymap[SDLK_i] = Qt::Key_I;
    m_keymap[SDLK_j] = Qt::Key_J;
    m_keymap[SDLK_k] = Qt::Key_K;
    m_keymap[SDLK_l] = Qt::Key_L;
    m_keymap[SDLK_m] = Qt::Key_M;
    m_keymap[SDLK_n] = Qt::Key_N;
    m_keymap[SDLK_o] = Qt::Key_O;
    m_keymap[SDLK_p] = Qt::Key_P;
    m_keymap[SDLK_q] = Qt::Key_Q;
    m_keymap[SDLK_r] = Qt::Key_R;
    m_keymap[SDLK_s] = Qt::Key_S;
    m_keymap[SDLK_t] = Qt::Key_T;
    m_keymap[SDLK_u] = Qt::Key_U;
    m_keymap[SDLK_v] = Qt::Key_V;
    m_keymap[SDLK_w] = Qt::Key_W;
    m_keymap[SDLK_x] = Qt::Key_X;
    m_keymap[SDLK_y] = Qt::Key_Y;
    m_keymap[SDLK_z] = Qt::Key_Z;

    m_keymap[SDLK_0] = Qt::Key_0;
    m_keymap[SDLK_1] = Qt::Key_1;
    m_keymap[SDLK_2] = Qt::Key_2;
    m_keymap[SDLK_3] = Qt::Key_3;
    m_keymap[SDLK_4] = Qt::Key_4;
    m_keymap[SDLK_5] = Qt::Key_5;
    m_keymap[SDLK_6] = Qt::Key_6;
    m_keymap[SDLK_7] = Qt::Key_7;
    m_keymap[SDLK_8] = Qt::Key_8;
    m_keymap[SDLK_9] = Qt::Key_9;

    m_keymap[SDLK_RETURN] = Qt::Key_Return;
    m_keymap[SDLK_ESCAPE] = Qt::Key_Escape;
    m_keymap[SDLK_BACKSPACE] = Qt::Key_Backspace;
    m_keymap[SDLK_TAB] = Qt::Key_Tab;
    m_keymap[SDLK_SPACE] = Qt::Key_Space;

    m_keymap[SDLK_LEFT] = Qt::Key_Left;
    m_keymap[SDLK_RIGHT] = Qt::Key_Right;
    m_keymap[SDLK_UP] = Qt::Key_Up;
    m_keymap[SDLK_DOWN] = Qt::Key_Down;

    m_keymap[SDLK_LCTRL] = Qt::Key_Control;
    m_keymap[SDLK_RCTRL] = Qt::Key_Control;
    m_keymap[SDLK_LSHIFT] = Qt::Key_Shift;
    m_keymap[SDLK_RSHIFT] = Qt::Key_Shift;
    m_keymap[SDLK_LALT] = Qt::Key_Alt;
    m_keymap[SDLK_RALT] = Qt::Key_Alt;

    m_keymap[SDLK_F1] = Qt::Key_F1;
    m_keymap[SDLK_F2] = Qt::Key_F2;
    m_keymap[SDLK_F3] = Qt::Key_F3;
    m_keymap[SDLK_F4] = Qt::Key_F4;
    m_keymap[SDLK_F5] = Qt::Key_F5;
    m_keymap[SDLK_F6] = Qt::Key_F6;
    m_keymap[SDLK_F7] = Qt::Key_F7;
    m_keymap[SDLK_F8] = Qt::Key_F8;
    m_keymap[SDLK_F9] = Qt::Key_F9;
    m_keymap[SDLK_F10] = Qt::Key_F10;
    m_keymap[SDLK_F11] = Qt::Key_F11;
    m_keymap[SDLK_F12] = Qt::Key_F12;

    // Initialize buttonmap
    m_buttonmap.fill(Qt::NoButton);
    m_buttonmap[SDL_BUTTON_LEFT] = Qt::LeftButton;
    m_buttonmap[SDL_BUTTON_RIGHT] = Qt::RightButton;
    m_buttonmap[SDL_BUTTON_MIDDLE] = Qt::MiddleButton;
    m_buttonmap[SDL_BUTTON_X1] = Qt::XButton1;
    m_buttonmap[SDL_BUTTON_X2] = Qt::XButton2;

    if (camera_controller) {
        connect(this, &InputMapper::key_pressed, camera_controller, &nucleus::camera::Controller::key_press);
        connect(this, &InputMapper::key_released, camera_controller, &nucleus::camera::Controller::key_release);
        connect(this, &InputMapper::mouse_moved, camera_controller, &nucleus::camera::Controller::mouse_move);
        connect(this, &InputMapper::mouse_pressed, camera_controller, &nucleus::camera::Controller::mouse_press);
        connect(this, &InputMapper::wheel_turned, camera_controller, &nucleus::camera::Controller::wheel_turn);
        connect(this, &InputMapper::touch, camera_controller, &nucleus::camera::Controller::touch);
    }
}

void InputMapper::on_sdl_event(const SDL_Event& event)
{
    // Check if it's a keyboard event
    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
        handle_key_event(event);
    } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
        handle_mouse_button_event(event);
    } else if (event.type == SDL_MOUSEMOTION) {
        handle_mouse_motion_event(event);
    } else if (event.type == SDL_MOUSEWHEEL) {
        handle_mouse_wheel_event(event);
    } else if (event.type == SDL_FINGERDOWN || event.type == SDL_FINGERUP || event.type == SDL_FINGERMOTION) {
        handle_touch_event(event);
    }
}

void InputMapper::handle_key_event(const SDL_Event& event)
{
    if (m_gui_manager && m_gui_manager->want_capture_keyboard())
        return;

    auto it = m_keymap.find(event.key.keysym.sym);
    if (it == m_keymap.end()) {
        qWarning() << "Key not mapped " << event.key.keysym.sym;
        return;
    }

    Qt::Key qtKey = it->second;
    QKeyCombination combination(qtKey);

    if (event.type == SDL_KEYDOWN) {
        emit key_pressed(combination);
    } else if (event.type == SDL_KEYUP) {
        emit key_released(combination);
    }
}

void InputMapper::handle_mouse_button_event(const SDL_Event& event)
{
    const int button = event.button.button;
    const int action = (event.type == SDL_MOUSEBUTTONDOWN) ? SDL_PRESSED : SDL_RELEASED;

    assert(button >= 0 && (size_t)button < m_buttonmap.size());

    if (m_gui_manager && m_gui_manager->want_capture_mouse())
        return;

    m_mouse.point.last_position = m_mouse.point.position;
    m_mouse.point.position = { event.button.x, event.button.y };

    const auto qtButton = m_buttonmap[button];

    if (action == SDL_RELEASED) {
        m_mouse.buttons &= ~qtButton; // Unset the button bit
    } else if (action == SDL_PRESSED) {
        m_mouse.buttons |= qtButton; // Set the button bit
    }
    emit mouse_pressed(m_mouse);
}

void InputMapper::handle_mouse_motion_event(const SDL_Event& event)
{
    if (m_gui_manager && m_gui_manager->want_capture_mouse())
        return;

    m_mouse.point.last_position = m_mouse.point.position;
    m_mouse.point.position = { event.motion.x, event.motion.y };
    emit mouse_moved(m_mouse);
}

void InputMapper::handle_mouse_wheel_event(const SDL_Event& event)
{
    if (event.type != SDL_MOUSEWHEEL)
        return;
    if (m_gui_manager && m_gui_manager->want_capture_mouse())
        return;

    nucleus::event_parameter::Wheel wheel {};
    wheel.angle_delta = QPoint(static_cast<int>(event.wheel.x), static_cast<int>(event.wheel.y) * 200.0f);

    int xpos, ypos;
    SDL_GetMouseState(&xpos, &ypos);
    wheel.point.position = { xpos, ypos };

    emit wheel_turned(wheel);
}

// NOTE: ABOUT MAPPING SDL TOUCH EVENTS TO NUCLEUS TOUCH EVENTS:
// The nucleus touch events are based on the Qt touch events. In qt a list of current touch points is maintained.
// SDL touch events are executed for each finger. So we need to maintain a list of ongoing touch points. All touch points
// that have been released will be removed from the list. (Such that they will be emitted only once with the released state)
// Additional information like pressure or gestures are not relevant in the nucleus, but this mapping function could be
// extended to forward such information aswell.
void InputMapper::handle_touch_event(const SDL_Event& event)
{
    nucleus::event_parameter::Touch touchParams;

    // First step: Remove all touch points that are not longer active (state = Released)
    for (auto it = m_touchmap.begin(); it != m_touchmap.end();) {
        if (it->second.state == nucleus::event_parameter::TouchPointReleased)
            it = m_touchmap.erase(it);
        else {
            // set all to stationary that have been added in the last call
            if (it->second.state == nucleus::event_parameter::TouchPointPressed)
                it->second.state = nucleus::event_parameter::TouchPointStationary;
            ++it;
        }
    }

    // Calculate position of the touch event in screen space
    glm::vec2 pos_screen = { event.tfinger.x, event.tfinger.y };
    pos_screen *= m_viewport_size_callback();

    switch (event.type) {
    case SDL_FINGERDOWN: {
        nucleus::event_parameter::EventPoint point;
        point.state = nucleus::event_parameter::TouchPointPressed;
        point.position = point.press_position = point.last_position = pos_screen;

        // Add the touch point to the m_touchmap
        m_touchmap[event.tfinger.fingerId] = point;

        touchParams.is_begin_event = true;
        break;
    }

    case SDL_FINGERUP: {
        auto it = m_touchmap.find(event.tfinger.fingerId);
        if (it != m_touchmap.end()) {
            it->second.state = nucleus::event_parameter::TouchPointReleased;
            it->second.position = it->second.press_position = pos_screen;
        }

        touchParams.is_end_event = true;

        // This means the last finger has been released and we can stop the touch interaction
        // if (m_touchmap.size() == 1)
        break;
    }

    case SDL_FINGERMOTION: {
        auto it = m_touchmap.find(event.tfinger.fingerId);
        if (it != m_touchmap.end()) {
            it->second.state = nucleus::event_parameter::TouchPointMoved;
            it->second.last_position = it->second.position;
            it->second.position = it->second.press_position = pos_screen;
        }

        touchParams.is_update_event = true;
        break;
    }

    default:
        qWarning() << "Unknown touch event type" << event.type;
        break;
    }

    // Populate touchParams with the ongoing touch points
    touchParams.points.reserve(m_touchmap.size());
    for (const auto& [key, value] : m_touchmap) {
        touchParams.points.push_back(value);
    }

    emit touch(touchParams);
}

} // namespace webgpu_app
