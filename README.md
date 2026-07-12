# Physics + C++ = Pure Excellence ( Part 1 )

About ten days ago, I was scrolling through YouTube when I came across Kavan's video, "Simulating Black Holes in C++".

## As someone who loves both physics and software engineering, I was immediately fascinated. Watching photons bend around a Schwarzschild black hole through an RK4 integrator was incredible, and I couldn't resist cloning the repository and exploring it myself.

The mathematics were beautiful. Respect to Kavan for the inspiration and for making the original idea accessible.

https://www.youtube.com/watch?v=8-B6ryuBkCM&t=0s

But after profiling the code, I started asking myself a question that kept me awake for several nights:

Was the CPU really the bottleneck, or was it the software architecture?

That simple question turned into one of the most enjoyable optimization projects I've worked on.

Over the next several days—and far too many late-night debugging sessions—I dug deep into every layer of the system. I spent hours profiling memory behavior, tracing cache misses, analyzing OpenGL driver overhead, experimenting with adaptive integration schemes, and repeatedly validating orbital accuracy.

What began as a mathematical prototype slowly evolved into a production-oriented simulation engine.

## Problems I discovered

🔹 Dynamic heap allocations occurring inside the physics pipeline.  
🔹 Nested rendering loops causing an explosion of OpenGL driver calls.  
🔹 Fixed integration step sizes producing numerical drift near strong curvature regions.  
🔹 Memory layouts that prevented efficient CPU cache utilization.  

## What I changed

🔹 Hot/cold data separation and Data-Oriented Design.  
🔹 Fixed-size ring buffers with zero runtime allocations.  
🔹 Adaptive sub-stepping based on proximity to the Schwarzschild radius.  
🔹 Complete separation of physics and rendering pipelines.  
🔹 Thread-safe OpenMP parallelization.  
🔹 Improved conservation behavior for photon trajectories.  

________________________________________

## 🔬 Optimization Techniques Used

To transform the simulator from a mathematical prototype into a real-time engine, I focused on architecture rather than simply increasing compute power:

• Data-Oriented Design (DoD): Reorganized hot and cold data to maximize cache locality and hardware prefetch efficiency.  
• Cache-Friendly Layout: Compact physics structures fit efficiently within the CPU's L1/L2 cache hierarchy, minimizing cache misses and pointer chasing.  
• Zero Runtime Allocations: Replaced dynamic vectors with fixed-size ring buffers and stack-allocated primitives, eliminating heap contention and fragmentation.  
• Adaptive RK4 Substepping: Dynamically adjusted integration resolution near strong gravitational fields for higher numerical accuracy.  
• Pipeline Decoupling: Separated physics and rendering into independent linear passes, reducing complexity from O(N²•L) to O(N).  
• OpenMP Parallelization: Removed shared memory bottlenecks, allowing efficient multi-core scaling with #pragma omp parallel for.  

The biggest gains came not from adding hardware, but from respecting the CPU cache hierarchy and eliminating unnecessary work.

## 🌌 Real-Time Relativity Visualization Layer

To make the simulation more intuitive, I also built a real-time visualization and diagnostic layer directly inside the engine.

Rays are dynamically color-mapped based on conserved angular momentum and proximity to the critical impact parameter of the Schwarzschild geometry:

🟥 Capture zone – rays that fall into the black hole  
🟧 Photon sphere region – extreme relativistic lensing near unstable orbits  
🟩 Strong deflection – significant gravitational bending  
🟦 Weak deflection – minor trajectory distortion at larger radii  

A live toggle allows switching between this diagnostic mode and a clean monochrome visualization.

Each photon is rendered with a leading “head” and a fading trail, turning discrete ray steps into a continuous relativistic flow field.

## Performance comparison under a standardized workload

N = 250 active rays  
L = 800 trail coordinates  

Operations per frame  
Original prototype:  
• 100,228,500 operations/frame  
Optimized engine:  
• 2,066,000 operations/frame  

________________________________________

Physics arithmetic  
Original:  
38,250 FLOPs  
Optimized:  
815,000 FLOPs  

Despite performing significantly more mathematical work, the optimized engine spends far less time fighting memory and graphics overhead.

________________________________________

Dynamic memory allocations  
Original:  
750 kernel allocations every frame  
Optimized:  
0 allocations  

________________________________________

Graphics API submissions  
Original:  
100,189,500 unbatched OpenGL calls  
Optimized:  
251,000 linear pipeline submissions  

________________________________________

Throughput requirement at 60 FPS  
Original:  
6.05 billion operations/sec required  
Optimized:  
0.124 billion operations/sec  

________________________________________

Achievable framerate  
Original:  
~0.399 FPS  
Optimized:  
60+ FPS  

________________________________________

Time complexity  
Per frame:  
Original:  
O(N²•L)  
Optimized:  
O(N)  

Long-term execution:  
Original:  
O(N²•F²)  
Optimized:  
O(N•F)  

________________________________________

Space complexity  
Original:  
O(N•L)  
Optimized:  
O(N)  

Peak memory behavior:  
Original:  
1.641 MB with transient bursts  
Optimized:  
2.018 MB with completely flat memory usage  

________________________________________

# 📊 Real-Time Performance Scaling

Even under increasing computational load, the decoupled architecture maintains stable performance:

• 50 rays → 120+ FPS  
• 250 rays → ~70 FPS  
• 750 rays → ~30 FPS  

This stability comes directly from the architecture: linear processing, cache-aware memory layout, and elimination of runtime allocation pressure.

The biggest lesson I learned

This project reinforced something I've believed for a long time:

Performance problems are often architectural problems.

The CPU wasn't the enemy.

Memory layout mattered.

Cache locality mattered.

Avoiding allocations mattered.

Separating pipelines mattered.

Algorithmic complexity mattered.

Sometimes we don't need more hardware—we need better software.

Huge credit to Kavan for creating the original black hole simulation and sharing it publicly. Without that inspiration, this journey of profiling, optimization, and countless hours of experimentation wouldn't have happened.

I probably spent more time staring at cache behavior, OpenGL calls, and photon trajectories than I'd like to admit—but that's exactly why I love C++. There is something deeply satisfying about watching performance emerge from understanding the machine instead of simply throwing more hardware at the problem.

## The fastest instruction is the one you never execute, and the fastest memory access is the one already sitting in cache.

#CPlusPlus #PerformanceEngineering #DataOrientedDesign #OpenGL
#GeneralRelativity #PhysicsSimulation
#HighPerformanceComputing #OpenMP #SoftwareEngineering #ComputerGraphics
