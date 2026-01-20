/*****************************************************************************
 * weBIGeo
 * Copyright (C) 2025 Gerald Kimmersdorfer
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

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <unordered_set>

namespace webgpu::util {

/**
 * A text-based shader preprocessor for intended use with WebGPU shaders.
 *
 * Features:
 * - File inclusion with #include "filename"
 * - Automatic pragma once behavior (each file included only once)
 * - Conditional compilation with #ifdef/#ifndef/#else/#endif
 * - Value-based conditionals with #if/#elif/#else/#endif (only supports equality checks)
 * - Macro definitions with #define SYMBOL or #define SYMBOL value
 * - Environment variable defines (global, persist across calls)
 * - File content caching
 * 
 * IMPORTANT: All directives must be at the start of a line (no leading spaces/tabs)!
 */
class ShaderPreprocessor {
public:
    ShaderPreprocessor();
    ~ShaderPreprocessor() = default;

    /**
     * Preprocesses a shader file by name, resolving includes and conditional compilation.
     * @param name The name of the shader file to preprocess
     */
    std::string preprocess_file(const std::string& name);

    /**
     * Preprocesses shader code directly, resolving includes and conditional compilation.
     * @param code The shader code to preprocess
     */
    std::string preprocess_code(const std::string& code);

    /**
     * Defines a global preprocessor symbol for use in #ifdef/#ifndef directives.
     */
    void define(const std::string& symbol);

    /**
     * Defines a global preprocessor symbol with a value.
     */
    void define(const std::string& symbol, const std::string& value);

    /**
     * Undefines a global preprocessor symbol.
     */
    void undefine(const std::string& symbol);

    /**
     * Checks if a symbol is defined globally.
     */
    bool is_defined(const std::string& symbol) const;

    /**
     * Gets the value of a defined symbol. Returns empty string if not defined or has no value.
     */
    std::string get_value(const std::string& symbol) const;

    /**
     * Clears the file cache. Useful for hot-reloading shaders during development.
     */
    void clear_cache();

    /**
     * Enables or disables file caching. When disabled, files are read fresh each time.
     * Caching is enabled by default.
     */
    void set_cache_enabled(bool enabled);

    /**
     * Sets the file reading callback. This allows the preprocessor to read files
     * without depending on specific file I/O implementations.
     */
    void set_file_reader(std::function<std::string(const std::string&)> reader);

    /**
     * Sets the error callback. Called when a preprocessing error occurs.
     * The preprocessor will try to continue processing after an error when possible.
     * The callback can terminate the program if desired (e.g., by calling qFatal).
     */
    void set_error_callback(std::function<void(const std::string&)> callback);

private:
    std::string get_file_contents_with_cache(const std::string& name);
    std::string process_defines(const std::string& code, std::map<std::string, std::string>& local_defines);
    std::string process_includes(const std::string& code, std::unordered_set<std::string>& already_included, std::map<std::string, std::string>& local_defines);
    std::string process_conditionals(const std::string& code, const std::map<std::string, std::string>& local_defines);
    std::string replace_macros(const std::string& code, const std::map<std::string, std::string>& local_defines);
    void initialize_platform_defines();

    void report_error(const std::string& message);

private:
    std::map<std::string, std::string> m_shader_name_to_code;  // Cache of file contents
    std::map<std::string, std::string> m_global_defines;       // Global preprocessor symbols and their values (persist across calls, "1" if no value specified)
    std::function<std::string(const std::string&)> m_file_reader; // Callback for reading files
    std::function<void(const std::string&)> m_error_callback;   // Callback for error reporting
    bool m_cache_enabled = true;
};

} // namespace webgpu::util
