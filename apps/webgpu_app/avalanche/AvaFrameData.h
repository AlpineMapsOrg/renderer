/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2026 Manuel Eiweck
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

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <map>
#include <vector>

class QNetworkAccessManager;
class QNetworkReply;

namespace webgpu_app {

/* The AvaFrame sandbox datasets, prepared in the app: a C++ port of
 * mpm-mls-doc/avaframe/prepare_dataset.py for the five real test avalanches. The inputs are
 * downloaded at run time from AvaFrame's public repository, pinned to the commit of release 2.1,
 * so weBIGeo does not redistribute them (see 11-avaframe-comparison.md, "Native build only").
 *
 * The output is the same as the script's: dataset.json, dem.asc, release_<scenario>.asc,
 * reference/<benchmark>/{pft,pfv,ppr}.asc and params.json, path.json, and the dataset's entry
 * in index.json. Not ported: the preview and map overlay PNGs, which only served as check images
 * (the app draws both itself), and the synthetic datasets. Existing run results in <dataset>/mpm
 * stay; unlike the script, nothing is deleted. */
namespace avaframe_data {

    constexpr const char* AVAFRAME_TAG = "2.1";
    constexpr const char* AVAFRAME_COMMIT = "9c3cefb89d626f5c0fb4c0329b41bc0c492ce175";
    /// Raw files of AvaFrame's repository at AVAFRAME_COMMIT (served with CORS, so the web build can fetch them).
    QString source_url();

    struct Benchmark {
        const char* name; // folder in benchmarks/, e.g. avaKotNullTest
        const char* stem; // file stem of its rasters, <stem>_pft.asc etc.
    };
    struct Source {
        const char* name; // e.g. avaKot
        std::vector<const char*> scenarios; // release shapefiles in Inputs/REL, sorted like the script's glob
        std::vector<Benchmark> benchmarks; // com1DFA null tests of this dataset, sorted by folder
        const char* entrainment = nullptr; // entrainment shapefile in Inputs/ENT, e.g. entKot; nullptr = none
    };
    /// com1DFA defaults for the entrainment area (com1DFACfg.ini, AvaFrame 2.1): the thickness of
    /// a feature without one (entThIfMissingInShp) and the density of the entrained snow (rhoEnt).
    constexpr double ENT_THICKNESS_IF_MISSING = 0.3; // [m]
    constexpr double ENT_DENSITY = 100.0; // [kg/m^3]
    /// The five real datasets with a com1DFA reference: avaKot, avaGar, avaHit, avaWog, avaMal.
    const std::vector<Source>& real_datasets();
    /// Repository paths of every file a dataset needs.
    std::vector<QString> files_of(const Source& source);

    /// Prepares one dataset from its downloaded files (repository path -> content) into
    /// <out_root>/<name>/. Returns the dataset's index.json entry, or an empty object and an error.
    QJsonObject prepare(const Source& source, const std::map<QString, QByteArray>& files, const QString& out_root, QString& error);
    /// Adds or replaces these entries in <out_root>/index.json, keeping the other datasets.
    bool update_index(const QString& out_root, const std::vector<QJsonObject>& entries, QString& error);

    /// MGI / Austria Lambert (EPSG:31287) to WGS 84 latitude, longitude [degrees], the way
    /// pyproj does it for the script: inverse Lambert conformal conic on Bessel 1841, then the
    /// 7-parameter Helmert transformation "MGI to WGS 84 (8)" (stated accuracy 0.5 m).
    void austria_lambert_to_wgs84(double x, double y, double& lat, double& lon);

} // namespace avaframe_data

/// Downloads and prepares the real AvaFrame datasets (avaframe_data). Asynchronous: progress
/// and status are polled by the panel, finished() fires once at the end.
class AvaFrameDownload : public QObject {
    Q_OBJECT

public:
    enum class State { Idle, Downloading, Preparing, Done, Failed, Cancelled };

    explicit AvaFrameDownload(QObject* parent = nullptr);
    ~AvaFrameDownload() override;

    /// Starts over: downloads every file, then prepares the datasets into out_root.
    void start(const QString& out_root);
    void cancel();

    State state() const { return m_state; }
    bool running() const { return m_state == State::Downloading || m_state == State::Preparing; }
    /// One line for the panel: what is happening, or what went wrong.
    const QString& status() const { return m_status; }
    float progress() const; // [0, 1]

signals:
    void finished(bool ok);

private:
    void request_next();
    void on_reply(QNetworkReply* reply);
    void prepare_next();
    void on_prepared(const QJsonObject& entry, const QString& error);
    void fail(const QString& message);

    QNetworkAccessManager* m_network;
    State m_state = State::Idle;
    QString m_status;
    QString m_out_root;
    std::vector<QString> m_queue; // repository paths still to request
    std::vector<QNetworkReply*> m_in_flight;
    std::map<QString, QByteArray> m_files;
    size_t m_file_count = 0;
    qint64 m_bytes = 0;
    size_t m_dataset = 0; // next dataset to prepare
    int m_generation = 0; // per start(), so a result of a cancelled run is dropped
    std::vector<QJsonObject> m_entries;
};

} // namespace webgpu_app
