#include "ShaderProgram.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QOpenGLContext>
#include <QOpenGLShader>
#include <QOpenGLExtraFunctions>

#include <QDebug>

#include "helpers.h"

using gl_engine::ShaderProgram;

namespace {



QString get_qrc_or_path_prefix() {
    QString prefix = ":/gl_shaders/";
    if (!QOpenGLContext::currentContext()->isOpenGLES())
        prefix = ALP_RESOURCES_PREFIX; // FOR NATIVE BUILD: prefix = "shaders/";
    return prefix;
}

QString load_shader_code(const QString file_name) {
    QString code;
    QString line;
    QString extended_file_name = file_name;
    extended_file_name.prepend(get_qrc_or_path_prefix());
    QFile file(extended_file_name);

    static QRegularExpression re(R"RX(#include "(?<file>[\w\d\W ]*?)")RX");
    re.setPatternOptions(QRegularExpression::CaseInsensitiveOption);

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        while (!stream.atEnd()) {
            line = stream.readLine();
            QString line_command = line.trimmed();

            // HANDLE INCLUDES
            bool line_handled = false;
            if (line_command.startsWith("#include", Qt::CaseInsensitive)) {
                QRegularExpressionMatch match = re.match(line);
                if (match.hasMatch()) {
                    QString includeFileName = match.captured("file");
                    //includeFileName.prepend(get_qrc_or_path_prefix());
                    code.append("//" + line + ": \n");
                    code.append(load_shader_code(includeFileName));
                    line_handled = true;
                }
            }

            if (!line_handled)
                code.append(line + "\n");
        }
    } else {
        qDebug() << "Error opening shader file @" << extended_file_name;
        assert(false);
    }
    return code;
}

QString getShaderCodeVersion() {
    if (QOpenGLContext::currentContext()->isOpenGLES())
        return "#version 300 es\n";
    else
        return "#version 330\n";
}

QByteArray versionedShaderCode(const QByteArray& src)
{
    QByteArray versionedSrc;
    versionedSrc.append(getShaderCodeVersion().toLocal8Bit());
    versionedSrc.append(src);
    return versionedSrc;
}

QByteArray versionedShaderCode(const QString& src) {
    QByteArray versionedSrc;
    versionedSrc.append(getShaderCodeVersion().toLocal8Bit());
    versionedSrc.append(src.toLocal8Bit());
    return versionedSrc;
}

}


ShaderProgram::ShaderProgram(QString vertex_shader, QString fragment_shader, ShaderCodeSource code_source)
    : m_code_source(code_source), m_vertex_shader(vertex_shader), m_fragment_shader(fragment_shader)
{
    reload();
    assert(m_q_shader_program);
}

int ShaderProgram::attribute_location(const std::string& name)
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

void ShaderProgram::set_uniform_block(const std::string& name, GLuint location)
{
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    auto pId = m_q_shader_program->programId();
    unsigned int ubi = f->glGetUniformBlockIndex(pId, name.c_str());
    if (ubi == GL_INVALID_INDEX) {
        //qDebug() << "Uniform Block " << name << " not found in program " << pId;
    } else {
        f->glUniformBlockBinding(pId, ubi, location);
    }
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

void ShaderProgram::set_uniform_array(const std::string& name, const std::vector<glm::vec3>& array)
{
    if (!m_cached_uniforms.contains(name))
        m_cached_uniforms[name] = m_q_shader_program->uniformLocation(name.c_str());

    const auto uniform_location = m_cached_uniforms.at(name);
    m_q_shader_program->setUniformValueArray(uniform_location, reinterpret_cast<const float*>(array.data()), int(array.size()), 3);
}

// Helper function because i get frustrated with the shader compile errors...
// I want the actual line that an error relates to also outputed...
void outputMeaningfullErrors(const QString& qtLog, const QString& code, const QString& file) {
    QStringList code_lines = code.split('\n');
    qCritical() << "Compiling Error(s) @file: " << file;
#ifndef __EMSCRIPTEN__
    static QRegularExpression re(R"RX((\d+)\((\d+)\) : (.+))RX");
#else
    static QRegularExpression re(R"RX(ERROR: (\d+):(\d+): (.+))RX");
#endif
    re.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator matchIterator = re.globalMatch(qtLog);
    while (matchIterator.hasNext()) {
        QRegularExpressionMatch match = matchIterator.next();

        QString error_message = match.captured(3);
        auto line_number = match.captured(2).toInt();

        if (line_number >= 0 && line_number < code_lines.size()) {
            qCritical() << error_message << " on following line: " << "\n\r" << code_lines[line_number].trimmed();
        } else {
            qCritical() << "Error " << error_message << " appeared on line number which exceeds the input code string.";
        }
    }
}

void ShaderProgram::reload()
{
    QString vertexCode, fragmentCode;
    if (m_code_source == ShaderCodeSource::PLAINTEXT) {
        vertexCode = versionedShaderCode(m_vertex_shader);
        fragmentCode = versionedShaderCode(m_fragment_shader);
    } else if(m_code_source == ShaderCodeSource::FILE) {
        vertexCode = versionedShaderCode(load_shader_code(m_vertex_shader));
        fragmentCode = versionedShaderCode(load_shader_code(m_fragment_shader));
    }
    auto program = std::make_unique<QOpenGLShaderProgram>();
    if (!program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexCode)) {
        outputMeaningfullErrors(program->log(), vertexCode, m_vertex_shader);
    } else if (!program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentCode)) {
        outputMeaningfullErrors(program->log(), fragmentCode, m_fragment_shader);
    } else if (!program->link()) {
        qDebug() << "error linking shader " << m_vertex_shader << "and" << m_fragment_shader;
    } else {
        // NO ERROR
        m_q_shader_program = std::move(program);
        m_cached_attribs.clear();
        m_cached_uniforms.clear();
    }
}

template<typename T>
void ShaderProgram::set_uniform_template(const std::string& name, T value)
{
    if (!m_cached_uniforms.contains(name))
        m_cached_uniforms[name] = m_q_shader_program->uniformLocation(name.c_str());

    const auto uniform_location = m_cached_uniforms.at(name);
    m_q_shader_program->setUniformValue(uniform_location, gl_engine::helpers::toQtType(value));
}
