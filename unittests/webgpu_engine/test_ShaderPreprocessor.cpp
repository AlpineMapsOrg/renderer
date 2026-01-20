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

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <webgpu/util/ShaderPreprocessor.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace webgpu::util;

// Helper function to set up error callback for tests
inline void setup_error_callback(ShaderPreprocessor& preprocessor)
{
    preprocessor.set_error_callback([](const std::string& message) {
        FAIL(message);
    });
}

// Helper class to create a mock file system for testing
class MockFileSystem {
public:
    void add_file(const std::string& name, const std::string& content)
    {
        m_files[name] = content;
    }

    std::string read_file(const std::string& name) const
    {
        auto it = m_files.find(name);
        if (it == m_files.end()) {
            throw std::runtime_error("File not found: " + name);
        }
        return it->second;
    }

private:
    std::map<std::string, std::string> m_files;
};

TEST_CASE("ShaderPreprocessor - Include Statement Tests")
{
    ShaderPreprocessor preprocessor;
    setup_error_callback(preprocessor);
    MockFileSystem fs;

    SECTION("Simple include")
    {
        fs.add_file("header.wgsl", "// Header content\nfn helper() -> f32 { return 1.0; }");

        preprocessor.set_file_reader([&fs](const std::string& name) {
            return fs.read_file(name);
        });

        std::string input = R"(
// Main file
#include "header.wgsl"

fn main() -> f32 {
    return helper();
}
)";

        std::string result = preprocessor.preprocess_code(input);

        REQUIRE(result.find("fn helper() -> f32") != std::string::npos);
        REQUIRE(result.find("fn main() -> f32") != std::string::npos);
        REQUIRE(result.find("#include") == std::string::npos);
    }

    SECTION("Multiple includes of the same file (pragma once behavior)")
    {
        fs.add_file("common.wgsl", "const PI: f32 = 3.14159;");

        preprocessor.set_file_reader([&fs](const std::string& name) {
            return fs.read_file(name);
        });

        std::string input = R"(
#include "common.wgsl"
#include "common.wgsl"
#include "common.wgsl"

fn area(r: f32) -> f32 {
    return PI * r * r;
}
)";

        std::string result = preprocessor.preprocess_code(input);

        // Count occurrences of "const PI"
        size_t count = 0;
        size_t pos = 0;
        while ((pos = result.find("const PI", pos)) != std::string::npos) {
            count++;
            pos += 8;
        }

        REQUIRE(count == 1); // Should only appear once due to pragma once behavior
    }

    SECTION("Nested includes with pragma once")
    {
        fs.add_file("constants.wgsl", "const E: f32 = 2.71828;");
        fs.add_file("math_utils.wgsl", "#include \"constants.wgsl\"\nfn exp_approx(x: f32) -> f32 { return E; }");
        fs.add_file("physics.wgsl", "#include \"constants.wgsl\"\nfn decay(t: f32) -> f32 { return E; }");

        preprocessor.set_file_reader([&fs](const std::string& name) {
            return fs.read_file(name);
        });

        std::string input = R"(
#include "math_utils.wgsl"
#include "physics.wgsl"
#include "constants.wgsl"

fn main() {}
)";

        std::string result = preprocessor.preprocess_code(input);

        // Count occurrences of "const E"
        size_t count = 0;
        size_t pos = 0;
        while ((pos = result.find("const E", pos)) != std::string::npos) {
            count++;
            pos += 7;
        }

        REQUIRE(count == 1); // Should only appear once even though included multiple times
    }

    SECTION("Circular include detection")
    {
        fs.add_file("a.wgsl", "#include \"b.wgsl\"\nfn a() {}");
        fs.add_file("b.wgsl", "#include \"c.wgsl\"\nfn b() {}");
        fs.add_file("c.wgsl", "#include \"a.wgsl\"\nfn c() {}");

        preprocessor.set_file_reader([&fs](const std::string& name) {
            return fs.read_file(name);
        });

        std::string input = "#include \"a.wgsl\"";

        // Should not throw or infinite loop - pragma once behavior should prevent cycles
        std::string result;
        REQUIRE_NOTHROW(result = preprocessor.preprocess_code(input));

        // Verify all three files are included (each once)
        REQUIRE(result.find("fn a()") != std::string::npos);
        REQUIRE(result.find("fn b()") != std::string::npos);
        REQUIRE(result.find("fn c()") != std::string::npos);
    }

    SECTION("Include with path separators")
    {
        fs.add_file("util/helpers.wgsl", "fn utility() -> i32 { return 42; }");

        preprocessor.set_file_reader([&fs](const std::string& name) {
            return fs.read_file(name);
        });

        std::string input = R"(
#include "util/helpers.wgsl"

fn test() -> i32 {
    return utility();
}
)";

        std::string result = preprocessor.preprocess_code(input);

        REQUIRE(result.find("fn utility()") != std::string::npos);
        REQUIRE(result.find("#include") == std::string::npos);
    }
}

