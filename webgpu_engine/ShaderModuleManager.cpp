#include "ShaderModuleManager.h"

#include <regex>
#include <QFile>

namespace webgpu_engine {

ShaderModuleManager::ShaderModuleManager(wgpu::Device& device, const std::filesystem::path& prefix):
    m_device(&device),
    m_prefix(prefix)
{}

void ShaderModuleManager::create_shader_modules()
{
    m_debug_triangle_shader_module = create_shader_module(get_contents("DebugTriangle.wgsl"));
    m_debug_config_and_camera_shader_module = create_shader_module(get_contents("DebugConfigAndCamera.wgsl"));
}

void ShaderModuleManager::release_shader_modules()
{
    m_debug_triangle_shader_module.release();
}

wgpu::ShaderModule ShaderModuleManager::debug_triangle() const
{
    return m_debug_triangle_shader_module;
}

wgpu::ShaderModule ShaderModuleManager::debug_config_and_camera() const {
    return m_debug_config_and_camera_shader_module;
}

std::string ShaderModuleManager::read_file_contents(const std::string& name) const {
    const auto path = m_prefix / name; // operator/ concats paths
    auto file = QFile(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "could not open shader file " << path << std::endl;
        throw std::runtime_error("could not open file " + path.string());
    }
    return file.readAll().toStdString();
}

std::string ShaderModuleManager::get_contents(const std::string& name) {
    const auto found_it = m_shader_name_to_code.find(name);
    if (found_it != m_shader_name_to_code.end()) {
        return found_it->second;
    }
    const auto file_contents = read_file_contents(name);
    const auto preprocessed_contents = preprocess(file_contents);
    m_shader_name_to_code[name] = preprocessed_contents;
    return preprocessed_contents;
}

wgpu::ShaderModule ShaderModuleManager::create_shader_module(const std::string& code) {
    wgpu::ShaderModuleDescriptor shaderModuleDesc{};
    wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
    wgslDesc.chain.next = nullptr;
    wgslDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    wgslDesc.code = code.data();
    shaderModuleDesc.nextInChain = &wgslDesc.chain;
    return m_device->createShaderModule(shaderModuleDesc);
}

std::string ShaderModuleManager::preprocess(const std::string& code)
{
    std::string preprocessed_code = code;
    const std::regex include_regex("#include \"([a-zA-Z0-9 ._-]+)\"");
    for (std::smatch include_match; std::regex_search(preprocessed_code, include_match, include_regex);) {
        const std::string included_filename = include_match[1].str(); // first submatch
        const std::string included_file_contents = get_contents(included_filename);
        preprocessed_code.replace(include_match.position(), include_match.length(), included_file_contents);
    }
    return preprocessed_code;
}

}
