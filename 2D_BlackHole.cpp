#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

#define _USE_MATH_DEFINES
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace glm;
using namespace std;

// ============================================================
// 2D Schwarzschild lensing engine
//
// Physics policy:
// - Keep the Schwarzschild null-geodesic model.
// - Keep E = 1.0 as shared affine normalization.
// - Keep the Christoffel-derived radial equation.
// - Do not switch to Kerr or effective-potential rewrites.
//
// Tuned stages:
// - Stage 13: stateless RK4 kernel (no Ray copying, no trail copies)
// - Stage 14: OpenMP-ready ray stepping
// - Stage 15: bounded trail memory via fixed ring buffer
// - Stage 16: adaptive substep controller
// - Stage 17: active-ray culling
// - Stage 18: cache-friendly scalar integration state
// - Stage 19: device-aware tuning for i7-4800MQ + Quadro K2100M
//
// Added improvements:
// - exact angular-momentum re-enforcement after RK4
// - reference circles (horizon, photon sphere, ISCO)
// - impact-parameter coloring
// - pan/zoom/reset callbacks
// ============================================================

double c = 299792458.0;
double G = 6.67430e-11;

struct Ray;
struct TrailBuffer;
void rk4Step(Ray &ray, double dl, double rs);
vec3 impactColor(double L, double b_crit);
void drawCircle(double radius, float red, float green, float blue, float alpha);
void drawReferenceCircles(double rs);
void drawAllRays(const vector<Ray> &rays, const vector<TrailBuffer> &trails, double b_crit);

bool g_useImpactColors = true;
int g_launchPreset = 0;
int g_rayCount = 100;
double g_b_crit = 0.0;

constexpr float WORLD_X = 1.0e11f;
constexpr float WORLD_Y = 7.5e10f;

void rebuildRays();
void makeLaunchRay(int preset, double t, vec2 &pos, vec2 &dir);

// ─────────────────────────────────────────────────────────────────────────────
// Engine: OpenGL context + orthographic camera.
// The simulation world is in meters; the orthographic projection is just a
// visual window into that world.
// ─────────────────────────────────────────────────────────────────────────────
struct Engine
{       
        GLFWwindow *window;
        int WIDTH = 1280;
        int HEIGHT = 720;

        // World-space extents in meters.
        float width = 100000000000.0f;
        float height = 75000000000.0f;

        float offsetX = 0.0f, offsetY = 0.0f;
        float zoom = 1.0f;
        bool middleMousePressed = false;
        double lastMouseX = 0.0, lastMouseY = 0.0;

        Engine()
        {
                if (!glfwInit())
                {
                        cerr << "Failed to initialize GLFW" << endl;
                        exit(EXIT_FAILURE);
                }

                window = glfwCreateWindow(
                    WIDTH,
                    HEIGHT,
                    "Black Hole Simulation",
                    NULL,
                    NULL);

                if (!window)
                {
                        cerr << "Failed to create GLFW window" << endl;
                        glfwTerminate();
                        exit(EXIT_FAILURE);
                }

                glfwMakeContextCurrent(window);
                glfwSwapInterval(1);

                glewExperimental = GL_TRUE;
                if (glewInit() != GLEW_OK)
                {
                        cerr << "Failed to initialize GLEW" << endl;
                        glfwDestroyWindow(window);
                        glfwTerminate();
                        exit(EXIT_FAILURE);
                }

                glViewport(0, 0, WIDTH, HEIGHT);
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        }

