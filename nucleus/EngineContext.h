/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Adam Celarek
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

#include "nucleus/track/Manager.h"

namespace nucleus {

class EngineContext : public QObject {
    Q_OBJECT
public:
    EngineContext(QObject* parent = nullptr);
    virtual ~EngineContext();
    void initialise();
    void destroy();
    [[nodiscard]] virtual track::Manager* track_manager() = 0;
    bool is_alive() const;

protected:
    explicit EngineContext();
    virtual void internal_initialise() = 0;
    virtual void internal_destroy() = 0;

signals:
    void initialised();

private:
    bool m_initialised = false;
    bool m_destroyed = false;
};
} // namespace nucleus
