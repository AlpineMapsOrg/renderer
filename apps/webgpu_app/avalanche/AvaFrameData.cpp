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

#include "AvaFrameData.h"

#include <QByteArrayView>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QThreadPool>
#include <QCoreApplication>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <numbers>
#include <optional>

namespace webgpu_app {

namespace avaframe_data {

    namespace {

        constexpr double NODATA = -9999.0;
        constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
        constexpr double DEG = std::numbers::pi / 180.0;

        /* Numbers are parsed and printed with Qt's own conversions: they ignore the C locale, which
         * Qt sets from the environment (a German locale would make strtod stop at the '.'). */

        // -----------------------------------------------------------------------------------------
        // ESRI ASCII grid, as prepare_dataset.py reads and writes it
        // -----------------------------------------------------------------------------------------

        /// Row 0 is the northern edge; (xll, yll) the lower-left corner. NODATA is NaN.
        struct Grid {
            int ncols = 0;
            int nrows = 0;
            double xll = 0.0;
            double yll = 0.0;
            double cellsize = 0.0;
            std::vector<double> data;
            double at(int row, int col) const { return data[size_t(row) * size_t(ncols) + size_t(col)]; }
            size_t cells() const { return data.size(); }
            bool same_grid(const Grid& o) const
            {
                return ncols == o.ncols && nrows == o.nrows && std::abs(xll - o.xll) < 1e-3 * cellsize && std::abs(yll - o.yll) < 1e-3 * cellsize
                    && std::abs(cellsize - o.cellsize) < 1e-6;
            }
        };

        bool is_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

        /// The next whitespace-separated token, or an empty view at the end.
        QByteArrayView next_token(const char*& p, const char* end)
        {
            while (p < end && is_space(*p))
                p++;
            const char* start = p;
            while (p < end && !is_space(*p))
                p++;
            return QByteArrayView(start, p - start);
        }

        std::optional<double> to_double(QByteArrayView token)
        {
            if (token.compare("nan", Qt::CaseInsensitive) == 0)
                return NaN;
            bool ok = false;
            const double v = token.toDouble(&ok);
            return ok ? std::optional<double>(v) : std::nullopt;
        }

        std::optional<Grid> read_asc(const QByteArray& bytes, const QString& name, QString& error)
        {
            const char* p = bytes.constData();
            const char* end = p + bytes.size();
            std::map<QByteArray, double> header;
            // Up to six "key value" lines; the data starts at the first line without a key.
            for (int i = 0; i < 6; i++) {
                const char* line = p;
                const QByteArrayView key = next_token(p, end);
                if (key.isEmpty() || !std::isalpha(static_cast<unsigned char>(key[0]))) {
                    p = line;
                    break;
                }
                const auto value = to_double(next_token(p, end));
                if (!value) {
                    error = name + ": bad header value for " + key.toByteArray();
                    return std::nullopt;
                }
                header[key.toByteArray().toLower()] = *value;
            }
            for (const char* key : { "ncols", "nrows", "cellsize" }) {
                if (!header.contains(key)) {
                    error = name + ": no " + key + " in the header";
                    return std::nullopt;
                }
            }
            Grid grid;
            grid.ncols = int(header["ncols"]);
            grid.nrows = int(header["nrows"]);
            grid.cellsize = header["cellsize"];
            // xllcenter / yllcenter refer to the centre of the lower-left cell; normalise to corners.
            grid.xll = header.contains("xllcorner") ? header["xllcorner"] : header["xllcenter"] - grid.cellsize / 2;
            grid.yll = header.contains("yllcorner") ? header["yllcorner"] : header["yllcenter"] - grid.cellsize / 2;
            const double nodata = header.contains("nodata_value") ? header["nodata_value"] : NODATA;
            const size_t count = size_t(grid.ncols) * size_t(grid.nrows);
            grid.data.reserve(count);
            for (QByteArrayView token = next_token(p, end); !token.isEmpty(); token = next_token(p, end)) {
                const auto value = to_double(token);
                if (!value) {
                    error = name + ": bad value '" + token.toByteArray() + "'";
                    return std::nullopt;
                }
                grid.data.push_back(*value == nodata ? NaN : *value);
            }
            if (grid.data.size() != count) {
                error = QString("%1: %2 values, the header says %3x%4").arg(name).arg(grid.data.size()).arg(grid.nrows).arg(grid.ncols);
                return std::nullopt;
            }
            return grid;
        }

        /// printf's "%.<decimals>f" as Python has it: an exact tie (1053.5625 to 3 decimals) goes
        /// to the even digit, where Qt would round it away from zero.
        QByteArray format_fixed(double v, int decimals)
        {
            const double scale = std::pow(10.0, decimals);
            const double scaled = v * scale;
            const bool exact = std::fma(v, scale, -scaled) == 0.0;
            if (exact && scaled - std::floor(scaled) == 0.5)
                return QByteArray::number(std::nearbyint(scaled) / scale, 'f', decimals); // ties to even
            return QByteArray::number(v, 'f', decimals);
        }

