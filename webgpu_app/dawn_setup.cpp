#include "dawn_setup.h"

//#include <webgpu/webgpu_cpp.h>
//#include <webgpu/webgpu_glfw.h>
#include <dawn/native/DawnNative.h>
#include <dawn/dawn_proc.h>

void setup_dawn_proctable() {
    // Dawn forwards all wgpu* function calls to function pointer members of some struct.
    // This is stored in some (Dawn internal) variable. In our current setup, all these
    // function pointers default to nullptr, resulting in access violations when called.
    // However, we can just set these pointers explicitly to use dawn_native.
    dawnProcSetProcs(&(dawn::native::GetProcs()));
}
