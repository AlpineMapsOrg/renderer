#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <memory>

#include <glm/glm.hpp>
#include <QOpenGLShaderProgram>
#include <QUrl>
#include <QDebug>

namespace gl_engine {

enum class ShaderCodeSource {
    PLAINTEXT,
    FILE
};

class ShaderProgram {
private:
    std::unordered_map<std::string, int> m_cached_uniforms;
    std::unordered_map<std::string, int> m_cached_attribs;
    std::unique_ptr<QOpenGLShaderProgram> m_q_shader_program;
    QString m_vertex_shader;    // either filename or native shader code
    QString m_fragment_shader;  // either filename or native shader code
    ShaderCodeSource m_code_source;

public:
    //ShaderProgram() {};
    ShaderProgram(QString vertex_shader, QString fragment_shader, ShaderCodeSource code_source = ShaderCodeSource::FILE);

    int attribute_location(const std::string& name);
    void bind();
    void release();

    void set_uniform_block(const std::string& name, GLuint location);

    void set_uniform(const std::string& name, const glm::mat4& matrix);
    void set_uniform(const std::string& name, const glm::vec2& vector);
    void set_uniform(const std::string& name, const glm::vec3& vector);
    void set_uniform(const std::string& name, const glm::vec4& vector);
    void set_uniform(const std::string& name, int value);
    void set_uniform(const std::string& name, unsigned value);
    void set_uniform(const std::string& name, float value);

    void set_uniform_array(const std::string& name, const std::vector<glm::vec4>& array);
public slots:
    void reload();
private:
    template <typename T>
    void set_uniform_template(const std::string& name, T value);

};
}