        /// numpy.savetxt with "%.<decimals>f", NaN as NODATA; the header as the script writes it.
        bool write_asc(const QString& path, const Grid& grid, const std::vector<double>& data, int decimals)
        {
            QByteArray out;
            out.reserve(qsizetype(data.size()) * (decimals + 6));
            out += "ncols " + QByteArray::number(grid.ncols) + "\nnrows " + QByteArray::number(grid.nrows) + "\nxllcorner "
                + QByteArray::number(grid.xll, 'f', 6) + "\nyllcorner " + QByteArray::number(grid.yll, 'f', 6) + "\ncellsize "
                + QByteArray::number(grid.cellsize, 'f', 9) + "\nNODATA_value -9999\n";
            for (int row = 0; row < grid.nrows; row++) {
                for (int col = 0; col < grid.ncols; col++) {
                    const double v = data[size_t(row) * size_t(grid.ncols) + size_t(col)];
                    if (col)
                        out += ' ';
                    out += format_fixed(std::isnan(v) ? NODATA : v, decimals);
                }
                out += '\n';
            }
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(out) == out.size();
        }

        /// numpy.gradient: central differences inside, one-sided at the edges.
        double gradient(const Grid& g, int row, int col, bool along_rows)
        {
            const int n = along_rows ? g.nrows : g.ncols;
            const int i = along_rows ? row : col;
            const auto f = [&](int j) { return along_rows ? g.at(j, col) : g.at(row, j); };
            if (n < 2)
                return 0.0;
            if (i == 0)
                return (f(1) - f(0)) / g.cellsize;
            if (i == n - 1)
                return (f(n - 1) - f(n - 2)) / g.cellsize;
            return (f(i + 1) - f(i - 1)) / (2.0 * g.cellsize);
        }

        /// cos of the terrain slope (1 on flat ground, NaN next to NODATA).
        double slope_cos(const Grid& g, int row, int col)
        {
            const double dzdy = gradient(g, row, col, true);
            const double dzdx = gradient(g, row, col, false);
            return 1.0 / std::sqrt(1.0 + dzdx * dzdx + dzdy * dzdy);
        }

        double round_to(double v, int decimals)
        {
            const double f = std::pow(10.0, decimals);
            return std::round(v * f) / f;
        }

        // -----------------------------------------------------------------------------------------
        // Shapefiles (point 1, polyline 3, polygon 5) and their dBASE tables
        // -----------------------------------------------------------------------------------------

        struct Point {
            double x;
            double y;
        };
        using Part = std::vector<Point>;
        using Shape = std::vector<Part>;

        template <typename T> T read_le(const QByteArray& b, qsizetype off)
        {
            T v;
            std::memcpy(&v, b.constData() + off, sizeof(T)); // little-endian hosts only (x86, ARM, wasm)
            return v;
        }

        int32_t read_be32(const QByteArray& b, qsizetype off)
        {
            const auto* u = reinterpret_cast<const unsigned char*>(b.constData() + off);
            return int32_t(uint32_t(u[0]) << 24 | uint32_t(u[1]) << 16 | uint32_t(u[2]) << 8 | uint32_t(u[3]));
        }

        std::optional<std::vector<Shape>> read_shp(const QByteArray& b, const QString& name, QString& error)
        {
            std::vector<Shape> shapes;
            qsizetype off = 100;
            while (off + 8 <= b.size()) {
                const qsizetype content = qsizetype(read_be32(b, off + 4)) * 2;
                const qsizetype rec = off + 8;
                off = rec + content;
                if (off > b.size() || content < 4) {
                    error = name + ": truncated record";
                    return std::nullopt;
                }
                const int32_t type = read_le<int32_t>(b, rec);
                if (type == 0) {
                    shapes.emplace_back();
                } else if (type == 1) {
                    shapes.push_back({ { { read_le<double>(b, rec + 4), read_le<double>(b, rec + 12) } } });
                } else if (type == 3 || type == 5) {
                    const int32_t nparts = read_le<int32_t>(b, rec + 36);
                    const int32_t npoints = read_le<int32_t>(b, rec + 40);
                    const qsizetype points = rec + 44 + 4 * qsizetype(nparts);
                    if (nparts < 0 || npoints < 0 || points + 16 * qsizetype(npoints) > rec + content) {
                        error = name + ": bad record";
                        return std::nullopt;
                    }
                    Shape shape;
                    for (int32_t i = 0; i < nparts; i++) {
                        const int32_t first = read_le<int32_t>(b, rec + 44 + 4 * i);
                        const int32_t last = i + 1 < nparts ? read_le<int32_t>(b, rec + 44 + 4 * (i + 1)) : npoints;
                        Part part;
                        for (int32_t k = std::max(first, 0); k < std::min(last, npoints); k++)
                            part.push_back({ read_le<double>(b, points + 16 * k), read_le<double>(b, points + 16 * k + 8) });
                        shape.push_back(std::move(part));
                    }
                    shapes.push_back(std::move(shape));
                } else {
                    error = name + ": unsupported shape type " + QString::number(type);
                    return std::nullopt;
                }
            }
            return shapes;
        }

