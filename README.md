# Claude of Duty

[mshumer/Claude-of-Duty](https://github.com/mshumer/Claude-of-Duty), a procedural first-person shooter written in
three.js, ported to the Kaigen Engine, a high performance, cross-platform 3D engine written in C. The web page runs both
versions of the game in the browser, so you can switch engines and compare.

[Play it on kaigen3d.com](https://kaigen3d.com/demos/claude-of-duty/) · [Register for the closed beta.](https://kaigen3d.com/)

![Claude of Duty](preview.png)

Like the original, it has no art assets:
- the street, buildings, props and materials are generated at load in parallel C from a seed (the same seed gives the same street);
- a day/night sky, height fog, auto-exposure, motion blur, contact shadows and four quality presets;
- three weapons with fire modes, recoil, ADS, reload, inspect, ballistics and penetration;
- soldiers with procedural meshes and animation, squads, cover, grenades and ragdolls;
- procedural audio rendered at load, with reverb by space, occlusion and a ducking mixer;
- the HUD, minimap, compass, hit markers and the pause menu.

## Running it

The project needs the Kaigen Engine, which is in closed beta. `hz` is a link to your engine checkout at the project
root (it is not part of this repository).

```
hz/hzbuild run                   # native debug build, then run
hz/hzbuild --release run         # native optimized build
hz/hzbuild wasm --release run    # WebAssembly build, served at http://localhost:3000
```

Click the window to lock the pointer and play. Escape releases it and opens the pause menu.

## Controls

| Input | Action |
|---|---|
| W A S D / arrows | move |
| Mouse | look |
| Left mouse | fire |
| Right mouse | aim down sights |
| Shift | sprint |
| Space | jump, vault, mantle |
| C / Left Ctrl | crouch (toggles crouch and stand) |
| Z | prone (toggles prone and crouch) |
| Q / E | lean left / right |
| R | reload |
| 1 2 3 / Tab / wheel | rifle, smg, pistol / next weapon |
| B | fire mode |
| I | inspect |
| Escape | pause menu: graphics preset, sensitivity, field of view, volume, invert look |

Gamepad: left stick moves, right stick looks, RT fires, LT aims, A jumps, B crouches, X reloads, Y swaps, L3 sprints,
Start pauses. F, V, G and T (use, melee, grenade, flashlight) are bound but do nothing.

Touch (iOS, touch screens): a stick under the left thumb moves (push it past the ring, forward, to sprint), dragging anywhere
else looks, and on-screen buttons fire, aim (a toggle), jump, crouch, reload, swap weapons and pause. There is no pointer
lock on touch: the match plays as soon as it loads. Phones start on the High preset (the highest an iPhone 13 holds at
60 fps); desktops start where the engine's hardware rules put them. From there the engine's dynamic quality measures the
GPU during the match and steps the preset down or up to hold 60 fps. Picking a preset in the pause menu fixes it;
Defaults hands it back to dynamic quality.

`web/` is the comparison page published at kaigen3d.com. `web/threejs/` is the original game built from [d9b237b](https://github.com/mshumer/Claude-of-Duty/commit/d9b237b)
(`vite build --base ./`) with one addition, the same frame-rate panel the Kaigen version draws, so the two can be
compared. MIT licensed by its author.
