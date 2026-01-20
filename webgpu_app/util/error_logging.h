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

#define QT_LOGGING_TO_CONSOLE 1

#include <QDebug>
#include <webgpu/webgpu.h>

#define ASCII_COLOR_CYAN "\033[36m"
#define ASCII_COLOR_BLUE "\033[34m"
#define ASCII_COLOR_YELLOW "\033[33m"
#define ASCII_COLOR_RED "\033[31m"
#define ASCII_COLOR_GRAY "\033[38;5;245m" // Gray for file names in debug
#define ASCII_COLOR_MAGENTA "\033[35m"
#define ASCII_COLOR_RESET "\033[0m"

void qt_logging_callback(QtMsgType type, const QMessageLogContext& context, const QString& msg);

void webgpu_device_error_callback(const WGPUDevice* device, WGPUErrorType type, WGPUStringView message, void* userdata1, void* userdata2);

void webgpu_device_lost_callback(const WGPUDevice* device, WGPUDeviceLostReason reason, WGPUStringView message, void* userdata1, void* userdata2);
