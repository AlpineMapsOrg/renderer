# Setup
weBIGeo can be deployed to the web via emscripten and additionally we support native builds on Windows, using [Dawn](https://dawn.googlesource.com/) and [SDL2](https://github.com/libsdl-org/SDL/tree/SDL2).

## CMake Presets
| Preset | Description |
|--------|-------------|
| **msvc-debug** | MSVC Debug build for Windows (native) |
| **msvc-release** | MSVC Release build for Windows (native) |
| **wasm-debug** | WebAssembly Debug build |
| **wasm-release** | WebAssembly Release build |
| **wasm-publish** | WebAssembly Production build with minified shaders and no debug output |

## Building the web version

### Dependencies
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

### Configuration
> [!IMPORTANT]
> If you’re using Qt Creator, you can simply use the default kit *WebAssembly Qt 6.10.1 (multi-threaded)* - no additional setup is needed.

Before building, you need to ensure the following paths are correctly configured in [CMakePresets.json](../CMakePresets.json):

1. **Ninja**: Make sure Ninja is in your system PATH, OR update the `PATH` environment variable in the `emscripten-base` preset to point to your Ninja installation (e.g., `C:/Qt/Tools/Ninja`).

2. **EMSDK**: The `EMSDK` environment variable in the `emscripten-base` preset must point to your emsdk installation directory (e.g., `C:/tmp/webigeo/emsdk`).

3. **Toolchain file**: The `toolchainFile` path in the `emscripten-base` preset might need to be adapted depending on where Qt is installed (e.g., `C:/Qt/6.10.1/wasm_multithread/lib/cmake/Qt6/qt.toolchain.cmake`).

### Serving the WASM Build
After building, you can use the `serve_wasm.py` script to serve the build files for the WebAssembly build locally. This script sets up a local server with the correct headers required for WebAssembly.


## Building the native version

### Dependencies
* Windows
* Qt 6.10.1 with
  * MSVC2022 pre-built binaries
* Python 3
* Microsoft Visual C++ Compiler 17.6 (aka. MSVC2022, comes with Visual Studio 2022)
* cmake and ninja (come with Qt)

### Configuration
> [!IMPORTANT]
> If you’re using Qt Creator, you can simply use the default kit *Desktop Qt 6.10.1 MSVC2022 (64-bit)* - no additional setup is needed.

Before building, you need to ensure the following paths are correctly configured in [CMakePresets.json](../CMakePresets.json):

1. **Qt6_DIR**: Verify that the `Qt6_DIR` in the `msvc-base` preset points to your actual Qt installation's CMake directory (e.g., `C:/Qt/6.10.1/msvc2022_64/lib/cmake/Qt6`).

### Troubleshoot
- MY CONFIGURATION TAKES FOREVER: Upon first cmake configuration DAWN as well as SDL is being pulled, build and installed. This might take a while. (~10-40 min)
- Dawn and SDL installation as well as fetching the custom dawn port for emscripten now happens in the python scripts inside the respective folder. They get executed by the CMAKE-Setup. A change requires a reconfiguration of CMAKE as well as the deletion of the directory in the `extern` directory.
- If you have issues with your currently installed Vulkan SDK you may try one or all of the following:
  - disable `DDAWN_FORCE_SYSTEM_COMPONENT_LOAD`
  - Try with a different DAWN backend
  - copying the include files from your sdk into the respective folders in the dawn binaries `dawn\third_party\vulkan-utility-libraries\src\include\vulkan\utility\` and `dawn\third_party\vulkan-headers\src\include\vulkan\` and rebuild dawn.

### About DAWN Backends
Per default we opt for an only Vulkan-Backend Build for two reasons:
- Vulkan is probably the most supported Backend running on most devices
- We have more knowledge about Vulkan which comes to play when we use GPU debuggers

That being said you may enable different Backends in the `install_dawn.py` script.

## Install Targets
Install targets are now available for both web and native builds. These targets install all necessary files into the install directory, making it easy to deploy or distribute the built application.

To use the install target:
```bash
cmake --build build/<preset-name> --target install
```

The install directory is automatically configured in the CMake presets and will be located at:
- Native builds: `install/msvc-debug` or `install/msvc-release`
- Web builds: `install/wasm-debug`, `install/wasm-release`, or `install/wasm-publish`

## Tested Coding Environments
The following development environments have been tested and are known to work with this project:

- **Qt Creator 18** [recommended]
  - With Qt Creator we recommend using the default Kits (see above)
- **Visual Studio Code** with the following extensions:
  - [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
  - [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
  - [WGSL](https://marketplace.visualstudio.com/items?itemName=PolyMeilex.wgsl)
- **Visual Studio 2022 Community**