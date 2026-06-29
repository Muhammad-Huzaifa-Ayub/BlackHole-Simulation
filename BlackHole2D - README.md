# Physics + C++ = Pure Excellence

A real-time 2D Schwarzschild black hole lensing simulator built in C++ with OpenGL, GLFW, GLEW, GLM, and optional OpenMP acceleration.

## Overview

This repository contains a CPU-first simulation of light rays moving through the curved spacetime around a Schwarzschild black hole. Rays are launched from configurable emitter presets, integrated with a Runge–Kutta 4 (RK4) solver, and rendered in real time with impact-parameter-based coloring, fading motion trails, and reference circles for the event horizon, photon sphere, and ISCO.

The project began as an exploration of a black hole simulation on YouTube and evolved into a more structured engine focused on clarity, numerical stability, and performance. The goal was not to change the physics model, but to improve the way the simulation executes: tighter memory layout, bounded history storage, lower driver overhead, and a clean separation between physics and rendering.

The end result is a simulation that is both visually interesting and technically useful. It demonstrates gravitational lensing, unstable photon orbits, capture behavior, and strong deflection while staying interactive at higher ray counts.

## What this simulator is good at

* Showing how light rays bend near a Schwarzschild black hole.
* Visualizing the capture zone, photon sphere, and strong-deflection region.
* Demonstrating how impact parameter affects ray behavior.
* Highlighting the difference between weak and strong gravitational lensing.
* Serving as a compact reference for RK4-based ray stepping in curved spacetime.
* Providing a performance-oriented example of data layout, culling, and adaptive stepping.

## Core idea

Each ray starts from a configurable launch preset and is stepped forward through a null-geodesic model. The simulation uses conserved quantities to keep the model physically meaningful, while the rendering layer turns the evolving state into a readable visual scene.

The implementation is deliberately split into two concerns:

1. **Physics update** — advance rays using RK4, adaptive sub-stepping, and culling rules.
2. **Rendering** — draw the black hole, reference circles, ray heads, and fading trails.

That separation keeps the code easier to reason about and makes the hot path much more efficient.

## Features

### Real-time gravitational lensing

Rays move through a Schwarzschild field and bend according to their impact parameter. You can immediately see which rays pass safely by, which are strongly deflected, and which are captured.

### Impact-parameter color coding

Each ray can be colored based on its conserved angular momentum relative to the critical impact parameter. This makes the simulation more informative at a glance.

### Fading trails

Every ray keeps a bounded trail history in a ring buffer. The newest point appears as the brightest portion of the trail, and the oldest point fades toward a minimum opacity floor. This creates a motion-field effect instead of a single static line.

### Adaptive sub-stepping

The solver increases local integration resolution near the most numerically sensitive regions, especially near the photon sphere and close to the horizon. This improves stability without wasting effort everywhere else.

### Interactive controls

The camera can be panned and zoomed live. Launch presets can be switched during runtime, and the number of rays can be adjusted dynamically for stress testing.

### Optional parallel execution

When compiled with OpenMP, the ray update loop can be distributed across multiple CPU cores. The design keeps the per-ray state isolated enough that the workload parallelizes cleanly.

### Reference geometry

The scene includes visual circles for the event horizon, photon sphere, and ISCO so that the simulated ray behavior can be interpreted in relation to familiar Schwarzschild landmarks.

## Physics model

The simulation stays within the Schwarzschild null-geodesic framework.

A few important choices are preserved intentionally:

* The spacetime model remains Schwarzschild.
* The ray family is treated as lightlike geodesics.
* The conserved energy scale is normalized with `E = 1.0`.
* The conserved angular momentum `L` acts as the impact-parameter scale.
* The radial equation is retained from the Christoffel-derived form.

This means the project remains a black hole lensing simulator rather than a general relativity sandbox with a different spacetime metric. That keeps the behavior easy to verify and consistent with the original concept.

## Architecture highlights

### Stateless RK4 stepping

The RK4 kernel works on scalar state values rather than copying entire ray objects. This keeps the solver compact and avoids unnecessary heap traffic or deep copying inside the hot loop.

### Bounded trail storage

Each ray stores trail history in a fixed-size ring buffer of 1000 points. This guarantees predictable memory usage, prevents unbounded growth, and avoids allocation churn while the simulation runs.

### Hot/cold data separation

The current ray state is kept small and frequently accessed, while trail history is stored separately. This improves cache locality and keeps the physics loop focused on the data it needs most.

### Adaptive integration policy

The number of local micro-steps changes based on proximity to the black hole. Far-field rays can advance with fewer micro-steps, while rays near the photon sphere get extra refinement.

### Ray culling and regeneration

Rays are deactivated when they are absorbed or escape beyond a large radius. Once all rays are inactive, a new bundle is generated automatically so the scene keeps running without manual intervention.

