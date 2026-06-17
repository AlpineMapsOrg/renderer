# <img src="../app/icons/icon.png" width="40" height="40" align="left" style="margin-right:8px"/> AlpineMaps (app)

This is the software behind [alpinemaps.org](https://alpinemaps.org). It uses `gl_engine` for rendering and Qt Quick (QML) for the UI.

A developer version (trunk) is released [here](https://alpinemapsorg.github.io/renderer/), including APKs for android. Be aware that it can break at any time!

[If looking at the issues, best to filter out projects!](https://github.com/AlpineMapsOrg/renderer/issues?q=is%3Aissue%20state%3Aopen%20no%3Aproject)

## Dependencies

* Qt 6.11.1, or greater
* g++ 12+, clang or msvc
* OpenGL
* Qt Positioning and Charts modules
* Some other dependencies will be pulled automatically during building.

## Cloning

```sh
git clone git@github.com:AlpineMapsOrg/renderer.git
```

After that it should be a normal cmake project. That is, you run cmake to generate a project or build file and then run your favourite tool. All dependencies should be pulled automatically while you run CMake.
We use Qt Creator (with mingw on Windows), which is the only tested setup atm and makes setup of Android and WebAssembly builds reasonably easy. If you have questions, please go to Discord.

## Building the native version

Just run cmake and build.

## Building the android version

See also: [Creating APK signing keys](app_creating_apk_keys.md)

* We are usually building with Qt Creator, because it works relatively out of the box. However, it should also work on the command line or other IDEs if you set it up correctly.
* You need a Java JDK before you can do anything else. Not all Java versions work, and the error messages might be surprising (or non-existant). I'm running with Java 19, and I can compile for old devices. Iirc a newer version of Java caused issues. [Android documents the required Java version](https://developer.android.com/build/jdks), but as said, for me Java 19 works as well. It might change in the future.
* Once you have Java, go to Qt Creator Preferences -> Devices -> Android. There click "Set Up SDK" to automatically download and install an Android SDK.
* Finally, you might need to click on SDK Manager to install a fitting SDK Platform (take the newest, it also works for older devices), and ndk (newest as well).
* Then Google the internet to find out how to enable the developer mode on Android.
* On linux, you'll have to setup some udev rules. Run `Android/SDK/platform-tools/adb devices` and you should get instructions.
* If there are problems, check out the [documentation from Qt](https://doc.qt.io/qt-6/android-getting-started.html)
* Finally, you are welcome to ask in discord if something is not working!

## Building the WebAssembly version

* [The Qt documentation is quite good on how to get it to run](https://doc-snapshots.qt.io/qt6-dev/wasm.html#installing-emscripten).
* Be aware that only specific versions of emscripten work for specific versions of Qt, and the error messages are not helpfull.
* [More info on building and getting Hotreload to work](https://github.com/AlpineMapsOrg/documentation/blob/main/WebAssembly_local_build.md)
