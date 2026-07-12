# Schwarzschild 3D Black Hole Engine

> A real-time GPU implementation of Schwarzschild null geodesic ray tracing using a fourth-order Runge-Kutta (RK4) integrator.

---

# Overview

The Schwarzschild 3D Black Hole Engine is a physically motivated real-time renderer that simulates gravitational lensing around a non-rotating Schwarzschild black hole.

Unlike traditional rasterization or screen-space distortion techniques, every pixel represents an independent photon trajectory integrated numerically through curved spacetime.

The renderer executes the complete geodesic integration directly inside an OpenGL fragment shader, allowing tens of millions of photon paths to be evaluated every second on consumer GPUs.

The objective of this project is educational visualization of General Relativity while maintaining interactive frame rates.

---

# Features

## General Relativity

- Schwarzschild spacetime
- Null geodesic integration
- Event horizon
- Photon sphere behavior
- Gravitational lensing
- Relativistic accretion disk
- Doppler beaming
- Gravitational redshift
- Escape trajectories
- Photon capture

---

## Numerical Integration

Each pixel solves the null geodesic equations independently using

- Fourth-order Runge-Kutta (RK4)
- Fixed integration budget
- Stable coordinate formulation
- No Euler approximation
- No screen-space distortion tricks
- No lookup textures
- No precomputed ray tables

---

## Rendering

- OpenGL 4.3
- GLSL 430
- Full-screen fragment shader
- Uniform Buffer Objects
- Dynamic camera
- HDR color pipeline
- ACES filmic tone mapping
- Gamma correction
- Procedural star field
- Procedural accretion disk

---

## Camera

Interactive orbital camera supporting

- Rotation
- Zoom
- Dynamic aspect ratio
- Continuous movement
- Arbitrary viewing angles

---

## Benchmarking

Built-in benchmark system reports

- Average FPS
- Average frame time
- Pixels rendered
- Pixels per second
- Estimated RK4 evaluations
- RK4 evaluations per second
- Estimated GFLOPS
- Runtime statistics
- GPU information
- OpenGL version
- GLSL version

---

# Mathematical Model

The renderer models light propagation in Schwarzschild spacetime.

Each fragment represents one photon.

The photon is initialized from the camera position and integrated through curved spacetime until one of the following occurs:

- escapes to infinity
- intersects the accretion disk
- falls inside the event horizon

The trajectory is computed numerically using the Schwarzschild geodesic equations.

No image-space approximation is used.

---

# Physical Model

Current implementation includes

- Schwarzschild metric
- Circular accretion disk
- Thermal emission approximation
- Gravitational redshift
- Doppler boosting
- Relativistic beaming
- Volumetric disk thickness approximation
- Photon capture
- Escape boundary

---

# Rendering Pipeline

```
Camera

↓

Generate Primary Ray

↓

Construct Orbital Plane

↓

Initialize Geodesic State

↓

RK4 Integration

↓

Disk Intersection

↓

Thermal Emission

↓

Escape or Capture

↓

Procedural Skybox

↓

ACES Tone Mapping

↓

Gamma Correction

↓

Framebuffer
```

---

# Numerical Solver

Each pixel performs approximately

```
250 RK4 integration steps
```

Each RK4 step consists of

- 4 derivative evaluations
- state update
- coordinate reconstruction
- intersection testing
- termination testing

The solver is entirely executed inside the fragment shader.

---

# Accretion Disk

The accretion disk is procedurally generated.

Characteristics

- finite thickness
- radial density profile
- thermal emission
- Doppler shift
- relativistic beaming
- gravitational redshift
- smooth volumetric blending

No textures are used.

---

# Star Field

Background stars are generated procedurally.

Features

- deterministic hashing
- multiple frequency layers
- stable under camera movement
- high density
- color variation
- no texture assets

---

# Tone Mapping

The renderer outputs HDR radiance.

Final display uses

- ACES Filmic tone mapping
- Gamma correction

This preserves highlights while maintaining visibility of faint gravitational lensing.

---

# Controls

| Action | Control |
|---------|----------|
| Rotate camera | Left Mouse Button + Drag |
| Zoom | Mouse Wheel |
| Exit | Close Window |

---

# Benchmark Output

Example benchmark

```
Average FPS

Average Frame Time

Frames Rendered

Pixels Rendered

Pixels/sec

Estimated RK4 Steps

RK4 Steps/sec

Estimated GFLOPS/sec
```

These statistics describe only the benchmarked frames after the warm-up period.

---

# Performance

Performance depends on

- GPU architecture
- shader compiler
- OpenGL driver
- screen resolution
- integration step count

Typical workloads involve

Millions of pixels

×

250 RK4 integrations

×

4 derivative evaluations

per rendered frame.

---

# Architecture

The project is intentionally compact.

```
3D_BlackHole.cpp

├── OpenGL initialization
├── Camera system
├── Shader compilation
├── GPU data upload
├── Runtime benchmark
├── Event callbacks
├── Vertex shader
├── Fragment shader
└── Main render loop
```

Everything required for rendering is contained inside a single source file.

---

# Dependencies

- C++17
- OpenGL 4.3
- GLFW
- GLEW
- CMake

---

# Compilation

```
mkdir build

cd build

cmake ..

cmake --build . --config Release
```

---

# Output

The application produces

- interactive rendering window
- console performance display
- benchmark log

Runtime statistics are appended to

```
RuntimeLogs.txt
```

---

# Current Limitations

The current renderer models only the Schwarzschild solution.

Not currently implemented

- Kerr metric
- spinning black holes
- frame dragging
- polarization
- multiple scattering
- adaptive RK integration
- adaptive timestep
- gravitational time dilation visualization
- realistic radiative transfer
- physically based spectral rendering

---

# Project Goals

The project emphasizes

- numerical correctness
- mathematical transparency
- deterministic rendering
- educational visualization
- real-time performance
- compact implementation
- modern OpenGL

rather than cinematic effects or game-oriented rendering.

---

# Future Work

Potential future improvements include

- Kerr black hole support
- Adaptive RK45 integration
- CUDA implementation
- Compute shader implementation
- Multiple black holes
- Binary lensing
- Gravitational waves
- Polarization
- Volumetric radiative transfer
- Spectral rendering
- Bloom
- HDR output
- VR support

---

# License

This project is intended for educational, research, and visualization purposes.

---

# Acknowledgements

This renderer is inspired by classical numerical relativity, Schwarzschild geometry, and GPU-based scientific visualization techniques.

Although simplified for real-time execution, every rendered pixel follows a numerically integrated null geodesic through Schwarzschild spacetime rather than an image-space approximation.

---

# Author

**Muhammad Huzaifa**