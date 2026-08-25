# Camera controls

The 3D view's camera is driven by `nucleus/camera/Controller.cpp`, shared by both
the `app` (AlpineMaps.org) and `webgpu_app` (weBIGeo) frontends. It supports three
interaction styles, switchable at any time with a number key, all responding to the
same mouse/keyboard/touch input (`Controller::key_press`,
`nucleus/camera/Controller.cpp:166-173`).

| Key | Mode | Class |
|---|---|---|
| `1` | Orbit (default) | `OrbitInteraction` |
| `2` | First-person / flight | `FirstPersonInteraction` |
| `3` | CAD | `CadInteraction` |

## Orbit (default)

Rotates/pans around a point on the terrain under the cursor.

| Input | Action |
|---|---|
| Left-drag | Pan |
| Middle-drag, or `Ctrl` + Left-drag | Rotate/orbit around the point under the cursor |
| Right-drag, or `Alt` + Left-drag | Zoom (dolly) |
| Scroll wheel | Zoom toward cursor |
| Touch: one-finger drag | Pan |
| Touch: pinch | Zoom |
| Touch: two-finger shove | Tilt |
| Touch: two-finger rotate | Rotate |

Source: `nucleus/camera/OrbitInteraction.cpp:49-118`.

## First-person / flight

Free movement through the scene, mouse-look instead of orbit-around-a-point.

| Input | Action |
|---|---|
| Left-drag, or Middle-drag | Look around |
| `W` / `S` | Move forward / backward |
| `A` / `D` | Move left / right |
| `E` / `Q` | Move up / down |
| `Shift` (held) | 3x movement speed |
| Scroll wheel | Adjust movement speed (1x–4000x), not zoom |

Source: `nucleus/camera/FirstPersonInteraction.cpp:29-109`.

## CAD

Like Orbit, but with a double-click fly-to and CAD-style zoom math.

| Input | Action |
|---|---|
| Left-drag | Pan |
| Double left-click | Smooth fly-to animation to the clicked point |
| Middle-drag, or `Ctrl` + Left-drag | Orbit around the operation centre |
| Right-drag, or `Alt` + Left-drag | Zoom (dolly) |
| Scroll wheel | Zoom |

Source: `nucleus/camera/CadInteraction.cpp:34-101`.

## Notes

- All three modes read the same physical input events (SDL events forwarded through
  `apps/webgpu_app/util/InputMapper.cpp` to whichever `Controller` interaction style
  is currently active in `webgpu_app`, or the equivalent Qt input path in `app`) —
  switching modes doesn't require rebinding anything.
- Switching modes mid-interaction cancels any in-flight camera animation and resets
  interaction state (`Controller::key_press`, same file).
