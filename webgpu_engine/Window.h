#pragma once

#include <QVector3D>
#include <QMap>
#include <glm/glm.hpp>
#include "nucleus/AbstractRenderWindow.h"
#include "nucleus/camera/AbstractDepthTester.h"
#include "nucleus/camera/Controller.h"
#include "nucleus/utils/Stopwatch.h"
#include "webgpu.hpp"
#include "ShaderModuleManager.h"
#include "PipelineManager.h"
#include "UniformBufferObjects.h"

class QOpenGLFramebufferObject;

namespace webgpu_engine {

class Window : public nucleus::AbstractRenderWindow, public nucleus::camera::AbstractDepthTester {
    Q_OBJECT
public:
    using ObtainWebGpuSurfaceFunc = std::function<wgpu::Surface(wgpu::Instance instance)>;
    using ImGuiWindowImplInitFunc = std::function<void()>;
    using ImGuiWindowImplNewFrameFunc = std::function<void()>;
    using ImGuiWindowImplShutdownFunc = std::function<void()>;

#ifdef ALP_WEBGPU_APP_ENABLE_IMGUI
    Window(ObtainWebGpuSurfaceFunc obtainWebGpuSurfaceFunc, ImGuiWindowImplInitFunc imguiWindowInitFunc,
        ImGuiWindowImplNewFrameFunc imguiWindowNewFrameFunc, ImGuiWindowImplShutdownFunc imguiWindowShutdownFunc);
#else
    Window(ObtainWebGpuSurfaceFunc obtainWebGpuSurfaceFunc);
#endif

    ~Window() override;

    void initialise_gpu() override;
    void resize_framebuffer(int w, int h) override;
    void paint(QOpenGLFramebufferObject* framebuffer = nullptr) override;

    [[nodiscard]] float depth(const glm::dvec2& normalised_device_coordinates) override;
    [[nodiscard]] glm::dvec3 position(const glm::dvec2& normalised_device_coordinates) override;
    void deinit_gpu() override;
    void set_aabb_decorator(const nucleus::tile_scheduler::utils::AabbDecoratorPtr&) override;
    void remove_tile(const tile::Id&) override;
    [[nodiscard]] nucleus::camera::AbstractDepthTester* depth_tester() override;
    void set_permissible_screen_space_error(float new_error) override;

public slots:
    void update_camera(const nucleus::camera::Definition& new_definition) override;
    void update_debug_scheduler_stats(const QString& stats) override;
    void update_gpu_quads(const std::vector<nucleus::tile_scheduler::tile_types::GpuTileQuad>& new_quads, const std::vector<tile::Id>& deleted_quads) override;

signals:
    void gpu_ready_changed(bool ready);

private:
    void create_instance();
    void init_surface();
    void request_adapter();
    void request_device();
    void init_queue();
    void create_buffers();
    void create_bind_group_info();
    
    void create_swapchain(uint32_t w, uint32_t h);
    
    bool init_gui();
    void terminate_gui();
    void update_gui(wgpu::RenderPassEncoder render_pass);

private:
    wgpu::Instance m_instance = nullptr;
    wgpu::Device m_device = nullptr;
    wgpu::Adapter m_adapter = nullptr;
    wgpu::Surface m_surface = nullptr;
    wgpu::SwapChain m_swapchain = nullptr;
    wgpu::TextureFormat m_swapchain_format = wgpu::TextureFormat::Undefined;
    wgpu::Queue m_queue = nullptr;

    std::unique_ptr<ShaderModuleManager> m_shader_manager;
    std::unique_ptr<PipelineManager> m_pipeline_manager;

    std::unique_ptr<UniformBuffer<uboSharedConfig>> m_shared_config_ubo;
    std::unique_ptr<UniformBuffer<uboCameraConfig>> m_camera_config_ubo;

    std::unique_ptr<BindGroupInfo> m_bind_group_info;

    ObtainWebGpuSurfaceFunc m_obtain_webgpu_surface_func;
    ImGuiWindowImplInitFunc m_imgui_window_init_func;
    ImGuiWindowImplNewFrameFunc m_imgui_window_new_frame_func;
    ImGuiWindowImplShutdownFunc m_imgui_window_shutdown_func;

    nucleus::utils::Stopwatch m_stopwatch;
};

}
