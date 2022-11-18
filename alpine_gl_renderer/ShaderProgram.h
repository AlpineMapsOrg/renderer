#pragma once

//#include <vector>
//#include <filesystem>
#include <unordered_map>
#include <string>
#include <memory>

#include <glm/glm.hpp>
#include <QOpenGLShaderProgram>

class ShaderProgram
{
    std::unordered_map<std::string, int> m_cached_uniforms;
    std::unordered_map<std::string, unsigned> m_cached_attribs;
    std::unique_ptr<QOpenGLShaderProgram> m_q_shader_program;

public:
    ShaderProgram(const std::string& vetex_shader_source, const std::string& fragment_shader_source);

    unsigned attribute_location(const std::string& name);
    void bind();
    void release();

    void set_uniform(const std::string& name, const glm::mat4& matrix);
    void set_uniform(const std::string& name, const glm::vec2& vector);
    void set_uniform(const std::string& name, const glm::vec3& vector);
    void set_uniform(const std::string& name, const glm::vec4& vector);
    void set_uniform(const std::string& name, int value);
    void set_uniform(const std::string& name, unsigned value);
    void set_uniform(const std::string& name, float value);

    void set_uniform_array(const std::string& name, const std::vector<glm::vec4>& array);
private:
    template <typename T>
    void set_uniform_template(const std::string& name, T value);

};