TEST_CASE("ShaderPreprocessor - Conditional Compilation Tests")
{
    ShaderPreprocessor preprocessor;
    setup_error_callback(preprocessor);

    SECTION("Simple ifdef - symbol defined")
    {
        preprocessor.define("FEATURE_ENABLED");

        std::string input = R"(
fn main() {
#ifdef FEATURE_ENABLED
    let x = 1.0;
#endif
}
)";

        std::string result = preprocessor.preprocess_code(input);

        REQUIRE(result.find("let x = 1.0;") != std::string::npos);
        REQUIRE(result.find("#ifdef") == std::string::npos);
        REQUIRE(result.find("#endif") == std::string::npos);
    }

    SECTION("Simple ifdef - symbol not defined")
    {
        std::string input = R"(
fn main() {
#ifdef FEATURE_DISABLED
    let x = 1.0;
#endif
}
)";

        std::string result = preprocessor.preprocess_code(input);

        REQUIRE(result.find("let x = 1.0;") == std::string::npos);
        REQUIRE(result.find("#ifdef") == std::string::npos);
    }

    SECTION("Simple ifndef - symbol not defined")
    {
        std::string input = R"(
fn main() {
#ifndef DEBUG_MODE
    let optimized = true;
#endif
}
)";

        std::string result = preprocessor.preprocess_code(input);

        REQUIRE(result.find("let optimized = true;") != std::string::npos);
        REQUIRE(result.find("#ifndef") == std::string::npos);
    }

    SECTION("Simple ifndef - symbol defined")
    {
        preprocessor.define("DEBUG_MODE");

        std::string input = R"(
fn main() {
#ifndef DEBUG_MODE
    let optimized = true;
#endif
}
)";

        std::string result = preprocessor.preprocess_code(input);

        REQUIRE(result.find("let optimized = true;") == std::string::npos);
    }

    SECTION("ifdef with else - true branch")
    {
        preprocessor.define("USE_FAST_PATH");

        std::string input = R"(
fn compute() {
#ifdef USE_FAST_PATH
    let result = fast_compute();
#else
    let result = slow_compute();
#endif
}
)";

        std::string result = preprocessor.preprocess_code(input);

        REQUIRE(result.find("fast_compute()") != std::string::npos);
        REQUIRE(result.find("slow_compute()") == std::string::npos);
        REQUIRE(result.find("#ifdef") == std::string::npos);
        REQUIRE(result.find("#else") == std::string::npos);
    }

    SECTION("ifdef with else - false branch")
    {
        std::string input = R"(
fn compute() {
#ifdef USE_FAST_PATH
    let result = fast_compute();
#else
    let result = slow_compute();
#endif
}
)";

        std::string result = preprocessor.preprocess_code(input);

        REQUIRE(result.find("fast_compute()") == std::string::npos);
        REQUIRE(result.find("slow_compute()") != std::string::npos);
    }

    SECTION("Nested ifdef")
    {
        preprocessor.define("OUTER_FEATURE");
        preprocessor.define("INNER_FEATURE");

        std::string input = R"(
fn main() {
#ifdef OUTER_FEATURE
    let outer = 1;
#ifdef INNER_FEATURE
    let inner = 2;
#endif
    let outer_end = 3;
#endif
}
)";

        std::string result = preprocessor.preprocess_code(input);

        REQUIRE(result.find("let outer = 1;") != std::string::npos);
        REQUIRE(result.find("let inner = 2;") != std::string::npos);
        REQUIRE(result.find("let outer_end = 3;") != std::string::npos);
    }

    SECTION("Nested ifdef - outer false")
    {
        preprocessor.define("INNER_FEATURE");

        std::string input = R"(
fn main() {
#ifdef OUTER_FEATURE
    let outer = 1;
#ifdef INNER_FEATURE
    let inner = 2;
#endif
    let outer_end = 3;
#endif
}
)";

        std::string result = preprocessor.preprocess_code(input);

        // Outer is false, so nothing inside should be included
        REQUIRE(result.find("let outer = 1;") == std::string::npos);
        REQUIRE(result.find("let inner = 2;") == std::string::npos);
        REQUIRE(result.find("let outer_end = 3;") == std::string::npos);
    }

    SECTION("Multiple independent conditionals")
    {
        preprocessor.define("FEATURE_A");
        preprocessor.define("FEATURE_C");

        std::string input = R"(
fn main() {
#ifdef FEATURE_A
    let a = 1;
#endif

#ifdef FEATURE_B
    let b = 2;
#endif

#ifdef FEATURE_C
    let c = 3;
#endif
}
)";

        std::string result = preprocessor.preprocess_code(input);

        REQUIRE(result.find("let a = 1;") != std::string::npos);
        REQUIRE(result.find("let b = 2;") == std::string::npos);
        REQUIRE(result.find("let c = 3;") != std::string::npos);
    }

    SECTION("Undefine symbol")
    {
        preprocessor.define("TEMPORARY");
        REQUIRE(preprocessor.is_defined("TEMPORARY"));

        preprocessor.undefine("TEMPORARY");
        REQUIRE_FALSE(preprocessor.is_defined("TEMPORARY"));
    }
}