        /// One map per record: field name -> trimmed text (latin1).
        std::vector<std::map<QString, QString>> read_dbf(const QByteArray& b)
        {
            std::vector<std::map<QString, QString>> records;
            if (b.size() < 32)
                return records;
            const uint32_t n = read_le<uint32_t>(b, 4);
            const uint16_t header_len = read_le<uint16_t>(b, 8);
            const uint16_t record_len = read_le<uint16_t>(b, 10);
            std::vector<std::pair<QString, int>> fields;
            for (qsizetype i = 32; i + 17 <= b.size() && b[i] != 0x0D; i += 32)
                fields.emplace_back(QString::fromLatin1(b.mid(i, 11).split('\0').front()), static_cast<unsigned char>(b[i + 16]));
            for (uint32_t r = 0; r < n; r++) {
                qsizetype off = qsizetype(header_len) + qsizetype(r) * record_len + 1; // after the deletion flag
                std::map<QString, QString> rec;
                for (const auto& [field, length] : fields) {
                    rec[field] = QString::fromLatin1(b.mid(off, length)).trimmed();
                    off += length;
                }
                records.push_back(std::move(rec));
            }
            return records;
        }

        /// Cells whose centre lies inside the shape (even-odd rule over all its rings, so holes
        /// are holes), exactly as the script's vectorised test: per row, the edges crossing the
        /// centre line and their x there; a cell is inside when an odd number lies right of it.
        std::vector<uint8_t> rasterise(const Grid& g, const Shape& shape)
        {
            std::vector<uint8_t> mask(g.cells(), 0);
            std::vector<double> crossings;
            for (int row = 0; row < g.nrows; row++) {
                const double yc = g.yll + (g.nrows - row - 0.5) * g.cellsize;
                crossings.clear();
                for (const Part& ring : shape) {
                    for (size_t i = 0; i + 1 < ring.size(); i++) {
                        const Point a = ring[i];
                        const Point b = ring[i + 1];
                        if (a.y == b.y || (a.y > yc) == (b.y > yc))
                            continue;
                        crossings.push_back(a.x + (yc - a.y) * (b.x - a.x) / (b.y - a.y));
                    }
                }
                if (crossings.empty())
                    continue;
                std::sort(crossings.begin(), crossings.end());
                for (int col = 0; col < g.ncols; col++) {
                    const double xc = g.xll + (col + 0.5) * g.cellsize;
                    const auto right = crossings.end() - std::upper_bound(crossings.begin(), crossings.end(), xc);
                    mask[size_t(row) * size_t(g.ncols) + size_t(col)] = uint8_t(right % 2);
                }
            }
            return mask;
        }

        QJsonValue text_or_null(const std::map<QString, QString>& rec, const QString& key)
        {
            const auto it = rec.find(key);
            return it == rec.end() ? QJsonValue(QJsonValue::Null) : QJsonValue(it->second);
        }

        QString data_path(const Source& s) { return QString("avaframe/data/%1/Inputs").arg(s.name); }

    } // namespace

    // ---------------------------------------------------------------------------------------------
    // Georeferencing
    // ---------------------------------------------------------------------------------------------

