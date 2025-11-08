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

#include "TrackModel.h"

#ifdef __EMSCRIPTEN__
#include <QDir>
#include <QFile>
#include <emscripten.h>
#else
#include <QFileDialog>
#endif

#include "RenderThreadNotifier.h"
#include "RenderingContext.h"
#include <gl_engine/Context.h>

TrackModel::TrackModel(QObject* parent)
    : QObject { parent }
{
    auto* c = RenderingContext::instance();
    connect(c, &RenderingContext::initialised, this, [this, c]() {
        auto* track_manager = c->engine_context()->track_manager();
        connect(this, &TrackModel::tracks_changed, track_manager, &nucleus::track::Manager::change_tracks);
        connect(this, &TrackModel::display_width_changed, track_manager, &nucleus::track::Manager::change_display_width);
        connect(this, &TrackModel::shading_style_changed, track_manager, &nucleus::track::Manager::change_shading_style);
        connect(this, &TrackModel::tracks_changed, RenderThreadNotifier::instance(), &RenderThreadNotifier::redraw_requested);
        connect(this, &TrackModel::display_width_changed, RenderThreadNotifier::instance(), &RenderThreadNotifier::redraw_requested);
        connect(this, &TrackModel::shading_style_changed, RenderThreadNotifier::instance(), &RenderThreadNotifier::redraw_requested);
    });
}

QPointF TrackModel::lat_long(unsigned int index)
{
    if (index >= unsigned(m_data.size()))
        return {};
    const auto track = m_data.at(index);
    if (0 < track.track.size() && 0 < track.track[0].size()) {
        auto track_start = track.track[0][0];
        return { track_start.latitude, track_start.longitude };
    }
    return {};
}

#ifdef __EMSCRIPTEN__
// clang-format off
EM_ASYNC_JS(void, alpine_app_open_file_picker_and_mount, (), {
    const file_reader = new FileReader();
    const fileReadPromise = new Promise((resolve, reject) => {
        file_reader.onload = (event) => {
            try {
                const uint8Arr = new Uint8Array(event.target.result);
                const stream = FS.open("/tmp/track_upload/" + file_reader.filename.replace(/[^a-zA-Z0-9_\\-()]/g, '_'), "w+");
                FS.write(stream, uint8Arr, 0, uint8Arr.length, 0);
                FS.close(stream);
                resolve();
            } catch (error) {
                reject(error);
            }
        };
    });
    globalThis["open_file"] = function(e)
    {
        file_reader.filename = e.target.files[0].name;
        file_reader.mime_type = e.target.files[0].type;
        file_reader.readAsArrayBuffer(e.target.files[0]);
    };
    
    var file_selector = document.createElement('input');
    file_selector.setAttribute('type', 'file');
    file_selector.setAttribute('onchange', 'globalThis["open_file"](event)');
    file_selector.setAttribute('accept', '.gpx');
    file_selector.click();
    await fileReadPromise;
});
// clang-format on
#endif

void TrackModel::upload_track()
{
    auto fileContentReady = [this](const QString& /*fileName*/, const QByteArray& fileContent) {
        (void)fileContent;
        QXmlStreamReader xmlReader(fileContent);

        std::unique_ptr<nucleus::track::Gpx> gpx = nucleus::track::parse(xmlReader);
        if (gpx != nullptr) {
            m_data.push_back(*gpx);
            emit tracks_changed(m_data);
        } else {
            qDebug("Could not parse GPX file!");
        }
    };

#ifdef __EMSCRIPTEN__
    QDir dir("/tmp/track_upload/");
    dir.mkpath("/tmp/track_upload/");
    alpine_app_open_file_picker_and_mount();
    QStringList fileList = dir.entryList(QDir::Files);

    if (fileList.isEmpty()) {
        qDebug() << "No files in /tmp/track_upload/";
        return;
    }
    QString file_name = fileList.first();
    QFile file(dir.absoluteFilePath(file_name));

    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open the file:" << file_name;
        return;
    }

    QByteArray file_data = file.readAll();
    file.close();

    if (!QFile::remove(dir.absoluteFilePath(file_name))) {
        qDebug() << "Failed to delete the file:" << file_name;
    }
    fileContentReady(file_name, file_data);
#else
    const auto path = QFileDialog::getOpenFileName(nullptr, tr("Open GPX track"), "", "GPX (*.gpx *.xml)");
    auto file = QFile(path);
    if (file.open(QFile::ReadOnly))
        fileContentReady(file.fileName(), file.readAll());
    else
        qDebug() << "TrackModel::upload_track: failed to read file!" << path;
#endif
}

unsigned int TrackModel::shading_style() const { return m_shading_style; }

void TrackModel::set_shading_style(unsigned int new_shading_style)
{
    if (m_shading_style == new_shading_style)
        return;
    m_shading_style = new_shading_style;
    emit shading_style_changed(m_shading_style);
}

float TrackModel::display_width() const { return m_display_width; }

void TrackModel::set_display_width(float new_display_width)
{
    if (qFuzzyCompare(m_display_width, new_display_width))
        return;
    m_display_width = new_display_width;
    emit display_width_changed(m_display_width);
}