TEST_CASE("ShaderPreprocessor - Combined Include and Conditional Tests")
{
    ShaderPreprocessor preprocessor;
    setup_error_callback(preprocessor);
    MockFileSystem fs;

    SECTION("Include file with conditionals")
    {
        preprocessor.define("ENABLE_SHADOWS");

        fs.add_file("lighting.wgsl", R"(
#ifdef ENABLE_SHADOWS
fn compute_shadows() -> f32 { return 0.5; }
#else
fn compute_shadows() -> f32 { return 1.0; }
#endif
)");

        preprocessor.set_file_reader([&fs](const std::string& name) {
            return fs.read_file(name);
        });

        std::string input = R"(
#include "lighting.wgsl"

fn main() {
    let shadow = compute_shadows();
}
)";

        std::string result = preprocessor.preprocess_code(input);

        REQUIRE(result.find("return 0.5;") != std::string::npos);
        REQUIRE(result.find("return 1.0;") == std::string::npos);
    }

    SECTION("Conditional include")
    {
        preprocessor.define("USE_ADVANCED_MATH");

        fs.add_file("basic_math.wgsl", "fn add(a: f32, b: f32) -> f32 { return a + b; }");
        fs.add_file("advanced_math.wgsl", "fn multiply(a: f32, b: f32) -> f32 { return a * b; }");

        preprocessor.set_file_reader([&fs](const std::string& name) {
            return fs.read_file(name);
        });

        std::string input = R"(
#ifdef USE_ADVANCED_MATH
#include "advanced_math.wgsl"
#else
#include "basic_math.wgsl"
#endif

fn main() {}
)";

        std::string result = preprocessor.preprocess_code(input);

        REQUIRE(result.find("multiply") != std::string::npos);
        REQUIRE(result.find("add") == std::string::npos);
    }
}