    void austria_lambert_to_wgs84(double x, double y, double& lat, double& lon)
    {
        // Inverse Lambert conformal conic, two standard parallels (Snyder 1987, eq. 15-1 ff.),
        // with the parameters of AvaFrame's .prj (= EPSG:31287).
        constexpr double a = 6377397.155;
        constexpr double f = 1.0 / 299.1528128;
        const double e2 = f * (2.0 - f);
        const double e = std::sqrt(e2);
        const double phi1 = 49.0 * DEG, phi2 = 46.0 * DEG, phi0 = 47.5 * DEG, lam0 = 13.3333333333333 * DEG;
        const auto m = [&](double p) { return std::cos(p) / std::sqrt(1.0 - e2 * std::sin(p) * std::sin(p)); };
        const auto t = [&](double p) {
            return std::tan(std::numbers::pi / 4.0 - p / 2.0) / std::pow((1.0 - e * std::sin(p)) / (1.0 + e * std::sin(p)), e / 2.0);
        };
        const double n = (std::log(m(phi1)) - std::log(m(phi2))) / (std::log(t(phi1)) - std::log(t(phi2)));
        const double F = m(phi1) / (n * std::pow(t(phi1), n));
        const double rho0 = a * F * std::pow(t(phi0), n);
        const double dx = x - 400000.0;
        const double dy = rho0 - (y - 400000.0);
        const double rho = std::hypot(dx, dy);
        const double tt = std::pow(rho / (a * F), 1.0 / n);
        const double lam = std::atan2(dx, dy) / n + lam0;
        double phi = std::numbers::pi / 2.0 - 2.0 * std::atan(tt);
        for (int i = 0; i < 20; i++)
            phi = std::numbers::pi / 2.0 - 2.0 * std::atan(tt * std::pow((1.0 - e * std::sin(phi)) / (1.0 + e * std::sin(phi)), e / 2.0));

        // Geocentric on Bessel 1841 (height 0, as the 2D transformation keeps it).
        const double N = a / std::sqrt(1.0 - e2 * std::sin(phi) * std::sin(phi));
        const double X = N * std::cos(phi) * std::cos(lam);
        const double Y = N * std::cos(phi) * std::sin(lam);
        const double Z = N * (1.0 - e2) * std::sin(phi);

        // "MGI to WGS 84 (8)", coordinate frame convention, small-angle rotation like PROJ's
        // helmert step without +exact.
        constexpr double arcsec = DEG / 3600.0;
        const double rx = -4.7354 * arcsec, ry = -1.3145 * arcsec, rz = -5.393 * arcsec;
        const double scale = 1.0 + (-2.3887e-6);
        const double X2 = 601.705 + scale * (X + rz * Y - ry * Z);
        const double Y2 = 84.263 + scale * (-rz * X + Y + rx * Z);
        const double Z2 = 485.227 + scale * (ry * X - rx * Y + Z);

        // Back to geodetic on WGS 84.
        constexpr double wa = 6378137.0;
        constexpr double wf = 1.0 / 298.257223563;
        const double we2 = wf * (2.0 - wf);
        const double p = std::hypot(X2, Y2);
        double wphi = std::atan2(Z2, p * (1.0 - we2));
        for (int i = 0; i < 20; i++) {
            const double wn = wa / std::sqrt(1.0 - we2 * std::sin(wphi) * std::sin(wphi));
            wphi = std::atan2(Z2 + we2 * wn * std::sin(wphi), p);
        }
        lat = wphi / DEG;
        lon = std::atan2(Y2, X2) / DEG;
    }

    // ---------------------------------------------------------------------------------------------
    // Datasets
    // ---------------------------------------------------------------------------------------------

    QString source_url() { return QString("https://raw.githubusercontent.com/OpenNHM/AvaFrame/%1/").arg(AVAFRAME_COMMIT); }

    const std::vector<Source>& real_datasets()
    {
        static const std::vector<Source> datasets = {
            { "avaKot", { "relKot" }, { { "avaKotNullTest", "relKot_989874a9be_C_S_null_dfa" } } },
            { "avaGar", { "relGar", "relGar2", "relGar6" },
                { { "avaGarNullTest", "relGar_8d83c84a99_C_L_null_dfa" }, { "avaGarNullTest2", "relGar2_9598ec5adc_C_L_null_dfa" },
                    { "avaGarNullTest6", "relGar6_9dd147f24b_C_L_null_dfa" } } },
            { "avaHit", { "relHit" }, { { "avaHitNullTest", "relHit_8e7eda03a1_C_L_null_dfa" } } },
            { "avaWog", { "relWog" }, { { "avaWogNullTest", "relWog_005698a9de_C_L_null_dfa" } } },
            { "avaMal", { "relMal1to3" }, { { "avaMalNullTest", "relMal1to3_e94d7bcc61_C_L_null_dfa" } } },
        };
        return datasets;
    }

    std::vector<QString> files_of(const Source& s)
    {
        const QString inputs = data_path(s);
        std::vector<QString> files { inputs + "/" + s.name + ".asc", inputs + "/LINES/path_aimec.shp", inputs + "/POINTS/splitPoint.shp",
            inputs + "/REL/" + s.scenarios.front() + ".prj" };
        for (const char* scenario : s.scenarios) {
            files.push_back(inputs + "/REL/" + scenario + ".shp");
            files.push_back(inputs + "/REL/" + scenario + ".dbf");
        }
        for (const Benchmark& b : s.benchmarks) {
            const QString dir = QString("benchmarks/%1/").arg(b.name);
            files.push_back(dir + b.name + "_desDict.json");
            for (const char* kind : { "pft", "pfv", "ppr" })
                files.push_back(dir + b.stem + "_" + kind + ".asc");
        }
        return files;
    }

