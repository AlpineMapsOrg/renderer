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
#include "ShaderPreprocessor.h"

#include <functional>
#include <iostream>
#include <regex>
#include <sstream>
#include <stack>

namespace webgpu::util {

namespace {
    // Helper function to trim leading and trailing whitespace
    inline std::string trim_whitespace(std::string str) {
        str.erase(0, str.find_first_not_of(" \t"));
        str.erase(str.find_last_not_of(" \t") + 1);
        return str;
    }
}

ShaderPreprocessor::ShaderPreprocessor()
{
    initialize_platform_defines();
}

void ShaderPreprocessor::initialize_platform_defines()
{
    // NOTE: Add more platform-specific defines as needed if you want to use them in shaders
#ifdef QT_DEBUG
    m_global_defines["QT_DEBUG"] = "1";
#endif

#ifdef __EMSCRIPTEN__
    m_global_defines["__EMSCRIPTEN__"] = "1";
#endif

#ifdef ALP_ENABLE_DEV_TOOLS
    m_global_defines["ALP_ENABLE_DEV_TOOLS"] = "1";
#endif

#ifdef _WIN32
    m_global_defines["_WIN32"] = "1";
#endif

#ifdef _WIN64
    m_global_defines["_WIN64"] = "1";
#endif

#ifdef __linux__
    m_global_defines["__linux__"] = "1";
#endif

#ifdef __ANDROID__
    m_global_defines["__ANDROID__"] = "1";
#endif
}

void ShaderPreprocessor::define(const std::string& symbol)
{
    m_global_defines[symbol] = "1";
}

void ShaderPreprocessor::define(const std::string& symbol, const std::string& value)
{
    m_global_defines[symbol] = value;
}

void ShaderPreprocessor::undefine(const std::string& symbol)
{
    m_global_defines.erase(symbol);
}

bool ShaderPreprocessor::is_defined(const std::string& symbol) const
{
    return m_global_defines.contains(symbol);
}

std::string ShaderPreprocessor::get_value(const std::string& symbol) const
{
    auto it = m_global_defines.find(symbol);
    if (it != m_global_defines.end()) {
        return it->second;
    }
    return "";
}

void ShaderPreprocessor::clear_cache()
{
    m_shader_name_to_code.clear();
}

void ShaderPreprocessor::set_cache_enabled(bool enabled)
{
    m_cache_enabled = enabled;
    if (!enabled) {
        clear_cache();
    }
}

void ShaderPreprocessor::set_file_reader(std::function<std::string(const std::string&)> reader)
{
    m_file_reader = std::move(reader);
}

void ShaderPreprocessor::set_error_callback(std::function<void(const std::string&)> callback)
{
    m_error_callback = std::move(callback);
}

void ShaderPreprocessor::report_error(const std::string& message)
{
    if (m_error_callback) {
        m_error_callback(message);
    }
}

std::string ShaderPreprocessor::get_file_contents_with_cache(const std::string& name)
{
    if (m_cache_enabled) {
        const auto found_it = m_shader_name_to_code.find(name);
        if (found_it != m_shader_name_to_code.end()) {
            return found_it->second;
        }
    }

    if (!m_file_reader) {
        report_error("No file reader set for ShaderPreprocessor");
        return "";
    }

    const auto file_contents = m_file_reader(name);

    if (m_cache_enabled) {
        m_shader_name_to_code[name] = file_contents;
    }

    return file_contents;
}

std::string ShaderPreprocessor::process_defines(const std::string& code, std::map<std::string, std::string>& local_defines)
{
    static const std::regex define_regex(R"(^#define\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*(.*)$)");

    std::istringstream input(code);
    std::ostringstream output;
    std::string line;

    while (std::getline(input, line)) {
        // Quick Check: skip regex if line doesn't start with '#'
        if (line.empty() || line[0] != '#') {
            output << line << '\n';
            continue;
        }

        std::smatch match;
        if (std::regex_match(line, match, define_regex)) {
            const std::string symbol = match[1].str();
            std::string value = trim_whitespace(match[2].str());
            if (value.empty()) value = "1"; // default to "1"
            local_defines[symbol] = value;
            // Don't output the #define directive itself
        } else {
            output << line << '\n';
        }
    }

    return output.str();
}

std::string ShaderPreprocessor::process_includes(const std::string& code, std::unordered_set<std::string>& already_included, std::map<std::string, std::string>& local_defines)
{
    static const std::regex include_regex("#include \"([/a-zA-Z0-9 ._-]+)\"");
    std::string preprocessed_code = code;

    std::smatch include_match;
    while (std::regex_search(preprocessed_code, include_match, include_regex)) {
        const std::string included_filename = include_match[1].str(); // first submatch
        const size_t match_pos = include_match.position();
        const size_t match_len = include_match.length();

        if (already_included.contains(included_filename)) {
            // replace with empty string if already included
            preprocessed_code.replace(match_pos, match_len, "");
        } else {
            // NOTE: mark file as included BEFORE processing to prevent infinite recursion
            already_included.insert(included_filename);
            const std::string included_file_contents = get_file_contents_with_cache(included_filename);
            const std::string processed_included_contents = process_includes(included_file_contents, already_included, local_defines);
            preprocessed_code.replace(match_pos, match_len, processed_included_contents);
        }
    }

    return preprocessed_code;
}

std::string ShaderPreprocessor::process_conditionals(const std::string& code, const std::map<std::string, std::string>& local_defines)
{
    // NOTE: static for better performance
    static const std::regex ifdef_regex(R"(^#ifdef\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*$)");
    static const std::regex ifndef_regex(R"(^#ifndef\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*$)");
    static const std::regex if_regex(R"(^#if\s+([a-zA-Z_][a-zA-Z0-9_]*)\s+(.+)$)");
    static const std::regex elif_regex(R"(^#elif\s+([a-zA-Z_][a-zA-Z0-9_]*)\s+(.+)$)");
    static const std::regex endif_regex(R"(^#endif\s*$)");
    static const std::regex else_regex(R"(^#else\s*$)");

    std::istringstream input(code);
    std::ostringstream output;
    std::string line;

    // Stack to track conditional compilation state
    struct ConditionalState {
        bool is_active;          // Should we output code in this block?
        bool was_any_branch_taken; // Has any branch been taken in this if/else chain?
    };
    std::stack<ConditionalState> condition_stack;

    bool is_currently_active = true;

    auto is_symbol_defined = [&](const std::string& symbol) -> bool {
        return m_global_defines.contains(symbol) || local_defines.contains(symbol);
    };

    auto get_symbol_value = [&](const std::string& symbol) -> std::string {
        auto it = local_defines.find(symbol);
        if (it != local_defines.end()) {
            return it->second;
        }
        it = m_global_defines.find(symbol);
        if (it != m_global_defines.end()) {
            return it->second;
        }
        return "";
    };

    while (std::getline(input, line)) {
        // Quick check: if line doesn't start with '#', it's regular code
        if (line.empty() || line[0] != '#') {
            if (is_currently_active) 
                output << line << '\n';
            continue;
        }

        std::smatch match;

        if (std::regex_match(line, match, ifdef_regex)) {
            // #ifdef directive
            const std::string symbol = match[1].str();
            const bool symbol_defined = is_symbol_defined(symbol);
            const bool parent_active = is_currently_active;
            const bool this_active = parent_active && symbol_defined;

            condition_stack.push({this_active, symbol_defined});
            is_currently_active = this_active;

        } else if (std::regex_match(line, match, ifndef_regex)) {
            // #ifndef directive
            const std::string symbol = match[1].str();
            const bool symbol_not_defined = !is_symbol_defined(symbol);
            const bool parent_active = is_currently_active;
            const bool this_active = parent_active && symbol_not_defined;

            condition_stack.push({this_active, symbol_not_defined});
            is_currently_active = this_active;

        } else if (std::regex_match(line, match, if_regex)) {
            // #if directive - compares symbol value with a string
            const std::string symbol = match[1].str();
            const std::string expected_value = trim_whitespace(match[2].str());
            const std::string actual_value = get_symbol_value(symbol);
            const bool condition_met = (actual_value == expected_value);
            const bool parent_active = is_currently_active;
            const bool this_active = parent_active && condition_met;

            condition_stack.push({this_active, condition_met});
            is_currently_active = this_active;

        } else if (std::regex_match(line, match, elif_regex)) {
            // #elif directive
            if (condition_stack.empty()) {
                report_error("Shader preprocessing error: #elif without matching #if/#ifdef/#ifndef");
                continue;
            }

            auto& state = condition_stack.top();

            // Determine parent activity
            bool parent_active = true;
            if (condition_stack.size() > 1) {
                auto stack_copy = condition_stack;
                stack_copy.pop();
                parent_active = stack_copy.top().is_active;
            }

            // Only evaluate elif if parent is active and no branch was taken yet
            if (parent_active && !state.was_any_branch_taken) {
                const std::string symbol = match[1].str();
                const std::string expected_value = trim_whitespace(match[2].str());

                const std::string actual_value = get_symbol_value(symbol);
                const bool condition_met = (actual_value == expected_value);

                is_currently_active = condition_met;
                state.is_active = condition_met;
                if (condition_met) {
                    state.was_any_branch_taken = true;
                }
            } else {
                is_currently_active = false;
            }

        } else if (std::regex_match(line, else_regex)) {
            // #else directive
            if (condition_stack.empty()) {
                report_error("Shader preprocessing error: #else without matching #if/#ifdef/#ifndef");
                continue;
            }

            auto& state = condition_stack.top();

            // Determine parent activity
            bool parent_active = true;
            if (condition_stack.size() > 1) {
                auto stack_copy = condition_stack;
                stack_copy.pop();
                parent_active = stack_copy.top().is_active;
            }

            // #else is active if parent is active and no previous branch was taken
            is_currently_active = parent_active && !state.was_any_branch_taken;
            state.is_active = is_currently_active;
            state.was_any_branch_taken = true;

        } else if (std::regex_match(line, endif_regex)) {
            if (condition_stack.empty()) {
                report_error("Shader preprocessing error: #endif without matching #if/#ifdef/#ifndef");
                continue;
            }

            condition_stack.pop();

            // Restore parent activity state
            if (condition_stack.empty()) {
                is_currently_active = true;
            } else {
                is_currently_active = condition_stack.top().is_active;
            }

        } else {
            //  starts with '#' but doesn't match any directive - treat as regular code
            if (is_currently_active) {
                output << line << '\n';
            }
        }
    }

    if (!condition_stack.empty()) {
        report_error("Shader preprocessing error: unclosed #if/#ifdef/#ifndef directive");
    }

    return output.str();
}

std::string ShaderPreprocessor::replace_macros(const std::string& code, const std::map<std::string, std::string>& local_defines)
{
    // Merge global and local defines (local takes precedence)
    std::map<std::string, std::string> all_defines = m_global_defines;
    for (const auto& [symbol, value] : local_defines) {
        all_defines[symbol] = value;
    }

    // NOTE: We process in reverse order of symbol length to handle longer symbols first
    // for the case where one symbol is a part of another (e.g., VALUE and VAL)
    std::vector<std::pair<std::string, std::string>> sorted_defines(all_defines.begin(), all_defines.end());
    std::sort(sorted_defines.begin(), sorted_defines.end(),
        [](const auto& a, const auto& b) { return a.first.length() > b.first.length(); });

    std::string result;
    result.reserve(code.length()); // Reserve space to avoid reallocations

    size_t pos = 0;
    while (pos < code.length()) {
        bool replaced = false;

        // Check each symbol at current position
        for (const auto& [symbol, value] : sorted_defines) {
            const size_t sym_len = symbol.length();

            // Check if symbol matches at current position
            if (pos + sym_len <= code.length() && code.compare(pos, sym_len, symbol) == 0) {
                // Check word boundaries: previous character must not be alphanumeric/underscore
                const bool prev_ok = (pos == 0 || (!std::isalnum(code[pos - 1]) && code[pos - 1] != '_'));
                // Next character must not be alphanumeric/underscore
                const bool next_ok = (pos + sym_len >= code.length() || (!std::isalnum(code[pos + sym_len]) && code[pos + sym_len] != '_'));

                if (prev_ok && next_ok) {
                    result += value;
                    pos += sym_len;
                    replaced = true;
                    break;
                }
            }
        }

        if (!replaced) {
            result += code[pos];
            ++pos;
        }
    }

    return result;
}

std::string ShaderPreprocessor::preprocess_file(const std::string& name)
{
    const std::string code = get_file_contents_with_cache(name);
    return preprocess_code(code);
}

std::string ShaderPreprocessor::preprocess_code(const std::string& code)
{
    std::map<std::string, std::string> local_defines;
    std::string code_with_defines = process_defines(code, local_defines);
    std::unordered_set<std::string> already_included;
    std::string code_with_includes = process_includes(code_with_defines, already_included, local_defines);
    std::string code_with_conditionals = process_conditionals(code_with_includes, local_defines);
    std::string final_code = replace_macros(code_with_conditionals, local_defines);
    return final_code;
}

} // namespace webgpu::util
