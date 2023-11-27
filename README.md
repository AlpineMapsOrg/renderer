# alpine-renderer
A developer version is available at [alpinemaps.org](https://alpinemaps.org), and an apk for android under https://alpinemaps.org/apk. Be aware that it can break at any time!

We are in discord, talk to us!
https://discord.gg/p8T9XzVwRa

# Cloning and building
`git clone git@github.com:AlpineMapsOrg/renderer.git`

After that it should be a normal cmake project. That is, you run cmake to generate a project or build file and then run your favourite tool. All dependencies should be pulled automatically into `renderer/external` while you run CMake. 
We use Qt Creator (with mingw on Windows), which is the only tested setup atm and makes setup of Android and WebAssembly builds reasonably easy. If you have questions, please open a new [discussion](https://github.com/AlpineMapsOrg/renderer/discussions).

## Dependencies
* Qt 6.6.0, or greater
* OpenGL
* Qt Positioning and Charts modules
* Some other dependencies will be pulled automatically during building.

## Building the android version
* Due to a [bug](https://bugreports.qt.io/browse/QTBUG-113851) in the Qt/cmake/gradle build system for android, you need to delete all `libc.so` files from the build dir before rebuilding (yes! no, that's not a joke). There is a script that does that in linux [renderer/android/workaround_qt_cmake_build_bug.sh](https://github.com/AlpineMapsOrg/renderer/blob/main/android/workaround_qt_cmake_build_bug.sh), please don't run it anywhere important, definitely not as root;) ). You can add it as a custom build step before everything else in qt creator (in the %{buildDir} working directory).

## Building the WebAssembly version:
* Atm, none of the Qt versions works perfectly in all browsers
* In Qt 6.6 touch doesn't work on Firefox (issues #33)
* [WebAssembly version compatible with the Qt version](https://doc-snapshots.qt.io/qt6-dev/wasm.html#installing-emscripten)
* The threaded version doesn't seem to work atm, so use the non-threaded!
* There are a number of other bugs, we track them with the upstream tag.

# Code style
* class names are CamelCase, method, function and variable names are snake_case.
* class attributes have an m_ prefix and are usually private, struct attributes don't and are usually public.
* use `void set_attribute(int value)` and `int attribute() const` for setters and getters (that is, avoid the get_). Use [the Qt recommendations](https://wiki.qt.io/API_Design_Principles#Naming_Boolean_Getters,_Setters,_and_Properties) for naming boolean getters.
* structs are usually small, simple, and have no or only few methods. they never have inheritance.
* files are CamelCase if the content is a CamelCase class. otherwise they are snake_case, and have a snake_case namespace with stuff.
* the folder/structure.h is reflected in namespace folder::structure{ .. }
* indent with space only, indent 4 spaces
* ideally, use the clang-format file provided with the project
  (in case you use Qt Creator, go to Preferences -> C++ -> Code Style: Formatting mode: Full, Format while typing, Format edited code on file save, don't override formatting)
* follow the [Qt recommendations](https://wiki.qt.io/API_Design_Principles) and the [c++ core guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines) everywhere else.