    QJsonObject prepare(const Source& s, const std::map<QString, QByteArray>& files, const QString& out_root, QString& error)
    {
        const QString inputs = data_path(s);
        const auto file = [&](const QString& path) -> const QByteArray* {
            const auto it = files.find(path);
            if (it == files.end())
                error = "missing " + path;
            return it == files.end() ? nullptr : &it->second;
        };

        // Only Austria Lambert is georeferenced here (the real datasets all are).
        const QByteArray* prj = file(inputs + "/REL/" + s.scenarios.front() + ".prj");
        if (prj == nullptr)
            return {};
        if (!prj->contains("Austria_Lambert") || !prj->contains("Bessel")) {
            error = QString("%1: unexpected coordinate system in the .prj").arg(s.name);
            return {};
        }

        const QString dem_name = QString(s.name) + ".asc";
        const QByteArray* dem_bytes = file(inputs + "/" + dem_name);
        if (dem_bytes == nullptr)
            return {};
        const auto dem_read = read_asc(*dem_bytes, dem_name, error);
        if (!dem_read)
            return {};
        const Grid& dem = *dem_read;

        QDir out(out_root);
        if (!out.mkpath(QString(s.name) + "/reference") || !out.cd(s.name)) {
            error = "cannot create " + out.filePath(s.name);
            return {};
        }
        if (!write_asc(out.filePath("dem.asc"), dem, dem.data, 3)) {
            error = "cannot write " + out.filePath("dem.asc");
            return {};
        }

        const double cell_area = dem.cellsize * dem.cellsize;
        const double x0 = dem.xll, y0 = dem.yll;
        const double x1 = dem.xll + dem.ncols * dem.cellsize, y1 = dem.yll + dem.nrows * dem.cellsize;
        double lo = std::numeric_limits<double>::max(), hi = std::numeric_limits<double>::lowest();
        size_t nodata_cells = 0;
        for (const double v : dem.data) {
            if (std::isnan(v)) {
                nodata_cells++;
                continue;
            }
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }

        QJsonObject meta;
        meta["name"] = s.name;
        meta["source"] = QJsonObject { { "avaframe_version", AVAFRAME_TAG }, { "avaframe_commit", AVAFRAME_COMMIT }, { "inputs", inputs },
            { "downloaded_from", source_url() }, { "prepared_by", "weBIGeo (apps/webgpu_app/avalanche/AvaFrameData.cpp)" } };
        meta["crs"] = "EPSG:31287 (MGI / Austria Lambert)";
        meta["crs_note"] = QJsonValue::Null;
        meta["dem"] = QJsonObject { { "file", "dem.asc" }, { "source", dem_name }, { "ncols", dem.ncols }, { "nrows", dem.nrows },
            { "cellsize", dem.cellsize }, { "xllcorner", dem.xll }, { "yllcorner", dem.yll },
            { "extent_m", QJsonArray { round_to(dem.ncols * dem.cellsize, 1), round_to(dem.nrows * dem.cellsize, 1) } },
            { "altitude_m", QJsonArray { round_to(lo, 1), round_to(hi, 1) } }, { "nodata_cells", qint64(nodata_cells) } };

        // Corners SW, SE, NE, NW and the centre; AvaFrameSandboxNode fits its map placement to the corners.
        const double xs[5] = { x0, x1, x1, x0, (x0 + x1) / 2 };
        const double ys[5] = { y0, y0, y1, y1, (y0 + y1) / 2 };
        QJsonArray corners;
        double centre_lat = 0.0, centre_lon = 0.0;
        for (int i = 0; i < 5; i++) {
            double lat = 0.0, lon = 0.0;
            austria_lambert_to_wgs84(xs[i], ys[i], lat, lon);
            if (i < 4)
                corners.append(QJsonArray { round_to(lat, 6), round_to(lon, 6) });
            centre_lat = lat;
            centre_lon = lon;
        }
        const QJsonArray centre { round_to(centre_lat, 6), round_to(centre_lon, 6) };
        meta["location"] = QJsonObject { { "centre_lat_lon", centre }, { "corners_lat_lon", corners },
            { "transformation", "Inverse of Austria Lambert (EPSG:31287) + MGI to WGS 84 (8), as pyproj's best transformation" },
            { "accuracy_m", 0.5 } };

        // Release scenarios: one shapefile each; every feature has its own thickness.
        QJsonArray scenarios;
        QJsonArray scenario_names;
        for (const char* scenario : s.scenarios) {
            const QByteArray* shp = file(inputs + "/REL/" + scenario + ".shp");
            const QByteArray* dbf = shp ? file(inputs + "/REL/" + scenario + ".dbf") : nullptr;
            if (dbf == nullptr)
                return {};
            const auto shapes = read_shp(*shp, QString(scenario) + ".shp", error);
            if (!shapes)
                return {};
            const auto records = read_dbf(*dbf);
            std::vector<double> thickness(dem.cells(), 0.0);
            QJsonArray features;
            for (size_t f = 0; f < shapes->size() && f < records.size(); f++) {
                const auto& rec = records[f];
                const auto mask = rasterise(dem, (*shapes)[f]);
                const QString text = rec.contains("thickness") ? rec.at("thickness") : QString();
                const double th = text.isEmpty() ? 1.0 : text.toDouble(); // the script: float(rec.get("thickness") or 1.0)
                size_t cells = 0;
                for (size_t i = 0; i < mask.size(); i++) {
                    if (mask[i]) {
                        thickness[i] = th;
                        cells++;
                    }
                }
                QJsonValue name = text_or_null(rec, "Name");
                if (name.toString().isEmpty())
                    name = text_or_null(rec, "name");
                features.append(QJsonObject { { "name", name }, { "thickness_m", th }, { "cells", qint64(cells) },
                    { "area_m2", round_to(double(cells) * cell_area, 1) }, { "avalanche", text_or_null(rec, "Name_EZG") },
                    { "basis", text_or_null(rec, "Comment") }, { "thickness_from", text.isEmpty() ? "default 1.0 m" : "shapefile" } });
            }
            for (size_t i = 0; i < thickness.size(); i++) {
                if (std::isnan(dem.data[i]))
                    thickness[i] = NaN;
            }
            const QString release_file = QString("release_%1.asc").arg(scenario);
            if (!write_asc(out.filePath(release_file), dem, thickness, 3)) {
                error = "cannot write " + out.filePath(release_file);
                return {};
            }
            // AvaFrame's release thickness is slope-normal and its area on the inclined surface:
            // volume = sum(thickness / cos(slope)) * cell area (see the script).
            double volume = 0.0, projected = 0.0, slope_sum = 0.0;
            size_t released = 0;
            for (int row = 0; row < dem.nrows; row++) {
                for (int col = 0; col < dem.ncols; col++) {
                    const double th = thickness[size_t(row) * size_t(dem.ncols) + size_t(col)];
                    if (std::isnan(th))
                        continue;
                    projected += th;
                    if (!(th > 0.0))
                        continue;
                    const double c = slope_cos(dem, row, col);
                    volume += th / c;
                    slope_sum += std::acos(c) / DEG;
                    released++;
                }
            }
            scenarios.append(QJsonObject { { "name", scenario }, { "file", release_file }, { "features", features },
                { "thickness_convention", "perpendicular to the slope (vertical depth = thickness / cos(slope))" },
                { "volume_m3", round_to(volume * cell_area, 1) }, { "volume_projected_m3", round_to(projected * cell_area, 1) },
                { "mean_slope_deg", released ? QJsonValue(round_to(slope_sum / double(released), 1)) : QJsonValue(QJsonValue::Null) } });
            scenario_names.append(scenario);
        }
        meta["release_scenarios"] = scenarios;

        // References: com1DFA null-test benchmarks, matched to their release scenario.
        QJsonArray references;
        QJsonArray reference_names;
        for (const Benchmark& b : s.benchmarks) {
            const QString dir = QString("benchmarks/%1/").arg(b.name);
            const QByteArray* des = file(dir + b.name + "_desDict.json");
            if (des == nullptr)
                return {};
            const QJsonObject info = QJsonDocument::fromJson(*des).object();
            const QJsonObject params = info["Simulation Parameters"].toObject();
            if (info["AVANAME"].toString() != s.name || info["simType"].toString() != "null") {
                error = QString("%1: %2 is not a null test of this dataset").arg(s.name, b.name);
                return {};
            }
            if (!out.mkpath(QString("reference/") + b.name)) {
                error = "cannot create " + out.filePath(QString("reference/") + b.name);
                return {};
            }
            QJsonObject entry { { "benchmark", b.name }, { "release_scenario", params["Release Area Scenario"] },
                { "friction_model", params["Friction model"] }, { "density", params["Density [kgm-3]"] },
                { "program_version", params["Program version"] }, { "run_time_s", params["Avalanche run time [s]"] } };
            QJsonObject entry_files;
            for (const char* kind : { "pft", "pfv", "ppr" }) {
                const QByteArray* bytes = file(dir + b.stem + "_" + kind + ".asc");
                if (bytes == nullptr)
                    return {};
                const auto grid = read_asc(*bytes, QString(b.stem) + "_" + kind + ".asc", error);
                if (!grid)
                    return {};
                // com1DFA remeshes the DEM to its own cell size, so its results are usually on a
                // slightly different grid than the DEM; they are kept on theirs.
                entry["grid"] = grid->same_grid(dem) ? QJsonValue("same as the DEM")
                                                     : QJsonValue(QJsonObject { { "ncols", grid->ncols }, { "nrows", grid->nrows },
                                                           { "cellsize", grid->cellsize }, { "xllcorner", grid->xll }, { "yllcorner", grid->yll } });
                const QString rel = QString("reference/%1/%2.asc").arg(b.name, kind);
                if (!write_asc(out.filePath(rel), *grid, grid->data, 4)) {
                    error = "cannot write " + out.filePath(rel);
                    return {};
                }
                entry_files[kind] = rel;
                double max = std::numeric_limits<double>::lowest();
                size_t positive = 0;
                for (const double v : grid->data) {
                    if (std::isnan(v))
                        continue;
                    max = std::max(max, v);
                    positive += v > 0.0 ? 1 : 0;
                }
                entry[QString("max_") + kind] = round_to(max, 3);
                if (QByteArrayView(kind) == "pft") {
                    entry["footprint_cells"] = qint64(positive);
                    entry["footprint_area_m2"] = round_to(double(positive) * cell_area, 1);
                }
            }
            entry["files"] = entry_files;
            QFile params_file(out.filePath(QString("reference/%1/params.json").arg(b.name)));
            if (!params_file.open(QIODevice::WriteOnly) || params_file.write(QJsonDocument(params).toJson()) < 0) {
                error = "cannot write " + params_file.fileName();
                return {};
            }
            references.append(entry);
            reference_names.append(b.name);
        }
        meta["references"] = references;

        // AIMEC path and split point.
        QJsonObject path_meta;
        for (const auto& [key, rel] : { std::pair { "path", "/LINES/path_aimec.shp" }, std::pair { "split_point", "/POINTS/splitPoint.shp" } }) {
            const QByteArray* bytes = file(inputs + rel);
            if (bytes == nullptr)
                return {};
            const auto shapes = read_shp(*bytes, rel, error);
            if (!shapes)
                return {};
            if (shapes->empty() || shapes->front().empty())
                continue;
            QJsonArray xy, lat_lon;
            for (const Point& p : shapes->front().front()) {
                double lat = 0.0, lon = 0.0;
                austria_lambert_to_wgs84(p.x, p.y, lat, lon);
                xy.append(QJsonArray { round_to(p.x, 3), round_to(p.y, 3) });
                lat_lon.append(QJsonArray { round_to(lat, 7), round_to(lon, 7) });
            }
            path_meta[key] = QJsonObject { { "xy", xy }, { "lat_lon", lat_lon } };
        }
        if (!path_meta.isEmpty()) {
            QFile path_file(out.filePath("path.json"));
            if (!path_file.open(QIODevice::WriteOnly) || path_file.write(QJsonDocument(path_meta).toJson()) < 0) {
                error = "cannot write " + path_file.fileName();
                return {};
            }
            meta["path"] = "path.json";
        }

        QFile meta_file(out.filePath("dataset.json"));
        if (!meta_file.open(QIODevice::WriteOnly) || meta_file.write(QJsonDocument(meta).toJson()) < 0) {
            error = "cannot write " + meta_file.fileName();
            return {};
        }
        error.clear();
        return QJsonObject { { "name", s.name }, { "location", centre }, { "release_scenarios", scenario_names }, { "references", reference_names } };
    }

