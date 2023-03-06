# alpine-renderer
A developer version is available at https://gataki.cg.tuwien.ac.at/. Be aware that it can break at any time.

We are in discord, talk to us!
https://discord.gg/p8T9XzVwRa

# cloning
`git clone --recurse-submodules git@github.com:AlpineMapsOrg/renderer.git`

or a normal clone and

`git submodule init && git submodule update`

# dependencies for the native dev build
* Qt 6.4.1 (!), or greater
* OpenGL
* Catch2 (version 2.x, not 3.x!) and GLM will be pulled as git submodules.
* Qt Positioning and qt5 compatibility modules

# for building the WebAssembly version:
* Qt 6.4.1 (!), or greater
* WebAssembly version compatible with the Qt version (https://doc-snapshots.qt.io/qt6-dev/wasm.html#installing-emscripten)
* For compiling the threaded version, you need your own build of Qt (Instructions: https://doc-snapshots.qt.io/qt6-dev/wasm.html#building-qt-from-source, my compile commands were `mkdir my_threaded_qt && cd my_threaded_qt`, `../Src/configure -qt-host-path ../gcc_64 -platform wasm-emscripten -submodules qtbase -no-dbus -prefix $PWD/qtbase -feature-thread -opengles3 -c++std 20 -static -optimize-size` and `cmake --build . -t qtbase`; after which I added this build to the kits in Qt Creator)

# code style
* class names are CamelCase, method and variable names are snake_case.
* class attributes have an m_ prefix and are usually private, struct attributes don't and are usually public.
* structs are usually small, simple, and have no or only few methods. they never have inheritance.
* files are CamelCase if the content is a CamelCase class. otherwise they are snake_case, and have a snake_case namespace with stuff.
* the folder/structure.h is reflected in namespace folder::structure{ .. }
* indent with space only, indent 4 spaces
* ideally, use clang-format with the WebKit style  
  (in case you use Qt Creator, go to Preferences -> C++ -> Code Style: Formatting mode: Full, Format while typing, Format edited code on file save, Clang-Format Style -> BasedOnStyle=WebKit)

