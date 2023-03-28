/*****************************************************************************
 * Alpine Terrain Renderer
 * Copyright (C) 2023 Adam Celarek
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

#include <QMouseEvent>
#include <QObject>

#include <glm/glm.hpp>

class QOpenGLFramebufferObject;

namespace tile {
struct Id;
}

namespace nucleus {
namespace tile_scheduler {
    class AabbDecorator;
    using AabbDecoratorPtr = std::shared_ptr<AabbDecorator>;
}
namespace camera {
    class Definition;
    class AbstractDepthTester;
}
struct Tile;

class AbstractRenderWindow : public QObject {
    Q_OBJECT
public:
    virtual void initialise_gpu() = 0;
    virtual void resize_framebuffer(int width, int height) = 0;
    virtual void paint(QOpenGLFramebufferObject* framebuffer = nullptr) = 0;
    virtual void deinit_gpu() = 0;
    virtual void set_permissible_screen_space_error(float new_error) = 0;
    [[nodiscard]] virtual camera::AbstractDepthTester* depth_tester() = 0;

public slots:
    virtual void update_camera(const camera::Definition& new_definition) = 0;
    virtual void update_debug_scheduler_stats(const QString& stats) = 0;
    virtual void set_aabb_decorator(const tile_scheduler::AabbDecoratorPtr&) = 0;
    virtual void add_tile(const std::shared_ptr<nucleus::Tile>&) = 0;
    virtual void remove_tile(const tile::Id&) = 0;

signals:
    void update_requested();
    void key_pressed(const QKeyCombination&) const;
    void key_released(const QKeyCombination&) const;
};

}
