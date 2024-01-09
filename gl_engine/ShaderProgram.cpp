 /*****************************************************************************
 * Alpine Renderer
 * Copyright (C) 2023 Adam Celarek
 * Copyright (C) 2023 Gerald Kimmersdorfer
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include "ShaderProgram.h"

#include <iostream>

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QOpenGLContext>
#include <QOpenGLShader>
#include <QOpenGLExtraFunctions>
#include <QDebug>

#if defined(__ANDROID__)
#include <GLES3/gl3.h>  // for GL_INVALID_INDEX! DONT EXACTLY KNOW WHY I NEED THIS HERE! (on other platforms it works without)
#endif

// FOR DOWNLOADING SHADERS
#if ALP_ENABLE_SHADER_NETWORK_HOTRELOAD
#include <QNetworkRequest>
#include <QNetworkReply>
#endif

#include "helpers.h"

using gl_engine::ShaderProgram;


QString ShaderProgram::get_qrc_or_path_prefix() {
    QString prefix = ":/gl_shaders/";
    if (!QOpenGLContext::currentContext()->isOpenGLES())
        prefix = ALP_RESOURCES_PREFIX; // FOR NATIVE BUILD: ALP_RESOURCES_PREFIX = "shaders/";
    return prefix;
}

QString ShaderProgram::get_shader_code_version() {
    if (QOpenGLContext::currentContext()->isOpenGLES())
        return "#version 300 es\n";
    else
        return "#version 330\n";
}

QByteArray ShaderProgram::make_versioned_shader_code(const QByteArray& src)
{
    QByteArray versionedSrc;
    versionedSrc.append(get_shader_code_version().toLocal8Bit());
    versionedSrc.append(src);
    return versionedSrc;
}

QByteArray ShaderProgram::make_versioned_shader_code(const QString& src) {
    QByteArray versionedSrc;
    versionedSrc.append(get_shader_code_version().toLocal8Bit());
    versionedSrc.append(src.toLocal8Bit());
    return versionedSrc;
}

void ShaderProgram::preprocess_shader_content_inplace(QString& base) {
    static QRegularExpression re(R"RX(^\s*#\s*include\s+"(?<file>[^"]+)")RX");
    re.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    re.setPatternOptions(QRegularExpression::MultilineOption);

    // Create an iterator to go through matches in the text
    QRegularExpressionMatchIterator i = re.globalMatch(base);

    int positionOffset = 0;
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        // Get the whole matched string
        QString includeFileName = match.captured("file");
        QString includeContent = read_file_content(includeFileName);
        preprocess_shader_content_inplace(includeContent);
        int position = match.capturedStart(0) + positionOffset;
        base.remove(position, match.capturedLength(0));
        // Insert the new text
        base.insert(position, includeContent);
        positionOffset += includeContent.length() - match.capturedLength(0);
    }
}

// ========== STATIC DECLARATIONS =====================
std::map<QString, QString> ShaderProgram::shader_file_cache = {};

#if ALP_ENABLE_SHADER_NETWORK_HOTRELOAD

std::map<QString, QString> ShaderProgram::web_download_file_cache = {};
std::unique_ptr<QNetworkAccessManager> ShaderProgram::web_network_manager = nullptr;

void ShaderProgram::web_download_shader_files_and_put_in_cache(std::function<void()> callback) {
    // Initialize network manager on correct thread:
    if (!web_network_manager) web_network_manager = std::make_unique<QNetworkAccessManager>();

    for (const auto& cache_entry : shader_file_cache) {
        auto name = cache_entry.first;
        auto url = QUrl(ALP_SHADER_NETWORK_URL + name);

        QNetworkRequest request(url);
        request.setTransferTimeout(500);
        // IMPORTANT: QNetworkRequest::AlwaysNetwork doesn't seem to be supported for WebAssembly!!!
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferNetwork);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0) // Don't know if i need that but i copied it from the TileLoadService
        request.setAttribute(QNetworkRequest::UseCredentialsAttribute, false);
#endif
        QNetworkReply* reply = web_network_manager->get(request);
        //qDebug() << "requested file " << name;
        QObject::connect(reply, &QNetworkReply::finished, [reply, name, callback]() {
            if (reply->error() == QNetworkReply::NoError) {
                //qDebug() << name << "put into filecache";
                web_download_file_cache[name] = reply->readAll();
            } else {
                qWarning("Error at downloading file (%s). Local file will be reload.",  qPrintable(reply->errorString()));
                web_download_file_cache[name] = read_file_content_local(name); // Just so that the overall count in web_download_file_cache will be correct
            }
            reply->deleteLater(); // Schedule the reply object for deletion.

            if (web_download_file_cache.size() == shader_file_cache.size()) {
                //qDebug() << "All files are downloaded!";
                shader_file_cache = std::move(web_download_file_cache);
                web_download_file_cache.clear(); // might not be necessary because of move
                callback();
            }
        });
    }
}
#endif

void ShaderProgram::reset_shader_cache() {
    shader_file_cache.clear();
}

QString ShaderProgram::read_file_content(const QString& name) {
    // Check for cached version:
    auto it = shader_file_cache.find(name);
    if (it != shader_file_cache.end()) return it->second;
    // if not in cache lets hope its local:
    auto content = read_file_content_local(name);
    // now put it in cache:
    shader_file_cache[name] = content;
    return content;
}

QString ShaderProgram::read_file_content_local(const QString& name) {
    QFile file(get_qrc_or_path_prefix() + name);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "Cannot open file:" << name << "for reading:" << file.errorString();
        return QString();
    }
    QTextStream in(&file);
    QString content = in.readAll();
    if (in.status() != QTextStream::Ok) {
        qCritical() << "Error reading file:" << name << ":" << in.status();
        file.close();
        return QString();
    }
    file.close();
    return content;
}

// =========== MEMBER DECLARATIONS =======================

ShaderProgram::ShaderProgram(QString vertex_shader, QString fragment_shader, ShaderCodeSource code_source)
    : m_vertex_shader(vertex_shader)
    , m_fragment_shader(fragment_shader)
    , m_code_source(code_source)
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
void outputMeaningfullErrors(const QString& qtLog, const QString& code, const QString& file)
{
#if defined(_MSC_VER)
    // msvc has a different error text (matchers don't work)
    // when using msvc in github ci qDebug/Critical don't print when an assert fails
    // effectively, we don't see any error
    std::cerr << "Compiling Error(s) @file: " << file.toStdString() << "\n" << qtLog.toStdString() << std::endl;
    fflush(stderr);
    fflush(stdout);
    return;
#else
#if !defined(__EMSCRIPTEN__)
    static QRegularExpression re(R"RX((\d+)\((\d+)\) : (.+))RX");
#else
    static QRegularExpression re(R"RX(ERROR: (\d+):(\d+): (.+))RX");
#endif
    re.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    QStringList code_lines = code.split('\n');
    qCritical() << "Compiling Error(s) @file: " << file.toStdString();
    QRegularExpressionMatchIterator matchIterator = re.globalMatch(qtLog);
    while (matchIterator.hasNext()) {
        QRegularExpressionMatch match = matchIterator.next();

        QString error_message = match.captured(3);
        auto line_number = match.captured(2).toInt();

        if (line_number >= 0 && line_number < code_lines.size()) {
            qCritical() << error_message.toStdString() << " on following line: "
                        << "\n\r" << code_lines[line_number].trimmed().toStdString();
        } else {
            qCritical() << "Error " << error_message.toStdString() << " appeared on line number which exceeds the input code string.";
        }
    }
#endif
}

void ShaderProgram::reload()
{
    QString vertexCode = load_and_preprocess_shader_code(gl_engine::ShaderType::VERTEX);
    QString fragmentCode = load_and_preprocess_shader_code(gl_engine::ShaderType::FRAGMENT);
    auto program = std::make_unique<QOpenGLShaderProgram>();
    if (!program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexCode)) {
        outputMeaningfullErrors(program->log(), vertexCode, m_vertex_shader);
    } else if (!program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentCode)) {
        outputMeaningfullErrors(program->log(), fragmentCode, m_fragment_shader);
    } else if (!program->link()) {
#ifdef _MSC_VER
        // when using msvc in github ci qDebug/Critical don't print when an assert fails
        // effectively, we don't see any error
        std::cerr << "error linking shader " << m_vertex_shader.toStdString() << "and" << m_fragment_shader.toStdString() << std::endl;
        fflush(stderr);
        fflush(stdout);
#else
        qCritical() << "error linking shader " << m_vertex_shader.toStdString() << "and" << m_fragment_shader.toStdString();
#endif
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

QString ShaderProgram::load_and_preprocess_shader_code(gl_engine::ShaderType type) {
    QString code = (type == gl_engine::ShaderType::VERTEX) ? m_vertex_shader : m_fragment_shader;
    if (m_code_source == ShaderCodeSource::FILE)
        code = read_file_content(code);

    preprocess_shader_content_inplace(code);
    return make_versioned_shader_code(code);
}