    bool update_index(const QString& out_root, const std::vector<QJsonObject>& entries, QString& error)
    {
        QFile file(QDir(out_root).filePath("index.json"));
        QJsonObject index;
        if (file.open(QIODevice::ReadOnly)) {
            index = QJsonDocument::fromJson(file.readAll()).object();
            file.close();
        }
        QJsonArray datasets = index["datasets"].toArray();
        for (const QJsonObject& entry : entries) {
            qsizetype i = 0;
            while (i < datasets.size() && datasets[i].toObject()["name"] != entry["name"])
                i++;
            if (i < datasets.size())
                datasets[i] = entry;
            else
                datasets.append(entry);
        }
        index["datasets"] = datasets;
        index["avaframe"] = QJsonObject { { "tag", AVAFRAME_TAG }, { "commit", AVAFRAME_COMMIT } };
        if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(index).toJson()) < 0) {
            error = "cannot write " + file.fileName();
            return false;
        }
        return true;
    }

} // namespace avaframe_data

// -------------------------------------------------------------------------------------------------
// Download
// -------------------------------------------------------------------------------------------------

namespace {
    constexpr int PARALLEL_REQUESTS = 4;
    constexpr int TRANSFER_TIMEOUT_MS = 60000; // without any data
}

AvaFrameDownload::AvaFrameDownload(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

AvaFrameDownload::~AvaFrameDownload()
{
    for (QNetworkReply* reply : m_in_flight) {
        reply->disconnect(this);
        reply->abort();
    }
}

void AvaFrameDownload::start(const QString& out_root)
{
    if (running())
        return;
    m_generation++;
    m_out_root = out_root;
    m_files.clear();
    m_entries.clear();
    m_queue.clear();
    m_bytes = 0;
    m_dataset = 0;
    for (const auto& source : avaframe_data::real_datasets()) {
        const auto files = avaframe_data::files_of(source);
        m_queue.insert(m_queue.end(), files.begin(), files.end());
    }
    std::reverse(m_queue.begin(), m_queue.end()); // popped from the back
    m_file_count = m_queue.size();
    m_state = State::Downloading;
    m_status = "Downloading from github.com/OpenNHM/AvaFrame ...";
    for (int i = 0; i < PARALLEL_REQUESTS; i++)
        request_next();
}

void AvaFrameDownload::cancel()
{
    if (!running())
        return;
    for (QNetworkReply* reply : m_in_flight) {
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    m_in_flight.clear();
    m_queue.clear();
    // A dataset being prepared on the worker thread still finishes; its result is dropped.
    m_state = State::Cancelled;
    m_status = "Cancelled.";
    emit finished(false);
}

float AvaFrameDownload::progress() const
{
    if (m_file_count == 0)
        return 0.0f;
    // Downloading is most of the time; preparing the five datasets the rest.
    const float downloaded = float(m_files.size()) / float(m_file_count);
    const float prepared = float(m_dataset) / float(avaframe_data::real_datasets().size());
    return m_state == State::Downloading ? 0.9f * downloaded : (m_state == State::Done ? 1.0f : 0.9f + 0.1f * prepared);
}

void AvaFrameDownload::request_next()
{
    if (m_state != State::Downloading || m_queue.empty())
        return;
    const QString path = m_queue.back();
    m_queue.pop_back();
    QNetworkRequest request(QUrl(avaframe_data::source_url() + path));
    request.setTransferTimeout(TRANSFER_TIMEOUT_MS);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_network->get(request);
    reply->setProperty("avaframe_path", path);
    m_in_flight.push_back(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { on_reply(reply); });
}

void AvaFrameDownload::on_reply(QNetworkReply* reply)
{
    reply->deleteLater();
    std::erase(m_in_flight, reply);
    if (m_state != State::Downloading)
        return;
    const QString path = reply->property("avaframe_path").toString();
    const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || (http != 0 && http != 200)) {
        const QString reason = http != 0 ? QString("HTTP %1").arg(http) : reply->errorString();
        fail(QString("Could not download %1 (%2). AvaFrame may have moved or removed its test data, or the network is down.").arg(path, reason));
        return;
    }
    const QByteArray data = reply->readAll();
    m_bytes += data.size();
    m_files[path] = data;
    m_status = QString("Downloaded %1 of %2 files (%3 MB) from github.com/OpenNHM/AvaFrame ...")
                   .arg(m_files.size())
                   .arg(m_file_count)
                   .arg(double(m_bytes) / 1e6, 0, 'f', 1);
    if (m_files.size() == m_file_count) {
        m_state = State::Preparing;
        prepare_next();
        return;
    }
    request_next();
}

void AvaFrameDownload::prepare_next()
{
    const auto& datasets = avaframe_data::real_datasets();
    if (m_state != State::Preparing)
        return;
    if (m_dataset >= datasets.size()) {
        QString error;
        if (!avaframe_data::update_index(m_out_root, m_entries, error)) {
            fail(error);
            return;
        }
        m_files.clear();
        m_state = State::Done;
        m_status = QString("Prepared %1 datasets in %2.").arg(m_entries.size()).arg(QDir::toNativeSeparators(m_out_root));
        emit finished(true);
        return;
    }
    const auto& source = datasets[m_dataset];
    m_status = QString("Preparing %1 (%2 of %3) ...").arg(source.name).arg(m_dataset + 1).arg(datasets.size());
    // Parsing and writing a few MB of text takes a moment: off the render thread where there
    // are threads. The worker gets its own copy of the inputs.
    std::map<QString, QByteArray> files;
    for (const QString& path : avaframe_data::files_of(source))
        files[path] = m_files[path];
    const QString out_root = m_out_root;
    // The result goes through the application object, which outlives this one; the pointer is
    // only checked back on the main thread.
    const auto work = [source, files = std::move(files), out_root, self = QPointer<AvaFrameDownload>(this), generation = m_generation]() {
        QString error;
        const QJsonObject entry = avaframe_data::prepare(source, files, out_root, error);
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, generation, entry, error]() {
                if (self && self->m_generation == generation)
                    self->on_prepared(entry, error);
            },
            Qt::QueuedConnection);
    };
#ifdef __EMSCRIPTEN__
    // No threads in the web build; queued, so the panel draws the status first.
    QMetaObject::invokeMethod(this, work, Qt::QueuedConnection);
#else
    QThreadPool::globalInstance()->start(work);
#endif
}

void AvaFrameDownload::on_prepared(const QJsonObject& entry, const QString& error)
{
    if (m_state != State::Preparing)
        return;
    if (entry.isEmpty()) {
        fail("Could not prepare " + QString(avaframe_data::real_datasets()[m_dataset].name) + ": " + error);
        return;
    }
    m_entries.push_back(entry);
    m_dataset++;
    prepare_next();
}

void AvaFrameDownload::fail(const QString& message)
{
    for (QNetworkReply* reply : m_in_flight) {
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    m_in_flight.clear();
    m_queue.clear();
    m_files.clear();
    m_state = State::Failed;
    m_status = message;
    qWarning().noquote() << "AvaFrame download:" << message;
    emit finished(false);
}

} // namespace webgpu_app