        void setupCallbacks()
        {
                glfwSetWindowUserPointer(window, this);

                // Scroll wheel zooms the world window in/out.
                glfwSetScrollCallback(
                    window,
                    [](GLFWwindow *w, double, double dy)
                    {
                            auto *e = static_cast<Engine *>(glfwGetWindowUserPointer(w));
                            float factor = (dy > 0.0) ? 0.9f : 1.1f;

                            e->zoom *= factor;
                            e->zoom = std::clamp(e->zoom, 0.01f, 50.0f);
                    });

                // Middle mouse toggles panning.
                glfwSetMouseButtonCallback(
                    window,
                    [](GLFWwindow *w, int button, int action, int)
                    {
                            auto *e = static_cast<Engine *>(glfwGetWindowUserPointer(w));

                            if (button == GLFW_MOUSE_BUTTON_MIDDLE)
                            {
                                    e->middleMousePressed = (action == GLFW_PRESS);
                            }

                            if (action == GLFW_PRESS)
                            {
                                    glfwGetCursorPos(w, &e->lastMouseX, &e->lastMouseY);
                            }
                    });

                // Drag to pan.
                glfwSetCursorPosCallback(
                    window,
                    [](GLFWwindow *w, double x, double y)
                    {
                            auto *e = static_cast<Engine *>(glfwGetWindowUserPointer(w));

                            if (e->middleMousePressed)
                            {
                                    double xScale = 2.0 * e->width * e->zoom / e->WIDTH;
                                    double yScale = 2.0 * e->height * e->zoom / e->HEIGHT;

                                    e->offsetX -= float((x - e->lastMouseX) * xScale);
                                    e->offsetY += float((y - e->lastMouseY) * yScale);
                            }

                            e->lastMouseX = x;
                            e->lastMouseY = y;
                    });

                // Keyboard controls.
                glfwSetKeyCallback(
                    window,
                    [](GLFWwindow *w, int key, int, int action, int)
                    {
                            auto *e = static_cast<Engine *>(glfwGetWindowUserPointer(w));

                            if (action != GLFW_PRESS)
                                    return;

                            switch (key)
                            {
                            case GLFW_KEY_R:
                                    e->offsetX = 0.0f;
                                    e->offsetY = 0.0f;
                                    e->zoom = 1.0f;
                                    break;

                            case GLFW_KEY_C:
                                    g_useImpactColors = !g_useImpactColors;
                                    break;

                            case GLFW_KEY_1:
                                    g_launchPreset = 0;
                                    rebuildRays();
                                    break;

                            case GLFW_KEY_2:
                                    g_launchPreset = 1;
                                    rebuildRays();
                                    break;

                            case GLFW_KEY_3:
                                    g_launchPreset = 2;
                                    rebuildRays();
                                    break;

                            case GLFW_KEY_4:
                                    g_launchPreset = 3;
                                    rebuildRays();
                                    break;

                            case GLFW_KEY_5:
                                    g_launchPreset = 4;
                                    rebuildRays();
                                    break;

                            case GLFW_KEY_UP:
                                    g_rayCount += 25;
                                    if (g_rayCount > 2000)
                                            g_rayCount = 2000;
                                    rebuildRays();
                                    break;

                            case GLFW_KEY_DOWN:
                                    g_rayCount -= 25;
                                    if (g_rayCount < 10)
                                            g_rayCount = 10;
                                    rebuildRays();
                                    break;

                            default:
                                    break;
                            }
                    });
        }

        void run()
        {
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
                glOrtho(
                    -width * zoom + offsetX,
                    width * zoom + offsetX,
                    -height * zoom + offsetY,
                    height * zoom + offsetY,
                    -1.0,
                    1.0);

                glMatrixMode(GL_MODELVIEW);
                glLoadIdentity();
        }
};

Engine engine;

// ─────────────────────────────────────────────────────────────────────────────
// Black hole disk at the origin.
// Physics uses rs only; this is the visual primitive.
// ─────────────────────────────────────────────────────────────────────────────
struct BlackHole
{
        vec3 position;
        double mass;
        double radius;
        double r_s;

        BlackHole(vec3 pos, double m)
            : position(pos), mass(m), radius(0.0)
        {
                r_s = 2.0 * G * mass / (c * c);
        }

        void draw()
        {
                glBegin(GL_TRIANGLE_FAN);
                glColor3f(1.0f, 0.0f, 0.0f);
                glVertex2f(0.0f, 0.0f);

                for (int i = 0; i <= 100; ++i)
                {
                        float a = 2.0f * M_PI * i / 100.0f;
                        glVertex2f(float(r_s * cos(a)), float(r_s * sin(a)));
                }

                glEnd();
        }
};

BlackHole SagA(vec3(0.0f, 0.0f, 0.0f), 8.54e36);

// ─────────────────────────────────────────────────────────────────────────────
// Ray state.
//
// Trail storage is a fixed ring buffer:
// - no heap allocations during stepping
// - no allocator contention under OpenMP
// - bounded memory footprint
//
// Note:
// - The Ray object is large but stable.
// - rk4Step() never copies the Ray object.
// - trail remains a rendering/history buffer only.
// - E = 1.0 gives every ray the same affine scale.
// - Since E = 1.0, L is the impact-parameter scale.
// ─────────────────────────────────────────────────────────────────────────────
struct Ray
{
        static constexpr size_t MAX_TRAIL_POINTS = 1000;

