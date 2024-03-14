#include <emscripten/bind.h>
#include <QObject>


// The WebInterop class acts as bridge between the C++ code and the JavaScript code.
// It maps the exposed Javascript functions to signals on the singleton which can be used in our QObjects.
class WebInterop : public QObject {
    Q_OBJECT

public:
    // Deleted copy constructor and copy assignment operator
    WebInterop(const WebInterop&) = delete;
    WebInterop& operator=(const WebInterop&) = delete;

    // Static method to get the instance of the class
    static WebInterop& instance() {
        static WebInterop _instance;
        return _instance;
    }

    static void _canvas_size_changed(int width, int height);

signals:
    void canvas_size_changed(int width, int height);

private:
    // Private constructor
    WebInterop() {}
};

