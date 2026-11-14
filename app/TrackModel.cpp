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

#include <QFile>
#include <QFileInfo>

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
EM_JS(void, alpine_app_open_file_picker_and_mount, (TrackModel* track_model), {
    const file_selector = document.createElement('input');
    file_selector.type = 'file';
    file_selector.accept = '.gpx';
    file_selector.addEventListener('change', event => {
        const file = event.target.files[0];
        if (!file)
            return;

        const file_reader = new FileReader();
        file_reader.addEventListener('load', load_event => {
            try {
                const data = new Uint8Array(load_event.target.result);
                const filename = file.name.replace(/[^a-zA-Z0-9_\\-()]/g, '_');
                const stream = FS.open('/tmp/track_upload/' + filename, 'w+');
                FS.write(stream, data, 0, data.length, 0);
                FS.close(stream);
                _alpine_app_track_file_ready(track_model);
            } catch (error) {
                console.error('Failed to load GPX file', error);
            }
        }, { once: true });
        file_reader.addEventListener('error', () => {
            console.error('Failed to read GPX file', file_reader.error);
        }, { once: true });
        file_reader.readAsArrayBuffer(file);
    }, { once: true });
    file_selector.click();
});
// clang-format on

extern "C" EMSCRIPTEN_KEEPALIVE void alpine_app_track_file_ready(TrackModel* track_model)
{
    if (track_model)
        track_model->finish_wasm_upload();
}
#endif

void TrackModel::add_track(const QString& file_name, const QByteArray& file_content)
{
    Q_UNUSED(file_name);
    QXmlStreamReader xml_reader(file_content);

    std::unique_ptr<nucleus::track::Gpx> gpx = nucleus::track::parse(xml_reader);
    if (gpx != nullptr) {
        m_data.push_back(*gpx);
        emit tracks_changed(m_data);
        emit track_added(lat_long(m_data.size() - 1));
    } else {
        qWarning() << "TrackModel::add_track: could not parse GPX file!";
    }
}

#ifdef __EMSCRIPTEN__
void TrackModel::finish_wasm_upload()
{
    QDir dir("/tmp/track_upload/");
    const QStringList file_list = dir.entryList(QDir::Files);

    if (file_list.isEmpty()) {
        qDebug() << "No files in /tmp/track_upload/";
        return;
    }
    const QString file_name = file_list.first();
    QFile file(dir.absoluteFilePath(file_name));

    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open the file:" << file_name;
        return;
    }

    const QByteArray file_data = file.readAll();
    file.close();

    if (!QFile::remove(dir.absoluteFilePath(file_name))) {
        qDebug() << "Failed to delete the file:" << file_name;
    }
    add_track(file_name, file_data);
}
#endif

void TrackModel::import_track(const QUrl& file_url)
{
    // android file dialogs return content:// uris, which QFile can open but
    // which have no local file path
    const QString path = file_url.isLocalFile() ? file_url.toLocalFile() : file_url.toString();
    QFile file(path);
    if (!file.open(QFile::ReadOnly)) {
        qWarning() << "TrackModel::import_track: failed to open file!" << path;
        return;
    }
    add_track(QFileInfo(path).fileName(), file.readAll());
}

void TrackModel::upload_track()
{
#ifdef __EMSCRIPTEN__
    QDir().mkpath("/tmp/track_upload/");
    alpine_app_open_file_picker_and_mount(this);
#else
    const auto path = QFileDialog::getOpenFileName(nullptr, tr("Open GPX track"), "", "GPX (*.gpx *.xml)");
    auto file = QFile(path);
    if (file.open(QFile::ReadOnly))
        add_track(file.fileName(), file.readAll());
    else
        qWarning() << "TrackModel::upload_track: failed to read file!" << path;
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
