/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Adam Celarek
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
#include <memory>
#include <unordered_map>

namespace nucleus::tile {
class Scheduler;

class SchedulerDirector : public QObject {
    Q_OBJECT
public:
    explicit SchedulerDirector();
    bool check_in(QString name, std::shared_ptr<Scheduler> scheduler);

    template <typename Functor> void visit(Functor fun)
    {
        for (const auto& [key, value] : m_schedulers) {
            fun(value.get());
        }
    }

signals:

private:
    std::unordered_map<QString, std::shared_ptr<Scheduler>> m_schedulers;
};

} // namespace nucleus::tile