TEST_CASE("ShaderPreprocessor - Error Handling")
{
    auto test_error = [](const std::string& input) {
        ShaderPreprocessor preprocessor;
        bool error_called = false;
        preprocessor.set_error_callback([&error_called](const std::string&) {
            error_called = true;
        });
        preprocessor.preprocess_code(input);
        REQUIRE(error_called);
    };

    SECTION("Unclosed ifdef")
    {
        test_error("fn main() {\n#ifdef FEATURE\n    let x = 1;\n}\n");
    }

    SECTION("endif without ifdef")
    {
        test_error("fn main() {\n    let x = 1;\n#endif\n}\n");
    }

    SECTION("else without ifdef")
    {
        test_error("fn main() {\n#else\n    let x = 1;\n#endif\n}\n");
    }
}

TEST_CASE("ShaderPreprocessor - Cache Management")
{
    ShaderPreprocessor preprocessor;
    setup_error_callback(preprocessor);
    MockFileSystem fs;

    fs.add_file("cacheable.wgsl", "const VALUE: i32 = 42;");

    preprocessor.set_file_reader([&fs](const std::string& name) {
        return fs.read_file(name);
    });

    SECTION("Cache behavior")
    {
        std::string input = "#include \"cacheable.wgsl\"";

        // First preprocess - should cache the file
        std::string result1 = preprocessor.preprocess_code(input);
        REQUIRE(result1.find("const VALUE") != std::string::npos);

        // Modify the file in the mock filesystem
        fs.add_file("cacheable.wgsl", "const VALUE: i32 = 99;");

        // Second preprocess - should use cached version
        std::string result2 = preprocessor.preprocess_code(input);
        REQUIRE(result2.find("const VALUE: i32 = 42;") != std::string::npos);

        // Clear cache
        preprocessor.clear_cache();

        // Third preprocess - should read new version
        std::string result3 = preprocessor.preprocess_code(input);
        REQUIRE(result3.find("const VALUE: i32 = 99;") != std::string::npos);
    }
}

TEST_CASE("ShaderPreprocessor - Define and Macro Replacement Tests")
{
    ShaderPreprocessor preprocessor;
    setup_error_callback(preprocessor);

    SECTION("#define with value and macro replacement")
    {
        std::string input = R"(
#define PI 3.14159
#define RADIUS 10.0

fn area() -> f32 {
    return PI * RADIUS * RADIUS;
}
)";

        std::string result = preprocessor.preprocess_code(input);

        REQUIRE(result.find("3.14159") != std::string::npos);
        REQUIRE(result.find("10.0") != std::string::npos);
        REQUIRE(result.find("PI") == std::string::npos);
        REQUIRE(result.find("RADIUS") == std::string::npos);
        REQUIRE(result.find("#define") == std::string::npos);
    }

    SECTION("#define without value defaults to 1")
    {
        std::string input = R"(
#define ENABLED

fn test() -> i32 {
    return ENABLED;
}
)";

        std::string result = preprocessor.preprocess_code(input);

        REQUIRE(result.find("return 1;") != std::string::npos);
        REQUIRE(result.find("ENABLED") == std::string::npos);
    }

    SECTION("#if with value comparison")
    {
        std::string input = R"(
#define MODUS 1

fn main() {
#if MODUS 1
    let x = 1.0;
#endif
}
)";

        std::string result = preprocessor.preprocess_code(input);

        REQUIRE(result.find("let x = 1.0;") != std::string::npos);
    }

    SECTION("#if with value comparison - false")
    {
        std::string input = R"(
#define MODUS 1

fn main() {
#if MODUS 2
    let x = 1.0;
#endif
}
)";

        std::string result = preprocessor.preprocess_code(input);

        REQUIRE(result.find("let x = 1.0;") == std::string::npos);
    }

    SECTION("#if with #elif chain")
    {
        std::string input = R"(
#define MODUS 2

fn main() {
#if MODUS 1
    let msg = first;
#elif MODUS 2
    let msg = second;
#elif MODUS 3
    let msg = third;
#else
    let msg = default;
#endif
}
)";

        std::string result = preprocessor.preprocess_code(input);

        REQUIRE(result.find("let msg = second;") != std::string::npos);
        REQUIRE(result.find("let msg = first;") == std::string::npos);
        REQUIRE(result.find("let msg = third;") == std::string::npos);
        REQUIRE(result.find("let msg = default;") == std::string::npos);
    }

    SECTION("#elif falls through to #else when no match")
    {
        std::string input = R"(
#define MODUS 99

fn main() {
#if MODUS 1
    let msg = first;
#elif MODUS 2
    let msg = second;
#else
    let msg = default;
#endif
}
)";

        std::string result = preprocessor.preprocess_code(input);

        REQUIRE(result.find("let msg = default;") != std::string::npos);
        REQUIRE(result.find("let msg = first;") == std::string::npos);
        REQUIRE(result.find("let msg = second;") == std::string::npos);
    }

    SECTION("Combined #ifdef and #define")
    {
        std::string input = R"(
#define PI 3.14159
#define MODUS 1

fn main() {
#ifdef PI
    let pi = PI;
#endif
#if MODUS 1
    let mode = MODUS;
#else
    let mode = 0;
#endif
}
)";

        std::string result = preprocessor.preprocess_code(input);

        REQUIRE(result.find("let pi = 3.14159;") != std::string::npos);
        REQUIRE(result.find("let mode = 1;") != std::string::npos);
        REQUIRE(result.find("let mode = 0;") == std::string::npos);
    }

    SECTION("Global define vs local define")
    {
        preprocessor.define("GLOBAL_VAR", "100");

        std::string input = R"(
#define LOCAL_VAR 200

fn test() -> i32 {
    return GLOBAL_VAR + LOCAL_VAR;
}
)";

        std::string result = preprocessor.preprocess_code(input);

        REQUIRE(result.find("return 100 + 200;") != std::string::npos);

        // Local define should not persist
        std::string input2 = R"(
