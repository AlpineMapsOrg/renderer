# <img src="../apps/webgpu_app/shell/webigeo_logo.svg" width="40" height="40" align="left" style="margin-right:8px"/> weBIGeo (webgpu_app)

[![Discord](https://img.shields.io/badge/discord-join-5865F2?logo=discord&logoColor=white)](https://discord.gg/p8T9XzVwRa) [![Demo](https://img.shields.io/badge/demo-live-brightgreen)](https://webigeo.alpinemaps.org/)

`webgpu_app` (branded as **weBIGeo**) is a research application built on the AlpineMaps.org rendering infrastructure, focused on real-time 3D computer graphics, large data visualization, and human-computer interaction over geographical datasets. Further information: [netidee.at/webigeo](https://www.netidee.at/webigeo).

## [Developer Guide](webgpu_app_dev.md)
For an overview of the internal architecture, see the [Developer Guide](webgpu_app_dev.md).

## Setup

weBIGeo can be deployed to the web via emscripten and additionally we support native builds on Windows, using [Dawn](https://dawn.googlesource.com/) and [SDL2](https://github.com/libsdl-org/SDL/tree/SDL2).

### CMake Presets
| Preset | Description |
|--------|-------------|
| **webgpu_app_msvc_debug** | MSVC Debug build for Windows (native) |
| **webgpu_app_msvc_release** | MSVC Release build for Windows (native) |
| **unittests_msvc_debug** | MSVC Debug build for unit tests |
| **unittests_msvc_release** | MSVC Release build for unit tests |
| **webgpu_app_wasm_debug** | WebAssembly Debug build |
| **webgpu_app_wasm_release** | WebAssembly Release build |
| **webgpu_app_wasm_publish** | WebAssembly Production build with minified shaders and no debug output |

### Building the web version

#### Dependencies
* Qt 6.10.1 with
  * WebAssembly (multi-threaded) pre-built binaries
* Python 3
* cmake and ninja (come with Qt)
* emsdk 4.0.7 ([emscripten](https://emscripten.org/docs/getting_started/downloads.html))
   ```
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk
   emsdk install 4.0.7
   emsdk activate 4.0.7
   ```

#### Configuration
> [!IMPORTANT]
> If you're using Qt Creator, you can simply use the default kit *WebAssembly Qt 6.10.1 (multi-threaded)* - no additional setup is needed.

The WASM `toolchainFile` **is hardcoded** in `webgpu_wasm_base` (currently `C:/Qt/6.10.1/wasm_multithread/lib/cmake/Qt6/qt.toolchain.cmake`) -> edit it directly if your Qt install differs.

If you're using **VS Code** with the CMake Tools extension, the remaining variables (`Qt6_DIR`, `PATH`, `EMSDK`) can go in `.vscode/settings.json` (gitignored) under `cmake.environment` and will be injected automatically every time CMake Tools configures -> no manual step needed:
```jsonc
{
  "cmake.environment": {
    "Qt6_DIR": "C:/Qt/6.10.1/msvc2022_64/lib/cmake/Qt6",
    "PATH": "C:/Qt/Tools/Ninja;${env:PATH}",
    "EMSDK": "C:/tmp/webigeo/emsdk"
  }
}
```

#### Serving the WASM Build
After building, you can use the `serve_wasm.py` script to serve the build files for the WebAssembly build locally. This script sets up a local server with the correct headers required for WebAssembly.

### Building the native version

#### Dependencies
* Windows
* Qt 6.10.1 with
  * MSVC2022 pre-built binaries
* Python 3
* Microsoft Visual C++ Compiler 17.13 or later (aka. MSVC2022, comes with Visual Studio 2022 - keep VS up to date as the prebuilt Dawn library may require a recent toolset version)
* cmake and ninja (come with Qt)

#### Configuration
> [!IMPORTANT]
> If you're using Qt Creator, you can simply use the default kit *Desktop Qt 6.10.1 MSVC2022 (64-bit)* - no additional setup is needed.

If you use the Cmake Presets make sure that **Qt6_DIR** points at your actual Qt installation's CMake directory (e.g., `C:/Qt/6.10.1/msvc2022_64/lib/cmake/Qt6`).

#### Troubleshoot
- **`LNK2019: unresolved external symbol __std_find_first_of_trivial_pos_1`**: The prebuilt Dawn library was compiled with a newer MSVC toolset than what is installed. **Solution: update Visual Studio 2022 to the latest version** via the Visual Studio Installer.
- **MY CONFIGURATION TAKES FOREVER (first run)**: On first CMake configuration, SDL is cloned and built from source (~5–15 min). Dawn is normally downloaded as a prebuilt binary from GitHub releases (fast), but if the download fails it falls back to building Dawn from source, which can take an additional 10–40 min.
- Dawn and SDL setup is driven by Python scripts in `misc/scripts/` which are invoked automatically by CMake during configuration. To force a re-fetch (e.g. after a version bump), delete the relevant subdirectory under `extern/` and reconfigure.
- **`CMake Error at CMakeLists.txt:111 (find_package): By not providing "FindQt6.cmake" ...`**: Qt6 couldn't be located. Make sure `Qt6_DIR` is set (see Configuration above, for both the native and WASM builds).
- **`CMake Error: CMake was unable to find a build program corresponding to "Ninja"`**: Make sure Ninja is in your `PATH`.
- **`Could not find toolchain file: .`**: Make sure the `toolchainFile` path in `CMakePresets.json` points to the proper location for your Qt WASM install.
- **`The toolchain file to be chainloaded '/opt/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake' does not exist`**: Make sure `EMSDK` is set and points at your emsdk installation directory.

#### About DAWN Backends
In the normal flow, a prebuilt Dawn binary is downloaded during CMake configuration and includes multiple backends. If the download fails and Dawn is built from source via `misc/scripts/install_dawn.py`, only the Vulkan backend is enabled -> you can change this by modifying the CMake flags in that script.

### Install Targets
Install targets are now available for both web and native builds. These targets install all necessary files into the install directory, making it easy to deploy or distribute the built application.

To use the install target:
```bash
cmake --build build/<preset-name> --target install
```

The install directory is automatically configured in the CMake presets and will be located at:
- Native builds: `install/webgpu_app_msvc_debug` or `install/webgpu_app_msvc_release`
- Web builds: `install/webgpu_app_wasm_debug`, `install/webgpu_app_wasm_release`, or `install/webgpu_app_wasm_publish`

### Tested Coding Environments
The following development environments have been tested and are known to work with this project:

- **Qt Creator 18** [recommended]
  - With Qt Creator we recommend using the default Kits (see above)
- **Visual Studio Code** with the following extensions:
  - [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
  - [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
  - [WGSL](https://marketplace.visualstudio.com/items?itemName=PolyMeilex.wgsl)
- **Visual Studio 2022 Community**
