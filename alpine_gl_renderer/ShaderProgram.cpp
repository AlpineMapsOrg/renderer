#include "ShaderProgram.h"

#include <QFile>
#include <QOpenGLContext>

#include "GLHelpers.h"

namespace {

QByteArray create_shader_code(const ShaderProgram::Files& files)
{
    QByteArray code;
    for (const QString& url : files) {
        QFile f(url);
        f.open(QIODeviceBase::ReadOnly);
        code.append(f.readAll());
    }
    return code;
}

QByteArray versionedShaderCode(const QByteArray& src)
{
    QByteArray versionedSrc;

    if (QOpenGLContext::currentContext()->isOpenGLES())
        versionedSrc.append(QByteArrayLiteral("#version 300 es\n"));
    else
        versionedSrc.append(QByteArrayLiteral("#version 330\n"));

    versionedSrc.append(src);
    return versionedSrc;
}
}

ShaderProgram::ShaderProgram(const std::string& vetex_shader_source, const std::string& fragment_shader_source)
{
    auto program = std::make_unique<QOpenGLShaderProgram>();
    program->addShaderFromSourceCode(QOpenGLShader::Vertex, versionedShaderCode(vetex_shader_source.c_str()));
    program->addShaderFromSourceCode(QOpenGLShader::Fragment, versionedShaderCode(fragment_shader_source.c_str()));
    {
        const auto link_success = program->link();
        assert(link_success);
    }
    m_q_shader_program = std::move(program);
}

ShaderProgram::ShaderProgram(Files vertex_shader_parts, Files fragment_shader_parts)
    : m_vertex_shader_parts(std::move(vertex_shader_parts))
    , m_fragment_shader_parts(std::move(fragment_shader_parts))
{
    reload();
    assert(m_q_shader_program);
}

unsigned ShaderProgram::attribute_location(const std::string& name)
{
    if (!m_cached_attribs.contains(name))
        m_cached_attribs[name] = m_q_shader_program->attributeLocation(name.c_str());

    return m_cached_attribs.at(name);
}

void ShaderProgram::bind()
{
    m_q_shader_program->bind();
}

void ShaderProgram::release()
{
    m_q_shader_program->release();
}

void ShaderProgram::set_uniform(const std::string& name, const glm::mat4& matrix)
{
    set_uniform_template(name, matrix);
}

void ShaderProgram::set_uniform(const std::string& name, const glm::vec2& vector)
{
    set_uniform_template(name, vector);
}

void ShaderProgram::set_uniform(const std::string& name, const glm::vec3& vector)
{
    set_uniform_template(name, vector);
}

void ShaderProgram::set_uniform(const std::string& name, const glm::vec4& vector)
{
    set_uniform_template(name, vector);
}

void ShaderProgram::set_uniform(const std::string& name, int value)
{
    set_uniform_template(name, value);
}

void ShaderProgram::set_uniform(const std::string& name, unsigned value)
{
    set_uniform_template(name, value);
}

void ShaderProgram::set_uniform(const std::string& name, float value)
{
    set_uniform_template(name, value);
}

void ShaderProgram::set_uniform_array(const std::string& name, const std::vector<glm::vec4>& array)
{
    if (!m_cached_uniforms.contains(name))
        m_cached_uniforms[name] = m_q_shader_program->uniformLocation(name.c_str());

    const auto uniform_location = m_cached_uniforms.at(name);

    m_q_shader_program->setUniformValueArray(uniform_location, reinterpret_cast<const float*>(array.data()), int(array.size()), 4);
}

void ShaderProgram::reload()
{
    if (m_vertex_shader_parts.empty() || m_fragment_shader_parts.empty())
        return;

    auto program = std::make_unique<QOpenGLShaderProgram>();
    program->addShaderFromSourceCode(QOpenGLShader::Vertex, versionedShaderCode(create_shader_code(m_vertex_shader_parts)));
    program->addShaderFromSourceCode(QOpenGLShader::Fragment, versionedShaderCode(create_shader_code(m_fragment_shader_parts)));
    if (program->link()) {
        m_q_shader_program = std::move(program);
    }
}

template<typename T>
void ShaderProgram::set_uniform_template(const std::string& name, T value)
{
    if (!m_cached_uniforms.contains(name))
        m_cached_uniforms[name] = m_q_shader_program->uniformLocation(name.c_str());

    const auto uniform_location = m_cached_uniforms.at(name);
    m_q_shader_program->setUniformValue(uniform_location, gl_helpers::toQtType(value));
}
