![weBIGeo Sujet](https://github.com/weBIGeo/ressources/blob/main/for_md/webigeo_sujet_v3_github.png?raw=true)

# weBIGeo
WeBIGeo is a research project focused on real-time 3D computer graphics, large data visualization, and human-computer interaction. The goal is to offer a platform that is easy to use and allows for fast analysis and visualization of geographical datasets. (Further information can be found [here](https://www.netidee.at/webigeo).)

To that end, WeBIGeo uses existing technologies from the [AlpineMaps.org Project](https://alpinemapsorg.github.io/renderer/) and introduces a new WebGPU rendering engine.

We are in discord, talk to us!
https://discord.gg/p8T9XzVwRa

# Setup
weBIGeo's primary target is the web. Additionally we support native builds on Windows, using [Dawn](https://dawn.googlesource.com/). This allows for faster development as emscripten linking is quite slow and GPU debugging is not easily possible in the web. Both setups require significant setup time as we need to compile Qt by source. For the native workflow also Dawn needs to be compiled. The rest of the dependencies should get automatically pulled by the cmake setup.

<<<<<<< HEAD
## Building the web version
=======
After that it should be a normal cmake project. That is, you run cmake to generate a project or build file and then run your favourite tool. All dependencies should be pulled automatically while you run CMake. 
We use Qt Creator (with mingw on Windows), which is the only tested setup atm and makes setup of Android and WebAssembly builds reasonably easy. If you have questions, please go to Discord.
>>>>>>> main

### Dependencies
* Qt 6.6.2 with
  * MinGW (GCC on linux might work but not tested)
  * Qt Shader Tools (otherwise Qt configure fails)
  * Sources
* [emscripten](https://emscripten.org/docs/getting_started/downloads.html)
* To exactly follow along with build instructions you need `ninja`, `cmake` and `emsdk` in your PATH

### Building Qt with emscripten
For WebGPU to work we need to compile with emscripten version > 3.1.47. The default emscripten version, supported by Qt 6.6.2 is 3.1.37 (https://doc.qt.io/qt-6/wasm.html#installing-emscripten). Therefore we need to recompile the Qt sources with the appropriate version by ourselfs. (This step might be unnecessary in the near future with the release of [Qt 6.7](https://wiki.qt.io/Qt_6.7_Release)).

1. Open new CMD Terminal

2. Navigate to Qt Path:
   ```
   cd "C:\Qt\6.6.2"
   ```

3. Generate build and install path (and make sure they are empty)
   ```
   mkdir build & mkdir wasm_singlethread_emsdk_3_1_55_custom & cd build
   ```

4. Install and activate specific emsdk version
   ```
   emsdk install 3.1.55 & emsdk activate 3.1.55
   ```

5. Configure Qt with a minimal setup (takes ~ 4min)
   ```
   "../Src/configure" -debug-and-release -qt-host-path C:\Qt\6.6.2\mingw_64 -make libs -no-widgets -optimize-size -no-warnings-are-errors -platform wasm-emscripten -submodules qtdeclarative -no-dbus -no-sql-sqlite -feature-wasm-simd128 -no-feature-cssparser -no-feature-quick-treeview -no-feature-quick-pathview -no-feature-texthtmlparser -no-feature-textodfwriter -no-feature-quickcontrols2-windows -no-feature-quickcontrols2-macos -no-feature-quickcontrols2-ios -no-feature-quickcontrols2-imagine -no-feature-quickcontrols2-universal -no-feature-quickcontrols2-fusion -no-feature-qtwebengine-build -no-feature-qtprotobufgen -no-feature-qtpdf-build -no-feature-pdf -no-feature-printer -no-feature-sqlmodel -no-feature-qtpdf-quick-build -no-feature-quick-pixmap-cache-threaded-download -feature-quick-canvas -no-feature-quick-designer -no-feature-quick-particles -no-feature-quick-sprite -no-feature-raster-64bit -no-feature-raster-fp -prefix ../wasm_singlethread_emsdk_3_1_55_custom
   ```

6. Build Qt (takes ~ 8min)
   ```
   cmake --build . --parallel
   ```

7. Install Qt (takes ~ 1min)
   ```
   cmake --install .
   ```
   
8. Cleanup
   ```
   cd .. & rmdir /s /q build
   ```

### Create Custom Kit for Qt Creator
This step is specifically tailored to the Qt-Creator IDE.

1. `Preferences -> Devices -> WebAssembly`: Set path to the emsdk git repository
2. `Preferences -> Kits -> Qt Versions`: Add the newly built Qt-Version.
3. `Preferences -> Kits -> Kits`: Add the new kit which links to the created Qt-Version.

[![Emscripten Path Qt Creator](https://github.com/weBIGeo/ressources/blob/main/for_md/emscripten_path_qt_creator_thumb.jpg?raw=true)](https://github.com/weBIGeo/ressources/blob/main/for_md/emscripten_path_qt_creator.jpg?raw=true) [![Custom Qt Version for emsdk](https://github.com/weBIGeo/ressources/blob/main/for_md/custom_qt_version_thumb.jpg?raw=true)](https://github.com/weBIGeo/ressources/blob/main/for_md/custom_qt_version.jpg?raw=true) [![Custom Qt Kit for emsdk](https://github.com/weBIGeo/ressources/blob/main/for_md/custom_qt_kit_thumb.jpg?raw=true)](https://github.com/weBIGeo/ressources/blob/main/for_md/custom_qt_kit.jpg?raw=true)

## Building the native version

### Dependencies
* Windows
* Qt 6.6.2 with
  * Sources
  * Qt Shader Tools (otherwise Qt configure fails)
* Python
* Microsoft Visual C++ Compiler 17.6 (aka. MSVC2022) (comes with Visual Studio 2022)
* To exactly follow along with build instructions you need `ninja` and `cmake` in your PATH

> [!IMPORTANT]
> Dawn does not compile with MinGW! GCC might be possible, but is not tested (hence the Windows Requirement). If compiling on Linux is necessary it is also required to change the way we link to the precompiled Dawn libraries inside of `cmake/alp_target_add_dawn.cmake`.

### Building Qt for MSVC2022
There is no precompiled version of Qt for the MSVC2022-compiler. Therefore we again need to compile the Qt sources ourselves:

1. We need the compiler env variables, so choose either

   1. Start the `x64 Native Tools Command Prompt for VS 2022`.

   2. Start a new CMD and run: (you might have to adjust the link depending on your vs version)
      ```
      "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
      ```

2. Navigate to Qt Path:
   ```
   cd "C:\Qt\6.6.2"
   ```

3. Generate build and install path (and make sure they are empty)
   ```
   mkdir build & mkdir msvc2022_64_custom & cd build
   ```

4. Configure Qt (takes ~ 5min)
   ```
   "../Src/configure.bat" -debug-and-release -prefix ../msvc2022_64_custom -nomake examples -nomake tests
   ```


5. Build Qt (takes ~ 25min)
   ```
   cmake --build . --parallel
   ```

6. Install Qt (takes ~ 2min)
   ```
   ninja install
   ```
   
7. Cleanup
   ```
   cd .. & rmdir /s /q build
   ```

### Create Custom Kit for Qt Creator
If you work with the Qt-IDE you need to setup the newly created Qt build as a development kit, as it was already done with the emscripten version. Please see the screenshot for a proper configuration.

[![Qt Version MSVC2022 Screenshot](https://github.com/weBIGeo/ressources/blob/main/for_md/custom_qt_version_msvc2022_thumb.jpg?raw=true)](https://github.com/weBIGeo/ressources/blob/main/for_md/custom_qt_version_msvc2022.jpg?raw=true) [![Qt Kit MSVC2022 Screenshot](https://github.com/weBIGeo/ressources/blob/main/for_md/custom_qt_kit_msvc2022_2_thumb.jpg?raw=true)](https://github.com/weBIGeo/ressources/blob/main/for_md/custom_qt_kit_msvc2022_2.jpg?raw=true)

### Building Dawn
Dawn is the webgpu-implementation used in chromium, which is the open-source-engine for Google Chrome. Compiling dawn ourselves allows us to deploy weBIGeo natively such that we don't have to work in the browser sandbox during development. Building Dawn will take some time and memory as we need Debug and Release-Builds.

> [!NOTE]
> We choose the dawn version of branch `chromium/6246`, because it seems to be the one best aligned with the emscripten version in use. All of those versions are subject to change, especially since the webgpu-standard is not finalized! 

1. We need the compiler env variables, so choose either (or do manually :P)

   1. Start the `x64 Native Tools Command Prompt for VS 2022`.

   2. Start a new CMD and run: (you might have to adjust the link depending on your vs version)
      ```
      "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
      ```

2.  Navigate to the parent folder of where the weBIGeo renderer is located. (Not strictly necessary as DAWN location can be set with CMAKE-Variable `ALP_DAWN_DIR`)
    ```
    cd "/path/to/the/parent/directory/of/webigeo/renderer"
    ```

3.  Clone dawn and step into directory
    ```
    git clone --branch chromium/6246 --depth 1 https://dawn.googlesource.com/dawn & cd dawn
    ```

4.  Fetch dawn dependencies
    ```
    python tools/fetch_dawn_dependencies.py
    ```

5.  Create build directories
    ```
    mkdir build\release & mkdir build\debug
    ```

6.  Debug Build (normal build, without add. features):
    ```
    cd build/debug & cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DTINT_BUILD_SPV_READER=OFF -DDAWN_BUILD_SAMPLES=OFF -DTINT_BUILD_TESTS=OFF -DTINT_BUILD_FUZZERS=OFF -DTINT_BUILD_SPIRV_TOOLS_FUZZER=OFF -DTINT_BUILD_AST_FUZZER=OFF -DTINT_BUILD_REGEX_FUZZER=OFF -DTINT_BUILD_BENCHMARKS=OFF -DTINT_BUILD_TESTS=OFF -DTINT_BUILD_AS_OTHER_OS=OFF ../.. & ninja
    ```

7.  Release Build (normal build, without add. features):
    ```
    cd ../release & cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DTINT_BUILD_SPV_READER=OFF -DDAWN_BUILD_SAMPLES=OFF -DTINT_BUILD_TESTS=OFF -DTINT_BUILD_FUZZERS=OFF -DTINT_BUILD_SPIRV_TOOLS_FUZZER=OFF -DTINT_BUILD_AST_FUZZER=OFF -DTINT_BUILD_REGEX_FUZZER=OFF -DTINT_BUILD_BENCHMARKS=OFF -DTINT_BUILD_TESTS=OFF -DTINT_BUILD_AS_OTHER_OS=OFF ../.. & ninja
    ```
	
8. Cleanup (Optional)
   To cleanup unnecessary files, like the DAWN sources aswell as configuration files we created a python script. It frees up around 3GB of files. Maybe DAWN will introduce an installation target at some point then this shouldnt be necessary. You can get the [script either here](https://github.com/weBIGeo/ressources/raw/main/scripts/cleanup_dawn_build.py), or just download and execute it with the following command:
   ```
   cd ../.. & curl -L "https://github.com/weBIGeo/ressources/raw/main/scripts/cleanup_dawn_build.py" -o cleanup_dawn_build.py && python cleanup_dawn_build.py
   ```


# Code style
We adhere to the coding guidelines of [AlpineMaps.org](https://github.com/AlpineMapsOrg/renderer?tab=readme-ov-file#code-style).