        double x, y;
        double r, phi;
        double dr, dphi;
        double E, L;


        bool is_active = true;

        Ray(vec2 pos, vec2 dir)
        {
                x = double(pos.x);
                y = double(pos.y);

                r = sqrt(x * x + y * y);
                phi = atan2(y, x);

                // dir is a direction only, not a physical velocity.
                // This keeps the incoming ray family consistent and symmetric.
                double travel_angle = atan2(double(dir.y), double(dir.x));

                // Local polar projection used to seed the null-geodesic family.
                double dr_unit = cos(travel_angle - phi);

                // Avoid division by zero in the angular seed.
                double safe_r = std::max(r, 1e-12);
                double dphi_unit = sin(travel_angle - phi) / safe_r;

                // Shared affine normalization.
                E = 1.0;
                L = r * r * dphi_unit;
                dphi = L / (safe_r * safe_r);

                // Null-condition seed for radial motion.
                double f = std::max(1.0 - SagA.r_s / safe_r, 1e-6);
                double vr2 = E * E - f * (L / safe_r) * (L / safe_r);

                double dr_sign = (dr_unit < 0.0) ? -1.0 : 1.0;
                dr = dr_sign * ((vr2 > 0.0) ? sqrt(vr2) : 0.0);

        }

        

        // Stage 16-19 step policy:
        // - adaptive radial scaling
        // - Gaussian refinement near photon sphere
        // - angular limiter to suppress the crab-claw artifact
        // - bounded trail memory
        // - culling for absorbed / escaped rays
        //
        // Per active ray complexity:
        //   O(local_substeps)
        //
        // Memory:
        //   O(MAX_TRAIL_POINTS)
        void step(double base_dl, double rs)
        {
                if (!is_active)
                {
                        return;
                }

                // Absorption cutoff. Prevents horizon overshoot and wasted work.
                if (r <= rs * 1.01)
                {
                        is_active = false;
                        return;
                }

                // Safety guard against numerical corruption.
                if (!std::isfinite(r) || !std::isfinite(dr) || !std::isfinite(dphi))
                {
                        is_active = false;
                        return;
                }

                // Adaptive controller:
                // The photon sphere is numerically the hardest region.
                double u = rs / r;
                double photonSphere = 1.5 * rs;
                double photonDist = std::fabs(r - photonSphere) / photonSphere;

                int local_substeps = 2;
                if (photonDist < 0.12)
                {
                        local_substeps = 16;
                }
                else if (u > 0.92)
                {
                        local_substeps = 4;
                }
                else if (u > 0.50)
                {
                        local_substeps = 6;
                }

                // Base step budget.
                double total_dl = base_dl / (1.0 + 15.0 * u / (1.0 - u + 1e-6));

                // Photon-sphere Gaussian refinement.
                double x_ps = (r - photonSphere) / rs;
                double photon_refine = 1.0 + 6.0 * std::exp(-25.0 * x_ps * x_ps);
                total_dl /= photon_refine;

                double micro_dl = total_dl / double(local_substeps);

                for (int n = 0; n < local_substeps && is_active; ++n)
                {
                        if (r <= rs * 1.01)
                        {
                                is_active = false;
                                break;
                        }

                        if (!std::isfinite(r) || !std::isfinite(dr) || !std::isfinite(dphi))
                        {
                                is_active = false;
                                break;
                        }

                        // Angular-step limiter.
                        // Keeps the ray from jumping too far in angle per RK4 call.
                        const double max_delta_phi = 0.01;
                        double dl_safe = micro_dl;

                        if (std::fabs(dphi) > 1e-12)
                        {
                                double dl_angular = max_delta_phi / std::fabs(dphi);
                                dl_safe = std::min(dl_safe, dl_angular);
                        }

                        rk4Step(*this, dl_safe, rs);

                        // Convert back to Cartesian for rendering.
                        x = r * cos(phi);
                        y = r * sin(phi);

                        // Radial escape culling.
                        const double ESCAPE_RADIUS_SQ = 4.0e22; // (2e11)^2
                        double current_r2 = x * x + y * y;

                        if (current_r2 > ESCAPE_RADIUS_SQ)
                        {
                                is_active = false;
                                break;
                        }
                }
        }
};

