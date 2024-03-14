#pragma once

#include <map>
#include <string>
#include <filesystem>
#include "webgpu.hpp"

namespace webgpu_engine {

class ShaderModuleManager {
public:
    const static inline std::filesystem::path DEFAULT_PREFIX = ":/wgsl_shaders/";

public:
    ShaderModuleManager(wgpu::Device& device, const std::filesystem::path& prefix = DEFAULT_PREFIX);
    ~ShaderModuleManager() = default;

    void create_shader_modules();
    void release_shader_modules();

    wgpu::ShaderModule debug_triangle() const;
    wgpu::ShaderModule debug_config_and_camera() const;

private:
    std::string read_file_contents(const std::string& name) const;
    std::string get_contents(const std::string& name);
    std::string preprocess(const std::string& code);
    wgpu::ShaderModule create_shader_module(const std::string& code);

private:
    wgpu::Device* m_device;
    std::filesystem::path m_prefix;

    std::map<std::string, std::string> m_shader_name_to_code;

    wgpu::ShaderModule m_debug_triangle_shader_module = nullptr;
    wgpu::ShaderModule m_debug_config_and_camera_shader_module = nullptr;
};

}
