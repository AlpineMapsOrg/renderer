#define ENABLE_SHADOW_MAIN
#ifdef ENABLE_SHADOW_MAIN


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

#include <nucleus/camera/ShadowGenerationController.h>

// This example demonstrates easy, cross-platform usage of OpenGL ES 3.0 functions via
// QOpenGLExtraFunctions in an application that works identically on desktop platforms
// with OpenGL 3.3 and mobile/embedded devices with OpenGL ES 3.0.

// The code is always the same, with the exception of two places: (1) the OpenGL context
// creation has to have a sufficiently high version number for the features that are in
// use, and (2) the shader code's version directive is different.

void find_min_max_metric(camera::ShadowGenerationController& controller, GLTileManager& gpu_tile_manager, ShadowGeneration& generation_obj, GLuint radius, glm::ivec2& min, glm::ivec2& max, float& min_value, float& max_value);
void save_config(camera::ShadowGenerationController& controller, ShadowGeneration& generation_obj, GLuint radius, const glm::vec2& cam_orbit, int img, float value, const QDir& dir, const QString& folder, const QString& info);

int main(int argc, char* argv[])
{
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
    //estimate the sun position from the shadow of the network
    //combine different shadow maps from different sun positions
    //estimate sun position from landmark trees
    //https://www.sciencedirect.com/science/article/pii/S0950705121011035

    //git submodule
    //add_subdirectory(sherpa)
    //target_link_libraries(nucleus PUBLIC sherpa Qt::Core Qt::Gui Qt6::Network)

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

    //-65.6, 53.1
    glm::vec2 very_good(-65.6, 53.1);
    for(int i=0; i<cnt; i++) {
        /*
        GLuint ortho_texture_handle = 0;
        for(auto& tileset : gpu_tile_manager.tiles()) {
            if (tileset.tiles.front().first == controller.center_tile()) {
                ortho_texture_handle = tileset.ortho_texture->textureId();
            }
        }
        qDebug() << "img_" << i << " - metric = [" << generation_obj.calculate_shadow_metric(controller.tile_cam(), controller.sun_cam(very_good, glm::vec2(radius * 2 + 1), 800), ortho_texture_handle) << "]";
         */
        //save_config(controller, generation_obj, radius, very_good, i, 0, dir, "very_good", "");
        if (i == 2)  {
            glm::ivec2 max, min;
            float max_value, min_value;

            find_min_max_metric(controller, gpu_tile_manager, generation_obj, radius, min, max, min_value, max_value);

            save_config(controller, generation_obj, radius, min, i, min_value, dir, "minmax\\boundary", "min");
            save_config(controller, generation_obj, radius, max, i, max_value, dir, "minmax\\boundary", "max");
            break;
        }

   //     }
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

    generation_obj.render_sunview_shadow_ortho_tripple(controller.tile_cam(), controller.sun_cam(cam_orbit, glm::vec2(radius * 2 + 1), 800), controller.ortho_texture(), true)
                .save(total_path, "PNG");

}

void find_min_max_metric(camera::ShadowGenerationController& controller, GLTileManager& gpu_tile_manager, ShadowGeneration& generation_obj, GLuint radius, glm::ivec2& min, glm::ivec2& max, float& min_value, float& max_value) {
    max_value = -1000000.f;
    min_value = 1000000.f;

    for (int up = 30; up <= 80; up += 25) {
        for(int rot = -80; rot <= -40; rot += 20) {
            glm::vec2 sun_orbit(rot, up);

            camera::Definition sun = controller.sun_cam(sun_orbit, glm::vec2(radius * 2 + 1), 800);

            GLuint ortho_texture_handle = 0;
            for(auto& tileset : gpu_tile_manager.tiles()) {
                if (tileset.tiles.front().first == controller.center_tile()) {
                    ortho_texture_handle = tileset.ortho_texture->textureId();
                }
            }
            if (ortho_texture_handle != 0) {
                float metric = generation_obj.calculate_shadow_metric(controller.tile_cam(), sun, ortho_texture_handle);
                //qDebug() << "(" << rot << "/" << up << ") = " << generation_obj.calculate_shadow_metric(controller.tile_cam(), sun, ortho_texture_handle);

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
