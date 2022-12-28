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

#include "alpine_gl_renderer/GLTileManager.h"
#include "nucleus/camera/Controller.h"
#include "nucleus/TileLoadService.h"
#include "nucleus/tile_scheduler/SingleTileLoader.h"
#include "ShaderProgram.h"
#include "qnetworkreply.h"

#include <nucleus/camera/ShadowGenerationController.h>

// This example demonstrates easy, cross-platform usage of OpenGL ES 3.0 functions via
// QOpenGLExtraFunctions in an application that works identically on desktop platforms
// with OpenGL 3.3 and mobile/embedded devices with OpenGL ES 3.0.

// The code is always the same, with the exception of two places: (1) the OpenGL context
// creation has to have a sufficiently high version number for the features that are in
// use, and (2) the shader code's version directive is different.

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

    GLuint radius = 5;
    QOpenGLFramebufferObject img_fb(QSize(256, 256), QOpenGLFramebufferObject::Attachment::Depth);

    glm::vec2 shadow_range(radius * 2 + 1, radius * 2 + 1); //the shadow map will be a rectangle centered at the current tile, with width shadow_range.x tiles and height shadow_range.y tiles
    QOpenGLFramebufferObjectFormat format;
    format.setInternalTextureFormat(GL_RGBA32F);
    format.setAttachment(QOpenGLFramebufferObject::Attachment::Depth);
    QOpenGLFramebufferObject shadow_fb(QSize(static_cast<int>(256 * shadow_range.x), static_cast<int>(256 * shadow_range.y)), format);

    GLTileManager gpu_tile_manager;
    TileLoadService terrain_service("http://alpinemaps.cg.tuwien.ac.at/tiles/alpine_png/", TileLoadService::UrlPattern::ZXY, ".png");
    TileLoadService ortho_service("http://alpinemaps.cg.tuwien.ac.at/tiles/ortho/", TileLoadService::UrlPattern::ZYX_yPointingSouth, ".jpeg");
    SingleTileLoader scheduler(radius);

    //Hochschwab
    //camera::Controller camera_controller { { { 1689990.0, 6045529.0, 2074.4 + 100 }, { 1689990.0, 6045529.0, 2074.4 } } };
    //eps:4326 = 47.6300588, 15.1814383
    //http://alpinemaps.cg.tuwien.ac.at/#16/47.6300/15.1814
    //http://alpinemaps.cg.tuwien.ac.at/#16/47.63/15.1814/-170/70
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

    float bg[] = {0.1f, 0.1f, 0.1f, 1.f};
    float one[] = {1.f, 1.f, 1.f, 1.f};

    glm::mat4 biasMatrix(
        0.5, 0.0, 0.0, 0.0,
        0.0, 0.5, 0.0, 0.0,
        0.0, 0.0, 0.5, 0.0,
        0.5, 0.5, 0.5, 1.0);

    int cnt = 21;
    QDir dir("C:\\Users\\shalper\\Documents\\Shadow Detection\\Renderer\\mattes\\");

    context.functions()->glEnable(GL_DEPTH_TEST);
    context.functions()->glDepthFunc(GL_LEQUAL);

    context.functions()->glEnable(GL_CULL_FACE);
    context.functions()->glFrontFace(GL_BACK);

    for(int i=0; i<cnt; i++) {
     //   for(int rot = 0; rot <= 360; rot += 45) {
            int rot = -68;
            //-------------------------- sun pass --------------------------
            glm::vec2 sun_orbit(rot, 55.f);

            camera::Definition sun = controller.sun_cam(sun_orbit, shadow_range, 800);

            context.functions()->glViewport(0, 0, shadow_fb.size().width(), shadow_fb.size().height());

            shadow_fb.bind();
            context.extraFunctions()->glClearBufferfv(GL_COLOR, 0, one);
            context.extraFunctions()->glClearBufferfv(GL_DEPTH, 0, one);
            sun_program->bind();
            gpu_tile_manager.draw(shader.get(), sun);

            QImage shadow_map_img = shadow_fb.toImage(false);
            QOpenGLTexture shadow_map(shadow_map_img);

            shadow_map.setMinMagFilters(QOpenGLTexture::Filter::NearestMipMapNearest, QOpenGLTexture::Filter::Nearest);


            GLuint shadow_map_handle = shadow_fb.texture();

            qDebug() << shadow_map_handle;

            context.functions()->glBindTexture(GL_TEXTURE_2D, shadow_map_handle);
            context.functions()->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            context.functions()->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            context.functions()->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            context.functions()->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            context.functions()->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            context.functions()->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16);

            context.functions()->glActiveTexture(GL_TEXTURE1);
            context.functions()->glBindTexture(GL_TEXTURE_2D, shadow_map_handle);
            context.functions()->glActiveTexture(GL_TEXTURE0);


            //-------------------------- sun pass --------------------------
            //-------------------------- tile pass --------------------------

            context.functions()->glViewport(0, 0, img_fb.size().width(), img_fb.size().height());
            img_fb.bind();

            context.extraFunctions()->glClearBufferfv(GL_COLOR, 0, bg);
            context.extraFunctions()->glClearBufferfv(GL_DEPTH, 0, one);

            shader->bind();
            shader->set_uniform("out_mode", 0);
            shader->set_uniform("sun_position", glm::vec3(sun.position()));
            shader->set_uniform("shadow_matrix", biasMatrix * sun.localViewProjectionMatrix(sun.position()));

            gpu_tile_manager.draw(shader.get(), controller.tile_cam());

            QImage shadow_detected_image = img_fb.toImage();

            context.extraFunctions()->glClearBufferfv(GL_COLOR, 0, bg);
            context.extraFunctions()->glClearBufferfv(GL_DEPTH, 0, one);
            shader->set_uniform("out_mode", 1);
            gpu_tile_manager.draw(shader.get(), sun);

            QImage sun_pov = img_fb.toImage();

            QString orbit_string = QString("%1_%2_x")
                    .arg(sun_orbit.x)
                    .arg(sun_orbit.y);

            QString img_string = QString("img_%1")
                    .arg(i);

            QString sub_dir_name = orbit_string;
            QString filename = img_string;

            dir.mkdir(sub_dir_name);

            QPixmap pixmap(3 * 256, 256);
            QPainter painter(&pixmap);

            painter.drawImage(0, 0, sun_pov);
            painter.drawImage(256, 0, shadow_detected_image);
            painter.drawImage(512, 0, controller.ortho_texture());

            pixmap.toImage().save(dir.absolutePath() + "\\" + sub_dir_name + "\\" + filename + ".png", "PNG");

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
#endif
