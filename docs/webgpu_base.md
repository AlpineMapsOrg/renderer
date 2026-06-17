# webgpu_base

`webgpu/base/` is the foundational library shared by `webgpu_engine` and `webgpu_compute`. It provides:

- **RAII wrappers** for all raw WebGPU handles (`WGPUTexture`, `WGPUBuffer`, `WGPURenderPipeline`, …)
- A **custom shader preprocessor** for WGSL files with file inclusion and conditional compilation
- A **GPU resource registry** that owns shaders, bind group layouts, and pipeline constructors and can recreate them all in dependency order
- A lightweight **timing infrastructure** for CPU and GPU performance profiling
- A `webgpu::Context` object to bundle all raw WebGPU handles together with the `RenderResourceRegistry` and the appropriate `ShaderPreprocessor`

```mermaid
graph LR
    Ctx("webgpu::Context")
    Reg("RenderResourceRegistry")
    Pre("ShaderPreprocessor")


    Ctx --> Reg
    Reg --> Pre
```

*`Context` owns the `RenderResourceRegistry`, which in turn owns the `ShaderPreprocessor`.*

---

## Shader Preprocessor

**Class:** `webgpu::util::ShaderPreprocessor`  
**Files:** [webgpu/base/util/ShaderPreprocessor.h](../webgpu/base/util/ShaderPreprocessor.h), [ShaderPreprocessor.cpp](../webgpu/base/util/ShaderPreprocessor.cpp)

### Design principle

All preprocessor directives use a `///` comment prefix so that `.wgsl` files remain syntactically valid WGSL. Tooling (formatters, syntax highlighters) sees them as ordinary line comments; the preprocessor intercepts them before the GPU compiler.

### Directives

| Directive | Effect |
|-----------|--------|
| `///use relpath` | Include another shader file (same namespace) |
| `///use target::relpath` | Include a shader from a different namespace |
| `///define SYMBOL` | Define a symbol (value defaults to `"1"`) |
| `///define SYMBOL value` | Define a symbol with an explicit string value |
| `///ifdef SYMBOL` / `///ifndef SYMBOL` | Conditional block on presence of a symbol |
| `///if SYMBOL value` / `///elif SYMBOL value` | Conditional block on symbol's value |
| `///else` / `///endif` | Close a conditional block |

> [!Important]
Each file is included at most once per top-level call (`pragma-once` behaviour).

### Namespace-based file resolution

Shader files are identified by a logical name in the form `namespace::relative/path` (without `.wgsl`). The preprocessor resolves these names to physical files using a two-level lookup:

1. **Local filesystem path**:
If `set_local_shader_path("webgpu_engine", "/path/to/shaders")` has been called for the target namespace, the file is read from disk. This enables hot-reload during development.
2. **Qt resource (QRC) fallback**:
If no local path is configured, the file is read from the embedded resource `:/shaders/namespace/relative/path.wgsl`.

A `///use` directive without an explicit namespace inherits the namespace of the file that contains it:

e.g. inside `webgpu_engine::tile_mesh/render_tiles.wgsl`:
 - `///use util/shared_config`->     `webgpu/engine/shaders/util/shared_config.wgsl`
 - `///use webgpu::hashing` ->   `webgpu/base/shaders/hasing.wgsl`

This means shader libraries can be structured into per-module namespaces (`webgpu`, `webgpu_engine`, `webgpu_compute`) and reference each other without hardcoding absolute paths.

### Platform defines

The constructor automatically defines symbols for the current build environment, so shaders can use `///ifdef __EMSCRIPTEN__` or `///ifdef _WIN32` the same way C++ code uses `#ifdef`.


---

## GPU Resource Registry

**Class:** `webgpu::RenderResourceRegistry`  
**Files:** [webgpu/base/RenderResourceRegistry.h](../webgpu/base/RenderResourceRegistry.h), [RenderResourceRegistry.cpp](../webgpu/base/RenderResourceRegistry.cpp)

### Responsibilities

The registry centralises a few GPU resources that need to be recreated together, for example on device loss or when shaders are hot-reloaded. It tracks three resource categories:

| Category | Storage |
|----------|---------|
| Shader modules | Preprocessed + compiled WGSL |
| Bind group layouts | Factory-built `WGPUBindGroupLayout` |
| Pipeline constructors | Callbacks that build pipelines |

> [!WARNING]
> Pipelines are **not** stored inside the registry. The callback owns the pipeline object and stores it in the caller (typically a Renderer). This avoids the registry needing to know about the different concrete pipeline types, and additionally saves us an additional mapping access when we want to use a pipeline.

### Recreation order

`recreate_all` always recreates resources in dependency order:

```mermaid
graph LR
    S["1. Shaders\n(preprocess + compile)"]
    L["2. Bind Group Layouts\n(factory callbacks)"]
    P["3. Pipelines\n(pipeline callbacks)"]

    S --> L --> P
```

### Inline shader compilation

For one-off shaders that don't need to be reloaded, `compile_shader_from_code()` runs the preprocessor on raw WGSL code and returns a ready-to-use `ShaderModule` without registering it:

```cpp
auto module = reg.compile_shader_from_code(device, wgslSource, "my_inline_shader");
```