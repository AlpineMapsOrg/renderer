# webgpu_app - Developer Guide

## Application structure

`apps/webgpu_app/main.cpp` creates the `webgpu_app::App`, which owns the SDL window, the WebGPU device, and the render loop (`App.cpp`). Each frame:

1. `ImGuiManager::render()` records the ImGui draw commands for the UI on top of the scene.
2. The 3D scene (terrain, overlays) is rendered via [`webgpu_engine`](webgpu_engine.md) only on demand (e.g. on camera change). Both `webgpu_engine` and the compute graph depend on [`webgpu_base`](webgpu_base.md) for shader preprocessing, GPU resource management, and RAII wrappers.

```mermaid
graph LR
    App("App")
    WebGPUCtx("webgpu::Context")
    Window("webgpu_engine::Window")
    CameraCtrl("camera::Controller")

    ImGuiMgr("ImGuiManager")
    OverlaysPanel("OverlaysPanel")
    NodeGraphPanel("NodeGraphPanel *")
    OverlayImGuiR[["OverlayImGuiRenderer[ ]"]]
    NodeGraph(["NodeGraph *"])
    NodeRenderers[["NodeRenderer[ ] *"]]
    panels[["Panels[ ]"]]

    RenderCtx("RenderingContext")
    EngineCtx("webgpu_engine::Context")
    Schedulers("Schedulers / CloudsManager / SearchService")

    App --> WebGPUCtx
    App --> Window
    App --> CameraCtrl
    App --> ImGuiMgr
    App --> RenderCtx

    ImGuiMgr --> panels
    panels --> OverlaysPanel
    panels --> NodeGraphPanel

    OverlaysPanel --> OverlayImGuiR
    OverlaysPanel -.-> EngineCtx

    NodeGraphPanel --> NodeGraph
    NodeGraphPanel --> NodeRenderers
    NodeGraphPanel -.-> EngineCtx

    RenderCtx --> EngineCtx
    RenderCtx --> Schedulers
```

*\* ... only compiled when `ALP_WEBGPU_APP_ENABLE_COMPUTE` is enabled.*

### ImGuiManager

This class is tying the UI together:
- Initializes the Dear ImGui / ImNodes contexts and fonts.
- Owns the list of `ImGuiPanel`s and draws them every frame.
- Forwards SDL events to ImGui
- Offers some static helper functions

### Panels

All panels implement the `ImGuiPanel` interface (`apps/webgpu_app/ui/ImGuiPanel.h`)

In general we follow a feature-based directory layout - each feature folder owns its panel (e.g. `overlay/OverlaysPanel`, `compute/NodeGraphPanel`). General-purpose panels live in `apps/webgpu_app/ui/`. New panels must be manually instantiated and registered in `ImGuiManager` (`apps/webgpu_app/ImGuiManager.cpp`).

### OverlayImGuiRenderer

`OverlaysPanel` is the ImGui panel used to configure which overlays are active and their settings - the actual rendering is done by the corresponding [`webgpu_engine::OverlayRenderer`](webgpu_engine.md#overlays). Each overlay type can have a matching `OverlayImGuiRenderer` subclass (`apps/webgpu_app/overlay/`) that draws its settings controls.

If no specific subclass is registered, the base `OverlayImGuiRenderer` serves as a fallback.

> [!NOTE]
> After adding a new Overlay to the engine you have to:
> - **`OverlaysPanel.cpp`**: extend the `AddType` enum and `ADD_ITEMS[]` array and add a branch in `add_overlay_of_type()` to instantiate the new type.
>
> **and optionally**
> - **UI renderer**: create a matching `OverlayImGuiRenderer` subclass in `apps/webgpu_app/overlay/`
> - **`OverlayImGuiRendererFactory.cpp`**: add a `dynamic_cast` branch in `OverlayImGuiRendererFactory::create()`


### Compute

The `NodeGraphPanel` (`apps/webgpu_app/compute/`) manages the active `NodeGraph` and owns one `NodeRenderer` per node instance, providing the visual/interactive representation in the node-graph editor. Each node type can have a matching `NodeRenderer` subclass (`apps/webgpu_app/compute/nodes/`)

If no specific subclass is registered, the base `NodeRenderer` is used as a fallback (renders the node with sockets but no settings panel).

> [!NOTE]
> When adding a new compute node type, three files must be touched:
> 1. **Compute node**: create a `Node` subclass in `webgpu/compute/nodes/` (the engine-side compute logic).
> 2. **Node registry**: call `register_node()` in `NodeRegistry::NodeRegistry()` so the node can be instantiated by name (required for graph serialization / the add-node dialog).
> 3. **UI renderer**: optionally create a `NodeRenderer` subclass in `apps/webgpu_app/compute/nodes/` and add a `dynamic_cast` branch in `NodeRendererFactory::create()`

#### OverlayRenderNode

`OverlayRenderNode` (`apps/webgpu_app/compute/OverlayRenderNode.h`) is a special node that bridges the compute graph and the rendering system. Unlike regular compute nodes it lives in the app layer because it holds a reference to `webgpu_engine::Context`. When executed, it forwards the graph's output texture to a `TextureOverlay` managed by the engine's `OverlayRenderer`, making compute results visible in the 3D viewport.