fn test2() -> i32 {
    return GLOBAL_VAR;
}
)";

        std::string result2 = preprocessor.preprocess_code(input2);

        REQUIRE(result2.find("return 100;") != std::string::npos);
        REQUIRE(result2.find("LOCAL_VAR") == std::string::npos);
    }

    SECTION("Local define overrides global define")
    {
        preprocessor.define("VALUE", "global");

        std::string input = R"(
#define VALUE local

fn test() {
    let v = VALUE;
}
)";

        std::string result = preprocessor.preprocess_code(input);

        REQUIRE(result.find("let v = local;") != std::string::npos);
        REQUIRE(result.find("global") == std::string::npos);
    }
}

TEST_CASE("ShaderPreprocessor - Benchmark")
{
    namespace fs = std::filesystem;

    fs::path shader_dir = ALP_RESOURCES_PREFIX;

    if (!fs::exists(shader_dir)) {
        SKIP("Shader directory not found - skipping benchmark");
    }

    std::vector<std::string> shader_files;

    // collect shader files in the root directory only
    for (const auto& entry : fs::directory_iterator(shader_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".wgsl") {
            shader_files.push_back(entry.path().filename().string());
        }
    }

    if (shader_files.empty()) {
        SKIP("No shader files found - skipping benchmark");
    }

    auto create_file_reader = [&shader_dir]() {
        return [&shader_dir](const std::string& name) -> std::string {
            fs::path file_path = shader_dir / name;
            std::ifstream file(file_path);
            if (!file.is_open()) {
                throw std::runtime_error("File not found: " + name);
            }
            std::stringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        };
    };

    BENCHMARK("Preprocess all shaders (cache enabled)") {
        static ShaderPreprocessor preprocessor;
        setup_error_callback(preprocessor);
        preprocessor.set_file_reader(create_file_reader());

        size_t total = 0;
        for (const auto& shader_file : shader_files) {
            total += preprocessor.preprocess_file(shader_file).size();
        }
        return total;
    };

    BENCHMARK("Preprocess all shaders (cache disabled)") {
        ShaderPreprocessor preprocessor;
        preprocessor.set_cache_enabled(false);
        preprocessor.set_file_reader(create_file_reader());

        size_t total = 0;
        for (const auto& shader_file : shader_files) {
            total += preprocessor.preprocess_file(shader_file).size();
        }
        return total;
    };
}