struct TrailBuffer
{
        static constexpr size_t MAX_TRAIL_POINTS = 1000;

        array<vec2, MAX_TRAIL_POINTS> trail{};

        size_t trail_head = 0;
        size_t trail_count = 0;

        void push(const vec2 &point)
        {
                trail[trail_head] = point;
                trail_head = (trail_head + 1) % MAX_TRAIL_POINTS;

                if (trail_count < MAX_TRAIL_POINTS)
                {
                        trail_count++;
                }
        }

        size_t start() const
        {
                return (trail_head + MAX_TRAIL_POINTS - trail_count) % MAX_TRAIL_POINTS;
        }
};

// Global ray container.
vector<Ray> rays;
vector<TrailBuffer> trails;

// ─────────────────────────────────────────────────────────────────────────────
// Stateless geodesic RHS.
//
// Inputs are scalar state values only:
// - r, dr, dphi, E
//
// This keeps the kernel:
// - stack-only
// - cache-friendly
// - thread-safe
// - GPU-portable later
//
// The radial equation is preserved from the original Christoffel derivation.
// ─────────────────────────────────────────────────────────────────────────────
void geodesicRHS(double r, double dr, double dphi, double E, double rhs[4], double rs)
{
        if (r <= rs * 1.001)
        {
                rhs[0] = rhs[1] = rhs[2] = rhs[3] = 0.0;
                return;
        }

        double f = std::max(1.0 - rs / r, 1e-6);
        double dt_dl = E / f;

        rhs[0] = dr;
        rhs[1] = dphi;

        rhs[2] =
            -(rs / (2.0 * r * r)) * f * (dt_dl * dt_dl) +
            (rs / (2.0 * r * r * f)) * (dr * dr) +
            (r - rs) * (dphi * dphi);

        rhs[3] = -2.0 * dr * dphi / r;
}

// Linear state interpolation helper used by RK4.
void addState(const double a[4], const double b[4], double factor, double out[4])
{
        for (int i = 0; i < 4; ++i)
        {
                out[i] = a[i] + b[i] * factor;
        }
}

// Stage 13 RK4: stack-only, stateless, no heap traffic.
// This is the critical optimization that removes deep Ray copies.
void rk4Step(Ray &ray, double dl, double rs)
{
        double y0[4] = {ray.r, ray.phi, ray.dr, ray.dphi};
        double k1[4], k2[4], k3[4], k4[4], temp[4];

        geodesicRHS(y0[0], y0[2], y0[3], ray.E, k1, rs);

        addState(y0, k1, dl / 2.0, temp);
        geodesicRHS(temp[0], temp[2], temp[3], ray.E, k2, rs);

        addState(y0, k2, dl / 2.0, temp);
        geodesicRHS(temp[0], temp[2], temp[3], ray.E, k3, rs);

        addState(y0, k3, dl, temp);
        geodesicRHS(temp[0], temp[2], temp[3], ray.E, k4, rs);

        ray.r += (dl / 6.0) * (k1[0] + 2.0 * k2[0] + 2.0 * k3[0] + k4[0]);
        ray.phi += (dl / 6.0) * (k1[1] + 2.0 * k2[1] + 2.0 * k3[1] + k4[1]);
        ray.dr += (dl / 6.0) * (k1[2] + 2.0 * k2[2] + 2.0 * k3[2] + k4[2]);
        ray.dphi += (dl / 6.0) * (k1[3] + 2.0 * k2[3] + 2.0 * k3[3] + k4[3]);

        // Exact angular-momentum conservation.
        // Since L = r^2 * dphi and L is conserved in Schwarzschild spacetime,
        // re-deriving dphi from L prevents long-term RK4 drift.
        if (ray.r > 1e-12)
        {
                ray.dphi = ray.L / (ray.r * ray.r);
        }

        if (ray.r > rs * 1.001)
        {
                double f = std::max(1.0 - rs / ray.r, 1e-6);

                double vr2 =
                    ray.E * ray.E - f * (ray.L / ray.r) * (ray.L / ray.r);

                double sign = (ray.dr >= 0.0) ? 1.0 : -1.0;

                ray.dr = sign * sqrt(std::max(vr2, 0.0));
        }
}

