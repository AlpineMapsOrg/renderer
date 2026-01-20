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

#include "recording.h"

#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include <QDebug>

nucleus::camera::recording::Device::Device() { }

std::vector<nucleus::camera::recording::Frame> nucleus::camera::recording::Device::recording() const { return m_frames; }

void nucleus::camera::recording::Device::reset()
{
    m_enabled = false;
    m_frames.resize(0);
}

void nucleus::camera::recording::Device::record(const Definition& def)
{
    if (!m_enabled)
        return;
    m_frames.push_back({ uint(m_stopwatch.total().count()), def.model_matrix() });
}

void nucleus::camera::recording::Device::start()
{
    m_enabled = true;
    m_stopwatch.restart();
}

void nucleus::camera::recording::Device::stop()
{
    m_enabled = false;

    // --- Configuration constants ---
    const bool REMOVE_DUPLICATES = true;
    const double DUPLICATE_EPSILON = 1e-6;
    const bool ADD_LOOP_FRAMES = true;
    const int EXTRA_FRAMES = 0;
    const int SMOOTH_WINDOW = 60;
    const double SMOOTH_STRENGTH = 1.0;
    const bool ENABLE_SMOOTHING = true;

    if (m_frames.size() < 3)
        return;

    // --- Step 1: Remove consecutive duplicate frames ---
    if (REMOVE_DUPLICATES) {
        std::vector<Frame> filtered;
        filtered.reserve(m_frames.size());

        filtered.push_back(m_frames.front());
        uint time_shift = 0;

        for (size_t i = 1; i < m_frames.size(); ++i) {
            const glm::dmat4& prev = m_frames[i - 1].camera_to_world_matrix;
            const glm::dmat4& curr = m_frames[i].camera_to_world_matrix;

            double diff_sum = 0.0;
            for (int r = 0; r < 4; ++r)
                for (int c = 0; c < 4; ++c)
                    diff_sum += std::abs(curr[r][c] - prev[r][c]);

            if (diff_sum < DUPLICATE_EPSILON) {
                time_shift += (m_frames[i].msec - m_frames[i - 1].msec);
            } else {
                Frame adjusted = m_frames[i];
                adjusted.msec -= time_shift;
                filtered.push_back(adjusted);
            }
        }

        m_frames.swap(filtered);
    }

    // --- Step 2: Optional loop extension ---
    if (ADD_LOOP_FRAMES) {
        std::vector<Frame> extended = m_frames;
        const glm::dmat4 first = m_frames.front().camera_to_world_matrix;
        const glm::dmat4 last = m_frames.back().camera_to_world_matrix;
        const uint base_time = m_frames.back().msec;
        const uint delta_t = (m_frames.size() > 1) ? (m_frames.back().msec - m_frames[m_frames.size() - 2].msec) : 16;

        for (int i = 1; i <= EXTRA_FRAMES; ++i) {
            double t = double(i) / double(EXTRA_FRAMES + 1);
            glm::dmat4 interp;
            for (int r = 0; r < 4; ++r)
                for (int c = 0; c < 4; ++c)
                    interp[r][c] = (1.0 - t) * last[r][c] + t * first[r][c];
            extended.push_back({ base_time + i * delta_t, interp });
        }

        m_frames.swap(extended);
    }

    // --- Step 3: Optional smoothing ---
    if (ENABLE_SMOOTHING) {
        const bool CYCLIC_SMOOTHING = ADD_LOOP_FRAMES;
        const size_t N = m_frames.size();

        std::vector<Frame> smoothed;
        smoothed.reserve(N);

        for (size_t i = 0; i < N; ++i) {
            glm::dmat4 accum(0.0);
            double wsum = 0.0;

            for (int offset = -SMOOTH_WINDOW; offset <= SMOOTH_WINDOW; ++offset) {
                int j = static_cast<int>(i) + offset;

                if (CYCLIC_SMOOTHING) {
                    if (j < 0)
                        j = static_cast<int>(N) + j;
                    else if (j >= static_cast<int>(N))
                        j -= static_cast<int>(N);
                }

                if (j < 0 || j >= static_cast<int>(N))
                    continue;

                double dist = double(offset);
                double w = std::exp(-0.5 * (dist / SMOOTH_WINDOW) * (dist / SMOOTH_WINDOW));
                accum += w * m_frames[j].camera_to_world_matrix;
                wsum += w;
            }

            glm::dmat4 blended = accum / wsum;

            glm::dmat4 result;
            for (int r = 0; r < 4; ++r)
                for (int c = 0; c < 4; ++c)
                    result[r][c] = (1.0 - SMOOTH_STRENGTH) * m_frames[i].camera_to_world_matrix[r][c] + SMOOTH_STRENGTH * blended[r][c];

            smoothed.push_back({ m_frames[i].msec, result });
        }

        m_frames.swap(smoothed);
    }
}
