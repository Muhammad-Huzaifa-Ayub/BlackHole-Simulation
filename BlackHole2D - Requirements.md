# Requirements Specification — 2D Schwarzschild Lensing Engine

This document defines the full technical, architectural, hardware, and software requirements required to build, compile, and execute the High-Performance 2D Schwarzschild Lensing Engine.

The system is designed around a **Data-Oriented Design (DoD)** philosophy, prioritizing cache efficiency, deterministic memory layout, and predictable CPU execution paths over abstracted object-heavy designs.

---

# 1. System Requirements

The engine is optimized for real-time execution of relativistic ray tracing using RK4 numerical integration and OpenGL immediate-mode rendering. Performance is strongly dependent on CPU cache behavior, memory bandwidth, and hybrid single-thread + OpenMP scaling.

---

## 1.1 Hardware Requirements

| Component   | Minimum Specification | Recommended Specification |
|-------------|----------------------|---------------------------|
| **CPU**     | Intel Core i5 / AMD Ryzen 5 (2–4 cores, SMT enabled) | Intel i7-4800MQ / Ryzen 7 or higher (4–8 cores preferred) |
| **GPU**     | OpenGL 3.0 compatible integrated GPU | NVIDIA Quadro K2100M / GTX/RTX series or AMD equivalent with OpenGL 3.3+ |
| **RAM**     | 4 GB DDR3/DDR4 | 8–16 GB dual-channel memory (bandwidth sensitive workloads benefit significantly) |
| **Storage** | 50 MB free space | SSD recommended for faster compilation and dependency resolution |
| **Display** | 1280×720 minimum | 1280×960 or higher for stable orthographic projection scaling |

---

## 1.2 Performance Expectations

The engine maintains stable real-time performance under increasing ray density:

- 50 rays → ~120+ FPS (light load)
- 250 rays → ~60–80 FPS (baseline configuration)
- 750 rays → ~25–40 FPS (stress region)
- 2000 rays → depends on CPU core count and OpenMP scaling efficiency

Performance scaling is primarily linear due to:

- elimination of nested rendering loops  
- fixed memory allocation model  
- cache-optimized ray state structures  
- OpenMP-based loop parallelization  

---

# 2. Software Requirements

---

## 2.1 Core Toolchain

The system requires a modern C++ compiler with support for **C++17 or higher**.

### Supported Compilers

- GCC 9.0+
- Clang 10.0+
- MSVC 2019+
- Apple Clang 12+

### Build System

- CMake 3.15+ required  
- Ninja optional but recommended for faster builds  

---

## 2.2 Third-Party Dependencies

| Library    | Version | Purpose |
|------------|---------|---------|
| GLFW       | 3.3+    | Window creation, OpenGL context, input handling |
| GLEW       | 2.1+    | OpenGL function loading and extensions |
| GLM        | 0.9.9+  | Vector/matrix math library |
| OpenMP     | 4.0+    | Parallel ray stepping execution |

---

## 2.3 OpenGL Requirements

- Minimum OpenGL version: **3.0**
- Core profile recommended
- Immediate-mode rendering (`glBegin / glEnd`) used for simplicity
- Alpha blending required for ray trails
- Line smoothing optional depending on GPU driver support

---

# 3. Functional Requirements

---

## 3.1 Physics Simulation Engine

The system must simulate **null geodesics in Schwarzschild spacetime** using:

- Radial geodesic equations derived from Schwarzschild metric  
- RK4 numerical integration per ray  
- Affine parameter evolution (`dl`)  
- Conserved quantities:
  - Energy normalization: `E = 1`
  - Angular momentum: `L = r² dφ`

### Required Features

- Stable photon trajectory integration  
- Horizon detection (`r ≤ r_s`)  
- Photon sphere behavior (~1.5 r_s instability region)  
- Angular momentum conservation correction  
- Numerical stability safeguards (NaN / Inf rejection)  

---

## 3.2 Adaptive Step Controller

Each ray must support adaptive resolution:

### Trigger Regions
- event horizon  
- photon sphere  
- high curvature gradients  

### Rules

- Base step: `dl`
- Adaptive scaling based on:
  - `r / r_s`
  - proximity to photon sphere
  - angular velocity magnitude (`dφ`)

---

## 3.3 Rendering System

### Ray Visualization

- Real-time point rendering (ray heads)
- Line-strip trail rendering (history buffer)
- Alpha gradient trails (fade from head → tail)
- Optional impact-parameter-based coloring

### Black Hole Rendering

- Event horizon disk (triangle fan)
- Reference circles:
  - Event horizon (r_s)
  - Photon sphere (1.5 r_s)
  - ISCO (3 r_s)

---

## 3.4 Trail Buffer System

Each ray must maintain:

- Fixed-size circular buffer (1000 points)
- No runtime allocations
- Overwrite oldest entries automatically
- Deterministic memory footprint

---

## 3.5 Input & Interaction System

### Camera Controls
- Mouse drag → pan world
- Scroll wheel → zoom in/out
- Reset key → restore default view

### Simulation Controls
- Keys 1–5 → switch emission presets
- Arrow Up → +25 rays
- Arrow Down → -25 rays
- Toggle key → impact color mode

---

## 3.6 Ray Emission System

- Parameter `t ∈ [0,1]` defines ray distribution
- Multiple launch modes:
  - left → right beams
  - right → left beams
  - vertical beams
  - diagonal injections

Each ray initializes:
- position (x, y)
- direction vector
- angular momentum seed `L`

---

# 4. Architecture Requirements

---

## 4.1 Data-Oriented Design (DoD)

The system must use:

- contiguous memory layouts  
- struct-based state storage  
- cache-friendly ray arrays  
- minimal pointer usage  

---

## 4.2 Memory Constraints

- No per-frame heap allocations  
- All buffers preallocated  
- Ring buffers for trails  
- Deterministic memory footprint  

---

## 4.3 Parallelization Model

OpenMP is used for:

- ray stepping loop  
- guided scheduling (optional)  

Requirements:

- thread-safe ray updates  
- no shared mutable physics state  
- independent ray evolution  

---

## 4.4 Numerical Stability

System must:

- reject NaN/Inf states  
- enforce angular momentum conservation  
- clamp near horizon  
- prevent divergence  

---

# 5. Build & Execution Requirements

---

## 5.1 Build Steps

1. Configure CMake  
2. Link dependencies (GLFW, GLEW, GLM)  
3. Enable OpenMP:
   - `-fopenmp` (GCC/Clang)
4. Compile with optimization:
   - `-O2` or `-O3`

---

## 5.2 Runtime Requirements

- OpenGL-compatible driver
- Desktop window system (X11 / Wayland / Windows / macOS)
- Single display output minimum
- Real-time loop target: 60 FPS baseline

---


# 7. Summary

This engine is a **high-performance relativistic visualization system** designed around:

- physically consistent Schwarzschild ray tracing  
- strict numerical stability  
- cache-aware memory design  
- elimination of runtime allocations  
- linear, scalable execution architecture  

It prioritizes:

- deterministic performance  
- architectural clarity  
- real-time scalability  
- physics correctness under numerical constraints  