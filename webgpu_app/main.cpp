
#include <GLFW/glfw3.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#endif

#include <GLFW/glfw3.h>
#include <QCoreApplication>
#include "TerrainRenderer.h"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    // Init QCoreApplication seems to be necessary as it declares the current thread
    // as a Qt-Thread. Otherwise functionalities like QTimers wouldnt work.
    // It basically initializes the Qt environment
    QCoreApplication app(argc, argv);

    TerrainRenderer renderer;
    renderer.start();
    return 0;
}
