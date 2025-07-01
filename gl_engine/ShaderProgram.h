 /*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2023 Adam Celerek
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

#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <map>

#include <glm/glm.hpp>
#include <QOpenGLShaderProgram>
#include <QUrl>
#include <QDebug>

#if ALP_ENABLE_SHADER_NETWORK_HOTRELOAD
#include <functional>
#include <QNetworkAccessManager>
#endif

namespace gl_engine {

enum class ShaderCodeSource {
    PLAINTEXT,
    FILE
};

enum class ShaderType {
    VERTEX,
    FRAGMENT
};

class ShaderProgram {
private:
    std::unordered_map<std::string, int> m_cached_uniforms;
    std::unordered_map<std::string, int> m_cached_attribs;
    std::unique_ptr<QOpenGLShaderProgram> m_q_shader_program;
    QString m_vertex_shader;    // either filename or native shader code
    QString m_fragment_shader;  // either filename or native shader code
    ShaderCodeSource m_code_source;
    std::vector<QString> m_defines;

#if ALP_ENABLE_SHADER_NETWORK_HOTRELOAD
    // A temporary cache for the downloaded shader files.
    static std::map<QString, QString> web_download_file_cache;
    // Shared NetworkAccessManager for downloading files
    static std::unique_ptr<QNetworkAccessManager> web_network_manager;
#endif
    // A storage for the content of the shader files, such that includes
    // don't have to be read several times. It also allows for overriding
    // content by download if ALP_ENABLE_SHADER_NETWORK_HOTRELOAD is true
    static std::map<QString, QString> shader_file_cache;

    // Helper function which returns the content of the given shader file
    // as string. Parameter name has to be the name of the shader, eg. "tile.frag".
    static QString read_file_content_local(const QString& name);

    // Returns the content of the given shader file as string. If a cached version
    // exists inside shader_file_cache it will return the cached version. If not
    // it will attempt to read it from file, and add it to cache afterwards.
    static QString read_file_content(const QString& name);

    static QString get_qrc_or_path_prefix();
    static QString get_shader_code_version();
    static QByteArray make_versioned_shader_code(const QByteArray& src);
    static QByteArray make_versioned_shader_code(const QString& src);
    static void preprocess_shader_content_inplace(QString& base);

public:
    ShaderProgram(QString vertex_shader, QString fragment_shader, ShaderCodeSource code_source = ShaderCodeSource::FILE, const std::vector<QString>& defines = {});

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
    void set_uniform_array(const std::string& name, const std::vector<glm::vec3>& array);

    static void reset_shader_cache();

#if ALP_ENABLE_SHADER_NETWORK_HOTRELOAD
    // Redownloads all files inside the shader_file_cache from the
    // WEBGL_SHADER_DOWNLOAD_URL location, and executes the callback when done
    static void web_download_shader_files_and_put_in_cache(std::function<void()> callback);
#endif

public slots:
    void reload();
private:
    template <typename T>
    void set_uniform_template(const std::string& name, T value);

    QString load_and_preprocess_shader_code(gl_engine::ShaderType type);

};
}