### Rendering separation

Physics is updated first, then the complete scene is rendered afterward. That avoids nested draw calls inside the update loop and makes the execution path more linear.

## Visual design

The rendering layer is designed to make the physics easy to read:

* **Capture zone**: rays that fall into the black hole.
* **Photon sphere region**: rays that skim unstable orbits and show extreme lensing.
* **Strong deflection**: rays that bend heavily but still escape.
* **Weak deflection**: rays that travel farther out and only bend slightly.

The trail system and head marker create a sense of motion over time, so the viewer can track how the ray field evolves instead of only seeing isolated points.

## Controls

* **1** — Left-side parallel beam
* **2** — Right-side parallel beam
* **3** — Top-side beam
* **4** — Top-right diagonal family
* **5** — Bottom-right diagonal family
* **Up Arrow** — Increase ray count
* **Down Arrow** — Decrease ray count
* **C** — Toggle impact-parameter coloring
* **R** — Reset camera position and zoom
* **Middle Mouse Drag** — Pan the scene
* **Mouse Wheel** — Zoom in and out

## Code structure

The repository is organized around a few main ideas:

* **Engine setup**: window creation, OpenGL context initialization, camera callbacks, and frame setup.
* **Black hole visualization**: drawing the central mass and reference circles.
* **Ray state**: current position, polar coordinates, conserved quantities, and activity state.
* **Trail storage**: bounded ring buffer for history and fading display.
* **Geodesic integration**: scalar right-hand-side evaluation and RK4 stepping.
* **Rendering helpers**: color mapping, trail drawing, and scene composition.
* **Preset generation**: building ray bundles from different launch geometries.
* **Main loop**: update, render, telemetry, buffer swap, and event polling.

That split makes the code easier to explore and gives each subsystem a clear responsibility.

## Performance notes

This project is intentionally structured around practical optimization ideas:

* The RK4 kernel avoids copying large objects.
* Trail storage is bounded instead of unbounded.
* Rendering is separated from the physics loop.
* Data access is kept contiguous where possible.
* Adaptive stepping concentrates work where the geometry is hardest.
* Optional OpenMP support lets the update loop use more than one CPU core.

The goal is not to chase raw FLOPs. The goal is to keep the engine from wasting work on allocations, unnecessary branching, repeated driver submissions, and overly expensive history growth.

## Build requirements

You will need:

* A modern C++ compiler
* GLFW
* GLEW
* GLM
* OpenGL development headers
* Optional: OpenMP support

### Example Linux build

```bash
g++ -O3 -std=c++17 main.cpp -lglfw -lGLEW -lGL -lm -fopenmp -o blackhole
```

Depending on your Linux distribution, you may need to install the development packages for GLFW, GLEW, and OpenGL separately.

### Example CMake direction

If you prefer CMake, link against GLFW, GLEW, GLM, OpenGL, and optionally OpenMP. The code is written so that OpenMP can be enabled or disabled at compile time without changing the simulation logic.

## Runtime behavior

When the program starts:

1. The window and OpenGL context are created.
2. The Schwarzschild radius is computed from the mass parameter.
3. A ray bundle is generated using the selected preset.
4. The simulation loop begins.
5. Each frame updates active rays, stores their trail points, and renders the full scene.
6. When all rays are inactive, a fresh set is generated automatically.

This makes the application behave like a continuous laboratory for observing lensing behavior.

## Numerical behavior

The solver includes a few safeguards to keep the simulation stable:

* Rays are deactivated when they approach the horizon too closely.
* Invalid numerical states are culled immediately.
* Angular momentum is re-enforced after RK4 stepping to reduce drift.
* Radial velocity is re-derived from the conserved quantities when appropriate.
* The angular step size is limited to avoid abrupt jumps in the trajectory.

These measures are especially useful when rays approach the difficult region near the photon sphere.

## Extending the project

Possible next steps include:

* shader-based trail rendering,
* GPU compute acceleration with the same physics model,
* additional launch presets,
* a HUD for ray state and diagnostics,
* frame capture or trajectory export,
* and more advanced visual overlays for capture and deflection zones.

The current structure should make these additions relatively straightforward.

## Credits

This project was inspired by Kavan’s black hole simulation video:

[https://www.youtube.com/watch?v=8-B6ryuBkCM&t=240s](https://www.youtube.com/watch?v=8-B6ryuBkCM&t=240s)

Huge credit to Kavan for creating a clear and accessible entry point into this topic.

## Closing note

This repository sits at the intersection of physics, graphics programming, and performance-oriented C++. It is meant to be both educational and fun: a small but substantial example of how numerical modeling and low-level engineering can work together.

If you enjoy gravitational lensing, OpenGL, or high-performance C++ simulation work, this project is a great place to explore.

