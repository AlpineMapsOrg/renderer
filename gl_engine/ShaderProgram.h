#pragma once

// If true the shaders will be loaded from the given WEBGL_SHADER_DOWNLOAD_URL
// and can be reloaded inside the APP without the need for recompilation
#define WEBGL_SHADER_DOWNLOAD_ACCESS true
#define WEBGL_SHADER_DOWNLOAD_URL "http://localhost:5500/"
#define WEBGL_SHADER_DOWNLOAD_TIMEOUT 8000
// ALP_ENABLE_SHADER_WEB_HOTRELOAD

#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <map>

#include <glm/glm.hpp>
#include <QOpenGLShaderProgram>
#include <QUrl>
#include <QDebug>

#if WEBGL_SHADER_DOWNLOAD_ACCESS
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

#if WEBGL_SHADER_DOWNLOAD_ACCESS
    // A temporary cache for the downloaded shader files.
    static std::map<QString, QString> web_download_file_cache;
    // Shared NetworkAccessManager for downloading files
    static std::unique_ptr<QNetworkAccessManager> web_network_manager;
#endif
    // A storage for the content of the shader files, such that includes
    // don't have to be read several times. It also allows for overriding
    // content by download if WEBGL_SHADER_DOWNLOAD_ACCESS is true
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
    void set_uniform_array(const std::string& name, const std::vector<glm::vec3>& array);

    static void reset_shader_cache();

#if WEBGL_SHADER_DOWNLOAD_ACCESS
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