// ─────────────────────────────────────────────────────────────────────────────
// Visual mapping helpers.
//
// Rays are colored by impact parameter. Since E = 1 for all rays here,
// the conserved angular momentum L acts as the impact-parameter scale.
// ─────────────────────────────────────────────────────────────────────────────
vec3 impactColor(double L, double b_crit)
{
        if (b_crit <= 0.0)
        {
                return vec3(1.0f, 1.0f, 1.0f);
        }

        double t = std::fabs(L) / b_crit;

        if (t < 1.0)
        {
                return vec3(1.0f, 0.2f, 0.2f); // capture zone
        }

        if (t < 1.3)
        {
                return vec3(1.0f, 0.85f, 0.0f); // near photon sphere
        }

        if (t < 2.5)
        {
                return vec3(0.2f, 1.0f, 0.4f); // strong deflection
        }

        return vec3(0.4f, 0.6f, 1.0f); // weak deflection
}

// Physical reference circles: horizon, photon sphere, ISCO.
void drawCircle(double radius, float red, float green, float blue, float alpha)
{
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glColor4f(red, green, blue, alpha);
        glBegin(GL_LINE_LOOP);

        for (int i = 0; i < 200; ++i)
        {
                double a = 2.0 * M_PI * double(i) / 200.0;
                glVertex2f(
                    float(radius * cos(a)),
                    float(radius * sin(a)));
        }

        glEnd();
        glDisable(GL_BLEND);
}

void drawReferenceCircles(double rs)
{
        // Horizon glow
        drawCircle(1.05 * rs, 1.0f, 0.0f, 0.0f, 0.20f);

        // Photon sphere
        drawCircle(1.5 * rs, 1.0f, 0.6f, 0.0f, 0.45f);

        // ISCO
        drawCircle(3.0 * rs, 0.0f, 1.0f, 1.0f, 0.30f);
}

// Draw all rays: current heads + trail history.
void drawAllRays(const vector<Ray> &rays, const vector<TrailBuffer> &trails, double b_crit)
{
        // Draw the current positions of active rays.
        glPointSize(3.0f);
        glBegin(GL_POINTS);

        for (const auto &ray : rays)
        {
                if (!ray.is_active)
                        continue;

                if (!std::isfinite(ray.x) || !std::isfinite(ray.y))
                        continue;

                vec3 col = g_useImpactColors
                               ? impactColor(ray.L, b_crit)
                               : vec3(1.0f, 1.0f, 1.0f);

                glColor3f(col.r, col.g, col.b);
                glVertex2f(float(ray.x), float(ray.y));
        }

        glEnd();

        // Draw trails in logical order from oldest to newest.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glLineWidth(1.0f);

        for (size_t k = 0; k < rays.size(); ++k)
        {
                const Ray &ray = rays[k];
                const TrailBuffer &trail = trails[k];

                if (trail.trail_count < 2)
                        continue;

                size_t start = trail.start();

                vec3 col = impactColor(ray.L, b_crit);

                glBegin(GL_LINE_STRIP);

                for (size_t i = 0; i < trail.trail_count; ++i)
                {
                        const vec2 &p =
                            trail.trail[(start + i) % TrailBuffer::MAX_TRAIL_POINTS];

                        float alpha =
                            float(i) / float(trail.trail_count - 1);

                        if (g_useImpactColors)
                        {
                                glColor4f(
                                    col.r,
                                    col.g,
                                    col.b,
                                    std::max(alpha, 0.05f));
                        }
                        else
                        {
                                glColor4f(
                                    1.0f,
                                    1.0f,
                                    1.0f,
                                    std::max(alpha, 0.05f));
                        }

                        glVertex2f(p.x, p.y);
                }

                glEnd();
        }

        glDisable(GL_BLEND);
}

