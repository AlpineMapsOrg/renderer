# CMake & C++ builds, explained for a webdev/Java brain

You already know the *shape* of this problem from `package.json` + a bundler, or
`pom.xml`/`build.gradle` + Maven/Gradle. C++ has the same shape, but split into more
layers because there's no runtime (no JVM, no V8) to paper over platform differences.
Every layer below exists because "compile this code so it runs" is a much bigger
question in C++ than `npm run build`.

## The core difference: there is no runtime

- **JS**: ships source (or a bundle) to a JS engine (V8, etc.) that interprets/JITs it.
- **Java**: `javac` compiles to bytecode; the *same* `.class`/`.jar` runs on any JVM.
- **C++**: the compiler produces actual machine code for one specific
  (CPU architecture × operating system × ABI). A binary built for macOS arm64 will
  not run on Windows x64, or even on macOS x64, without rebuilding from source.

That's the whole reason this repo needs separate presets per platform
(`webgpu_app_msvc_debug` for Windows, `webgpu_app_macos_debug` for your Mac,
`webgpu_app_wasm_debug` for the browser) — each one points at a *different compiler*
producing a *different kind* of output.

## CMake is a build-file generator, not a build tool

This is the single most confusing thing coming from `npm`/Gradle, so it's worth being
precise:

- `CMakeLists.txt` files declare *what* to build (targets, sources, dependencies) —
  conceptually close to `build.gradle`/`pom.xml`.
- Running `cmake` ("**configure**") reads those files and *generates* actual build
  scripts for some other tool — Ninja files, Makefiles, an Xcode project, a Visual
  Studio solution. CMake itself never compiles anything.
- Running the generated tool ("**build**", e.g. `ninja` or `cmake --build`) is what
  actually invokes the compiler, in parallel, in dependency order. This step is the
  rough equivalent of `gradle build` actually running `javac`, or webpack actually
  running its transforms — the part that produces artifacts.

So every CMake workflow is always two phases: **configure** (decide *how*, write the
recipe) then **build** (follow the recipe). You did both today: `cmake --preset ...`
(configure) then `cmake --build build/...` (build) — Rider's CMake panel just does
both for you behind one "Build" button.

## CMakePresets.json ~ npm scripts / Gradle profiles, bundled

Configuring by hand means passing a pile of `-D FOO=BAR` flags every time (which
compiler, where Qt lives, which optional features are on). `CMakePresets.json` is
just a named, checked-in bundle of those flags — like a `package.json` "scripts"
entry, or a Gradle build variant. `webgpu_app_macos_debug` is shorthand for "use
Ninja, use this Qt install, build in Debug mode, build the webgpu app but not the
old GL/QML app."

Presets can `inherit` from each other (`webgpu_app_macos_debug` inherits shared
defaults from `webgpu_macos_base`, which inherits from `webgpu_base`) — same idea as
extending a base tsconfig or a Gradle convention plugin.

## The cache is a resolved-config snapshot, not a live view of the presets

Every configure writes `build/<preset>/CMakeCache.txt` — the *actual resolved*
settings for that build directory (which compiler was found, all the flag values).
Once it's written, CMake does **not** re-derive it from `CMakePresets.json` on every
build — it trusts the cache. This is why, when the wasm preset's toolchain path was
wrong, just fixing `CMakePresets.json` wasn't enough; the already-configured
`build/webgpu_app_wasm_debug` directory had the *old* wrong value baked into its
cache and kept using it until we deleted that directory and reconfigured from
scratch. Closest analogy: a lockfile that's gone stale relative to `package.json` —
you have to regenerate it, editing the manifest alone doesn't retroactively fix an
already-resolved install.

## Targets: one CMakeLists tree, many independent outputs

A "target" (`add_executable(webgpu_app ...)`, `add_library(webgpu ...)`) is one
buildable/runnable thing — closest to a Gradle module or a package in an npm
monorepo. This repo's build graph produces *several* targets from the same source
tree: `webgpu_app` (the actual application), `unittests_radix` (a test binary),
`webgpu` (an internal static library used by both), etc. That's why Rider needed you
to pick a target explicitly — "which preset" (how to compile) and "which target"
(what to actually run) are two independent choices, and the IDE defaulted to the
wrong target.

`target_link_libraries(app PUBLIC some_lib)` declares a dependency between targets —
same job as `implementation project(':some_lib')` in Gradle or an npm workspace
dependency, except linking is a real, low-level step (see next section), not just
"put it on the classpath/require path."

