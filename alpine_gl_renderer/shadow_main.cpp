#define ENABLE_SHADOW_MAIN
#ifdef ENABLE_SHADOW_MAIN

#define ROTATIONS 91
#define UPS 71

#include "nucleus/tile_scheduler/utils.h"
#include "qnetworkaccessmanager.h"

#include <iostream>

#include <QGuiApplication>
#include <QObject>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QOpenGLFramebufferObject>
#include <QOpenGLExtraFunctions>
#include <QTimer>
#include <QDir>
#include <QPainter>
#include <QOpenGLDebugLogger>

#include "alpine_gl_renderer/GLTileManager.h"
#include "nucleus/camera/Controller.h"
#include "nucleus/TileLoadService.h"
#include "nucleus/tile_scheduler/SingleTileLoader.h"
#include "ShaderProgram.h"
#include "qnetworkreply.h"
#include "ShadowGeneration.h"
#include "HoughTransform.h"

//#include <SolTrack.h>
//#include "libs/soltrack/SolTrack.h"

#include <nucleus/camera/ShadowGenerationController.h>

// This example demonstrates easy, cross-platform usage of OpenGL ES 3.0 functions via
// QOpenGLExtraFunctions in an application that works identically on desktop platforms
// with OpenGL 3.3 and mobile/embedded devices with OpenGL ES 3.0.

// The code is always the same, with the exception of two places: (1) the OpenGL context
// creation has to have a sufficiently high version number for the features that are in
// use, and (2) the shader code's version directive is different.

void find_min_max_metric(camera::ShadowGenerationController& controller, GLTileManager& gpu_tile_manager, ShadowGeneration& generation_obj, GLuint radius, glm::ivec2& min, glm::ivec2& max, float& min_value, float& max_value, float heatmap_array[][ROTATIONS]);
void save_config(camera::ShadowGenerationController& controller, ShadowGeneration& generation_obj, GLuint radius, const glm::vec2& cam_orbit, int img, float value, const QDir& dir, const QString& folder, const QString& info);

