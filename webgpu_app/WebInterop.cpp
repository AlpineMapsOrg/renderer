#include "WebInterop.h"

void WebInterop::_canvas_size_changed(int width, int height) {
    emit instance().canvas_size_changed(width, height);
}

// Emscripten binding
EMSCRIPTEN_BINDINGS(webinterop_module) {
    emscripten::function("canvas_size_changed", &WebInterop::_canvas_size_changed);
}