## Static vs. dynamic linking (why C++ "dependencies" feel heavier)

- **Static library** (`.a`): its compiled code gets copied directly into the final
  executable at link time. One self-contained binary, bigger file, no separate
  runtime dependency. Closest JS analogy: bundling everything into one `.js` file.
- **Shared/dynamic library** (`.dylib`/`.so`/`.dll`): stays a separate file, loaded
  by the OS when the program starts. Smaller binary, but that file has to be present
  on the target machine at the right version. Closest analogy: a `<script src>` to a
  CDN, or an OS-level `.jar` you assume is already installed rather than bundled.

Qt itself ships as shared libraries — that's why the build needs `CMAKE_PREFIX_PATH`
pointing at your actual Qt install (`~/Qt/6.11.1/macos`), so both the compiler (to
find headers) and the linker (to find the `.dylib`s) know where to look. In Java
terms this is like telling the compiler *and* the classloader where a
non-Maven-Central jar lives.

## `extern/` — vendoring instead of a package manager

There's no `npm install`/Maven-Central equivalent wired in here. Instead,
`cmake/AddRepo.cmake`'s `alp_add_git_repository()` clones each third-party dependency
(SDL, imgui, Dawn's WebGPU port, etc.) into `extern/<name>` at *configure* time,
pinned to an exact commit SHA — not a version range, not a registry. Once a repo is
already checked out at the pinned commit, configure skips touching it again (that's
why the goofy_tc header patch survives reconfigures — it's a real git checkout on
disk, not something re-materialized from a lockfile each time). Trade-off versus
npm/Maven: fully reproducible and no registry dependency, but no automatic version
resolution and no dependency-of-a-dependency graph — every dep is added by hand.

## Toolchain files — how the WASM build differs from "just compiling"

For native builds (macOS, Windows), the compiler on your machine directly produces
code for your machine — configure just needs to *find* that compiler.

For WASM, you're **cross-compiling**: producing WebAssembly on a machine that can't
natively run WebAssembly. That needs an entirely different compiler
(`em++`, from the Emscripten SDK) plus a matching set of system headers/libs built
for that target. A CMake **toolchain file** is a small script that tells CMake "use
this compiler instead, and here's where its headers/libs live" — you pass it once at
configure time (`-DCMAKE_TOOLCHAIN_FILE=...` or, as here, Qt's `qt-cmake` wrapper
injects it for you). There's no Java equivalent because the JVM abstracts this whole
problem away; the nearest cross-platform-JS analogy is something like compiling a
native Node addon for a different Node ABI — same idea, rarer in practice.

## Qt's MOC/autogen step ~ an annotation processor

Qt's signal/slot system relies on macros like `Q_OBJECT` that plain C++ can't act on
by itself. Qt ships a code generator, **moc** (meta-object compiler), that scans your
headers for these macros and emits extra `.cpp` files with the real implementation —
that's what you see as `webgpu_app_autogen/` in the build directory. CMake's
`AUTOMOC` setting wires this generator into the build automatically. Closest Java
analogy: an annotation processor (Lombok, Dagger) that generates extra source at
build time that you never hand-write yourself.

## Debug vs. Release ~ NODE_ENV / Gradle build variants

- **Debug**: no optimizations, debug symbols included, asserts compiled in. Slow but
  inspectable — this is what you want for development and what the debugger needs to
  show you real variable values and hit breakpoints reliably.
- **Release**: optimized, smaller, faster, much harder to debug (the compiler
  reorders/inlines/removes code, so line-by-line stepping gets unreliable).

`CMAKE_BUILD_TYPE=Debug`/`Release` is the flag that picks this, same job as
`NODE_ENV=production` or a Gradle `debug`/`release` variant.

## Where an IDE (Rider/CLion/VS Code+CMake Tools) fits in

The IDE isn't a separate build system — it's a UI wrapped around exactly the two
phases above: it calls `cmake --preset <x>` (configure) when you sync, calls the
generator's build tool (build) when you hit Build, and for Run/Debug it just launches
whichever target binary you selected, optionally under `lldb`/`gdb` so breakpoints
work. The "target" dropdown you were missing earlier is the IDE asking "which of the
many binaries this CMakeLists tree can produce do you want me to launch" — nothing
to do with which preset/platform you're building for.