int main(int argc, char* argv[])
{
    /*
     * ------------------- Code for the hough transform
    HoughTransform transform;

    std::map<int, QImage> shadow_maps;
    std::map<int,  Raster<uint16_t>> height_maps;

    QString base = "C:/Users/shalper/Documents/Shadow Detection/Renderer/mattes/hough/images/";
    for (int i=0; i<20; i++) {
        QString shadow_map_str = QString("%1detected_shadow_%2.png")
                .arg(base)
                .arg(i);
        QString height_file_str = QString("%1%2.txt")
                .arg(base)
                .arg(i);

        QImage shadow_map(shadow_map_str, "PNG");
        FILE* fp;

        fopen_s(&fp, height_file_str.toStdString().c_str(), "r");

        if (fp == NULL || shadow_map.isNull()) {
            qDebug() << "Error for img [" << i << "]";
            continue;
        }

        std::vector<uint16_t> height_map;

        uint16_t height;
        while(fscanf_s(fp, "%hd ", &height) == 1) {
            height_map.push_back(height);
        }

        fclose(fp);


        Raster<uint16_t> raster(std::move(height_map), 65);

        transform.add_sun_normals(shadow_map, raster, 2.3519);

        shadow_maps.emplace(i, shadow_map);
        height_maps.emplace(i, raster);


    }

    for (const auto& pair : shadow_maps) {
        int i = pair.first;
        const QImage& shadow_map = pair.second;

        const Raster<uint16_t>& raster = height_maps.at(i);

        QString heatmap_name = QString("%1v4/heatmap_%2.png")
                .arg(base)
                .arg(i);


        glm::vec2 help = transform.get_most_likely_sun_position(shadow_map, raster, heatmap_name, 2.3519); // one tile width is 152.874 m and there are 65 height samples, so each height sample is 2.3519 m apart

        qDebug() << help.x << "|" << help.y;
    }


    QString final_heatmap_name = QString("%1v4/heatmap_%2.png")
            .arg(base)
            .arg("final");

    glm::vec2 max = transform.generate_space_heatmap(final_heatmap_name);
    qDebug() << "max = " << max.x << "|" << max.y;


    exit(EXIT_SUCCESS);
    */

    //----------------- code for the shadow mapping

    QGuiApplication app(argc, argv);
    Q_UNUSED(app);

    QOffscreenSurface surface;

    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    fmt.setOption(QSurfaceFormat::DebugContext);

    fmt.setVersion(4, 4);
    fmt.setProfile(QSurfaceFormat::CoreProfile);

    QSurfaceFormat::setDefaultFormat(fmt);

    QOpenGLContext context;
    context.setFormat(fmt);
    surface.setFormat(fmt);
    context.create();
    surface.create();
    context.makeCurrent(&surface);

    QOpenGLDebugLogger logger;
    logger.initialize();

    GLuint radius = 5;

    GLTileManager gpu_tile_manager;
    TileLoadService terrain_service("http://alpinemaps.cg.tuwien.ac.at/tiles/alpine_png/", TileLoadService::UrlPattern::ZXY, ".png");
    TileLoadService ortho_service("http://alpinemaps.cg.tuwien.ac.at/tiles/ortho/", TileLoadService::UrlPattern::ZYX_yPointingSouth, ".jpeg");
    SingleTileLoader scheduler(radius);

    ShadowGeneration generation_obj(context, gpu_tile_manager, radius);

    //Hochschwab
    //camera::Controller camera_controller { { { 1689990.0, 6045529.0, 2074.4 + 100 }, { 1689990.0, 6045529.0, 2074.4 } } };
    //eps:4326 = 47.6300588, 15.1814383
    //http://alpinemaps.cg.tuwien.ac.at/#16/47.6300/15.1814
    //http://alpinemaps.cg.tuwien.ac.at/#16/47.63/15.1814/-170/70
    //https://www.suncalc.org/#/47.5849,14.9593,9/2023.08.31/15:18/1/3
    //https://www.sciencedirect.com/science/article/pii/S0950705121011035

    camera::ShadowGenerationController controller({ 1689990.0, 6045529.0}, 18);


    std::unique_ptr<ShaderProgram> shader = std::make_unique<ShaderProgram>(
        ShaderProgram::Files({"gl_shaders/shadow.vert"}),
        ShaderProgram::Files({"gl_shaders/shadow.frag"}),
                "#version 420 core");

    std::unique_ptr<ShaderProgram> sun_program = std::make_unique<ShaderProgram>(
                ShaderProgram::Files({"gl_shaders/sun.vert"}),
                ShaderProgram::Files({"gl_shaders/sun.frag"}),
                "#version 420 core");

   gpu_tile_manager.initiliseAttributeLocations(shader.get());

   QEventLoop wait_for_height_data;
    QNetworkAccessManager m_network_manager;
    QNetworkReply* reply = m_network_manager.get(QNetworkRequest(QUrl("http://gataki.cg.tuwien.ac.at/tiles/alpine_png2/height_data.atb")));
    QObject::connect(reply, &QNetworkReply::finished, [reply, &scheduler, &app]() {
        const auto url = reply->url();
        const auto error = reply->error();
        if (error == QNetworkReply::NoError) {
            const QByteArray data = reply->readAll();
            scheduler.set_aabb_decorator(tile_scheduler::AabbDecorator::make(TileHeights::deserialise(data)));
        } else {
            qDebug() << "Loading of " << url << " failed: " << error;
            app.exit(0);
            // do we need better error handling?
        }
        reply->deleteLater();
    });
    QObject::connect(reply, &QNetworkReply::finished, &wait_for_height_data, &QEventLoop::quit);
    wait_for_height_data.exec();

    QEventLoop wait_for_tiles;

    QObject::connect(&controller, &camera::ShadowGenerationController::centerChanged,
                     [&scheduler](const auto& tile) {scheduler.loadTiles(tile);});

    QObject::connect(&scheduler, &TileScheduler::tileRequested, &terrain_service, &TileLoadService::load);
    QObject::connect(&scheduler, &TileScheduler::tileRequested, &ortho_service, &TileLoadService::load);
    QObject::connect(&scheduler, &TileScheduler::tileExpired,
                     [&gpu_tile_manager](const auto& tile) { gpu_tile_manager.removeTile(tile); });

    QObject::connect(&ortho_service, &TileLoadService::loadReady, &scheduler, &TileScheduler::receiveOrthoTile);
    QObject::connect(&ortho_service, &TileLoadService::tileUnavailable, &scheduler, &TileScheduler::notifyAboutUnavailableOrthoTile);
    QObject::connect(&terrain_service, &TileLoadService::loadReady, &scheduler, &TileScheduler::receiveHeightTile);
    QObject::connect(&terrain_service, &TileLoadService::tileUnavailable, &scheduler, &TileScheduler::notifyAboutUnavailableHeightTile);

    QObject::connect(&scheduler, &TileScheduler::tileReady, [&gpu_tile_manager](const std::shared_ptr<Tile>& tile) { gpu_tile_manager.addTile(tile); });
    QObject::connect(&scheduler, &TileScheduler::tileReady, &controller, &camera::ShadowGenerationController::addTile);

    QObject::connect(&scheduler, &TileScheduler::allTilesLoaded, &wait_for_tiles, &QEventLoop::quit);

    scheduler.loadTiles(controller.center_tile());
    wait_for_tiles.exec();

    int cnt = 20;
    QDir dir("C:\\Users\\shalper\\Documents\\Shadow Detection\\Renderer\\mattes\\");

    context.functions()->glEnable(GL_DEPTH_TEST);
    context.functions()->glDepthFunc(GL_LEQUAL);

    //---------------------------NOTICE
    //the lines in the main from here onwards have been frequently changed by me and may or may not be relevant or make sense

    //-65.6, 53.1
    glm::vec2 very_good(-65.6, 53.1);

    float heatmap_array[70][ROTATIONS];

    for (int v=0; v<70; v++) {
        for (int k=0; k<ROTATIONS; k++) {
            heatmap_array[v][k] = 0;
        }
    }

    std::vector<int> cliffs = {0, 1, 2, 5, 6, 10, 11, 16, 17};

    for(int i=0; i<cnt; i++) {

        bool good_image = false;

        for (int cliff : cliffs) {
            if (i == cliff) {
                good_image = true;
                break;
            }
        }

        if (good_image) {
            glm::ivec2 max, min;
            float max_value, min_value;

            find_min_max_metric(controller, gpu_tile_manager, generation_obj, radius, min, max, min_value, max_value, heatmap_array);


            save_config(controller, generation_obj, radius, min, i, min_value, dir, "minmax\\boundary2", "min");
            save_config(controller, generation_obj, radius, max, i, max_value, dir, "minmax\\boundary2", "max");
        }

        if (i + 1 < cnt) {
            QEventLoop wait;
            QObject::connect(&scheduler, &TileScheduler::allTilesLoaded, &wait, &QEventLoop::quit);
            if ((i + 1) % 5) {
                controller.advance(glm::ivec2(1, 0));
            } else {
                controller.advance(glm::ivec2(-5, 1));
            }

            wait.exec();
        }
        //-------------------------- tile pass --------------------------
    }


    QImage heatmap(ROTATIONS, UPS, QImage::Format::Format_Grayscale8);
    glm::ivec2 max(0.f, 0.f);
    float max_v = -1000000.f;
    int n = 0;

    for (int x = 0; x < UPS; x++) {
        for (int y = 0; y < ROTATIONS; y++) {
            if (heatmap_array[x][y] > max_v) {
                max.x = x;
                max.y = y;

                max_v = heatmap_array[x][y];
                n = 1;
            } else if (heatmap_array[x][y] == max_v) {
                n++;
            }
        }
    }

    for (int x = 0; x < UPS; x++) {
        for (int y = 40; y < ROTATIONS; y++) {
            int value = max_v > 0 ? (heatmap_array[x][y] * 256) / max_v : 0;

            heatmap.setPixel(90 - y, x, qRgba(value, value, value, value));
        }
    }

    //heatmap.save("C:\\Users\\shalper\\Documents\\Shadow Detection\\Renderer\\mattes\\minmax\\brightness2\\heatmap.png", "PNG");
    //qDebug() << "max_value = " << max_v << " - n = " << n << " at [" << max.x  << "|" << max.y << "]";

    //let the os do the cleanup
    //(might be better to do it manually)
}