// -----------------------------------------------------------------------------
// Build one ray family based on the selected launch preset.
// t ranges from 0..1 across the ray bundle.
// -----------------------------------------------------------------------------
void makeLaunchRay(int preset, double t, vec2 &pos, vec2 &dir)
{
        switch (preset)
        {
        case 0:
        {
                pos = vec2(
                    -WORLD_X,
                    float((-5.0 + 10.0 * t) * g_b_crit));

                dir = vec2(1.0f, 0.0f);
        }
        break;

        case 1:
                // Right side -> left
                pos = vec2(WORLD_X, float((-1.0 + 2.0 * t) * WORLD_Y));
                dir = vec2(-1.0f, 0.0f);
                break;

        case 2:
                // Top side -> down
                pos = vec2(float((-1.0 + 2.0 * t) * WORLD_X), WORLD_Y);
                dir = vec2(0.0f, -1.0f);
                break;

        case 3:
                // Top-right corner family -> down-left diagonal
                pos = vec2(float(0.2 * WORLD_X + 0.8 * t * WORLD_X), WORLD_Y);
                dir = normalize(vec2(-1.0f, -1.0f));
                break;

        case 4:
                // Bottom-right corner family -> up-left diagonal
                pos = vec2(float(0.2 * WORLD_X + 0.8 * t * WORLD_X), -WORLD_Y);
                dir = normalize(vec2(-1.0f, 1.0f));
                break;

        default:
                // Fallback = left side
                pos = vec2(-WORLD_X, float((-1.0 + 2.0 * t) * WORLD_Y));
                dir = vec2(1.0f, 0.0f);
                break;
        }
}

// -----------------------------------------------------------------------------
// Rebuild all rays using the current launch preset and ray count.
// -----------------------------------------------------------------------------
void rebuildRays()
{
        rays.clear();
        trails.clear();

        rays.reserve(std::max(1, g_rayCount));
        trails.reserve(std::max(1, g_rayCount));

        for (int i = 0; i < g_rayCount; ++i)
        {
                double t = (g_rayCount == 1) ? 0.5 : double(i) / double(g_rayCount - 1);

                vec2 pos, dir;
                makeLaunchRay(g_launchPreset, t, pos, dir);

                rays.emplace_back(pos, dir);

                trails.emplace_back();
                trails.back().push(vec2(float(rays.back().x), float(rays.back().y)));
        }
}       

// ─────────────────────────────────────────────────────────────────────────────
// main
//
// Device-aware defaults for the i7-4800MQ / Quadro K2100M system:
// - 250 rays
// - 1000 trail points
// - 5 physics passes per frame
// - adaptive substeps inside Ray::step()
//
// OpenMP note:
// - If compiled with OpenMP, the ray update loop parallelizes cleanly.
// - If OpenMP is not enabled, the pragma is ignored and the code still runs.
// ─────────────────────────────────────────────────────────────────────────────
int main()
{
        engine.setupCallbacks();

        g_b_crit = (3.0 * sqrt(3.0) / 2.0) * SagA.r_s;
        rebuildRays();

        double base_dl = 1.5e8;

        double fpsTimer = glfwGetTime();
        int frameCounter = 0;

        while (!glfwWindowShouldClose(engine.window))
        {
                engine.run();

                SagA.draw();
                drawReferenceCircles(SagA.r_s);

#ifdef _OPENMP
#pragma omp parallel for schedule(guided)
#endif
                for (int i = 0; i < (int)rays.size(); ++i)
                {
                        for (int s = 0; s < 5; ++s)
                        {
                                if (rays[i].is_active)
                                {
                                        rays[i].step(base_dl, SagA.r_s);
                                        trails[i].push(vec2(float(rays[i].x), float(rays[i].y)));
                                }
                        }
                }

                drawAllRays(rays, trails, g_b_crit);

                // Automatic ray family regeneration
                int alive = 0;
                for (const auto &r : rays)
                {
                        if (r.is_active)
                                ++alive;
                }

                if (alive == 0)
                {
                        rebuildRays();
                }

                frameCounter++;

                double currentTime = glfwGetTime();
                double elapsed = currentTime - fpsTimer;

                // FIXED: Combined into a single, cohesive telemetry block
                if (elapsed >= 1.0)
                {
                        double fps = double(frameCounter) / elapsed;

                        std::cout << "\rFPS: " << fps
                                  << " | Rays: " << rays.size()
                                  << " | Active: " << alive
#ifdef _OPENMP
                                  << " | Threads: " << omp_get_max_threads()
#endif
                                  << "      " << std::flush;

                        frameCounter = 0;
                        fpsTimer = currentTime;
                }

                glfwSwapBuffers(engine.window);
                glfwPollEvents();
        }

        glfwDestroyWindow(engine.window);
        glfwTerminate();
        return 0;
}
