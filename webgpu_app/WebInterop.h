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


#include <emscripten/bind.h>
#include <QObject>


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

    static void _canvas_size_changed(int width, int height);

signals:
    void canvas_size_changed(int width, int height);

private:
    // Private constructor
    WebInterop() {}
};