void save_config(camera::ShadowGenerationController& controller, ShadowGeneration& generation_obj, GLuint radius, const glm::vec2& cam_orbit, int img, float value, const QDir& dir, const QString& folder, const QString& info) {

    QString orbit_string = QString("%1_%2_%3_%4")
            .arg(info)
            .arg(cam_orbit.x)
            .arg(cam_orbit.y)
            .arg(value);

    QString img_string = QString("img_%1")
            .arg(img);

    QString filename = img_string + orbit_string;

    dir.mkdir(folder);

    QString total_path = dir.absolutePath() + "\\" + folder + "\\" + filename + ".png";

    qDebug() << "image name: " << total_path;

    generation_obj.render_sunview_shadow_ortho_tripple(controller.tile_cam(), controller.sun_cam(cam_orbit, glm::vec2(radius * 2 + 1), 800), controller.ortho_texture(), false)
                .save(total_path, "PNG");

}

void find_min_max_metric(camera::ShadowGenerationController& controller, GLTileManager& gpu_tile_manager, ShadowGeneration& generation_obj, GLuint radius, glm::ivec2& min, glm::ivec2& max, float& min_value, float& max_value, float heatmap_array[][ROTATIONS]) {
    max_value = -1000000.f;
    min_value = 1000000.f;

    for (int up = 40; up < 70; up += 1) {
        for(int rot = -(ROTATIONS - 1); rot <= 0; rot += 1) {
            glm::vec2 sun_orbit(rot, up);

            camera::Definition sun = controller.sun_cam(sun_orbit, glm::vec2(radius * 2 + 1), 800);

            GLuint ortho_texture_handle = 0;
            for(auto& tileset : gpu_tile_manager.tiles()) {
                if (tileset.tiles.front().first == controller.center_tile()) {
                    ortho_texture_handle = tileset.ortho_texture->textureId();
                    break;
                }
            }
            if (ortho_texture_handle != 0) {
                float metric = generation_obj.calculate_shadow_metric(controller.tile_cam(), sun, ortho_texture_handle);
                //qDebug() << "(" << rot << "/" << up << ") = " << generation_obj.calculate_shadow_metric(controller.tile_cam(), sun, ortho_texture_handle);

                heatmap_array[up][-rot] += metric;

                if (metric > max_value) {
                    max = glm::ivec2(rot, up);
                    max_value = metric;
                }
                if (metric < min_value) {
                    min = glm::ivec2(rot, up);
                    min_value = metric;
                }
            }
        }
    }

}

#endif
