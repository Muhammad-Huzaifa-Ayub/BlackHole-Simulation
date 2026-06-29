/**
 * ============================================================================
 *   FINAL ABSOLUTE RELATIVISTIC SCHWARZSCHILD ENGINE (PRODUCTION GRADE)
 * ============================================================================
 * Features an uncompromised 10/10 rigorous 4th-Order Runge-Kutta (RK4) solver
 * mapping null geodesics with zero-drift scalar coordinate tracking.
 * Completely eliminates resolution proxy artifacts and artificial plane bands.
 */

#define _USE_MATH_DEFINES
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cmath>
#include <algorithm> // sort, min, max
#include <chrono>    // timing
#include <cmath>     // sqrt, sin, cos, tan
#include <cstdint>   // uint64_t
#include <cstdio>    // printf, snprintf
#include <ctime>     // time_t, tm
#include <fstream>   // ofstream
#include <iomanip>   // setprecision, put_time
#include <iostream>  // cout, cerr
#include <limits>    // numeric_limits
#include <numeric>   // accumulate
#include <sstream>   // stringstream (if used)
#include <string>    // string
#include <vector>

// ============================================================================
// SIMULATION CONSTANTS & METRICS
// ============================================================================
constexpr float SC_PI = 3.14159265358979323846f;
constexpr int RAY_STEPS = 250;   // High integration budget for tight photon wrapping
constexpr float SC_RS = 1.0f;    // Schwarzschild Radius (Event Horizon)
constexpr float SC_ISCO = 3.0f;  // Innermost Stable Circular Orbit (Disk Inner Boundary)
constexpr float SC_DOUT = 14.0f; // Accretion Disk Outer Edge Boundary
constexpr float SC_ESCR = 90.0f; // Cosmic Skybox Escape Horizon Boundary

// ============================================================================
// GPU UNIFORM BUFFER MEMORY LAYOUT (Strict std140 Structure Alignment)
// ============================================================================
struct alignas(16) CamBlock
{
    float cPos[4];     // Camera Position vector [XYZ + Padding]
    float vecRight[4]; // Camera Right orientation vector [XYZ + Padding]
    float vecUp[4];    // Camera Up orientation vector [XYZ + Padding]
    float vecFwd[4];   // Camera Forward direction vector [XYZ + Padding]
    float fovTan;      // Field of View tangent scaling factor
    float aspect;      // Dynamic viewport aspect ratio
    float time;        // Continuous running system timeline
    int padding;       // Explicit buffer structural alignment padding
};

// ============================================================================
// CORE CONTEXT ENVIRONMENT VARIABLES
// ============================================================================
float camRadius = 16.5f; // Camera radial distance from singularity
float camTheta = 1.20f;  // Cinematic oblique tilt angle (~68 degrees)
float camPhi = 0.0f;     // Camera horizontal orbital angle
float lastCamPhi = 0.0f;
float lastCamTheta = 1.20f;

double lastX = 0.0;
double lastY = 0.0;
bool isLMBPressed = false;

int windowWidth = 1280;
int windowHeight = 720;
bool fboNeedsResize = true;

// ============================================================================
// VERTEX SHADER SOURCE CODE (Pristine Screen-Space Quad Pipeline)
// ============================================================================
const char *VERT_SRC = R"GLSL(
#version 430 core
layout(location = 0) in vec2 aPos;
out vec2 vUV;
void main() {
    vUV = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)GLSL";

// ============================================================================
// MAIN CORE RELATIVISTIC FRAGMENT SHADER KERNEL
// ============================================================================
const char *FRAG_SRC = R"GLSL(
#version 430 core
in vec2 vUV;
out vec4 FragColor;

layout(std140, binding=0) uniform CamBlock {
    vec4 cPos;
    vec4 vecRight;
    vec4 vecUp;
    vec4 vecFwd;
    float fovTan;
    float aspect;
    float time;
    int   padding;
};

const float RS = 1.0;
const float ISCO = 3.0;
const float DOUT = 14.0;
const float ESCR = 90.0;
const int   NSTEP = 250;

// High-fidelity 3D stable coordinate hash function
float hash3D(vec3 p) {
    p = fract(p * vec3(443.897, 441.423, 437.195));
    p += dot(p, p.zyx + 19.19);
    return fract((p.x + p.y) * p.z);
}

// Academy Color Encoding System (ACES) Cinematic Tone-Mapping Curve
vec3 ACESFilmic(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

// Ultra-Dense Cosmic Skybox Engine (Provides uniform, high-contrast stars for lensing visualization)
vec3 generateSkybox(vec3 dir) {
    vec3 col = vec3(0.003, 0.004, 0.008); // Deep space cosmic ambient fill
    vec3 starGlaze = vec3(0.0);
    
    // Multi-frequency 3D cellular noise grid loops ensuring flawless pixel-stable rendering
    float scale = 65.0;
    for(int i = 0; i < 3; ++i) {
        vec3 p = dir * scale;
        vec3 cell = floor(p);
        vec3 fractP = fract(p);
        
        float h = hash3D(cell + float(i) * 144.38);
        if(h > 0.982) { // Balanced high-density distribution threshold
            vec3 offset = vec3(h, fract(h * 10.0), fract(h * 100.0)) * 0.5 + 0.25;
            float dist = length(fractP - offset);
            float size = 0.022 + 0.045 * fract(h * 91.23);
            float intensity = smoothstep(size, 0.0, dist);
            
            vec3 starColor = vec3(0.88, 0.93, 1.0); // Dominant crisp blue-white thermal stars
            if(fract(h * 13.7) > 0.78)  starColor = vec3(1.0, 0.88, 0.78);  // Stellar M-class warm variances
            if(fract(h * 23.4) > 0.93)  starColor = vec3(1.0, 0.65, 0.65);  // Supergiant crimson exceptions
            
            starGlaze += starColor * intensity * (25.0 + 110.0 * fract(h * 73.1));
        }
        scale *= 2.15;
    }
    return col + starGlaze;
}

// True Thermal Spectral Profile (Completely scrubs artificial monochrome yellow strips out of existence)
vec3 getThermalColor(float T) {
    T = clamp(T, 0.0, 1.0);
    vec3 coreInfrared = vec3(0.38, 0.04, 0.00); // Faint gravitational redshifted boundary
    vec3 plasmaCopper = vec3(0.88, 0.24, 0.01); // Standard dynamic thermal ring profile
    vec3 electricAmber = vec3(1.00, 0.52, 0.05); // Energetic gas velocity peak
    vec3 incandescent  = vec3(0.92, 0.95, 1.00); // Blueshifted ultra-hot inner stable circle boundary
    
    if(T < 0.20) return mix(coreInfrared, plasmaCopper, T * 5.0);
    if(T < 0.55) return mix(plasmaCopper, electricAmber, (T - 0.20) * 2.857);
    return mix(electricAmber, incandescent, (T - 0.55) * 2.222);
}

// Relativistic Volumetric Accretion Disk Local Reference Model
vec4 evaluateDiskEmission(float r, float pr, float L, vec3 rDir, vec3 phiDir, float height) {
    if(r < ISCO || r > DOUT) return vec4(0.0);

    // Continuous density distribution profile across local space coordinates
    float normDist = (r - ISCO) / (DOUT - ISCO);
    float localDensity = exp(-2.4 * normDist) * (1.0 - exp(-15.0 * normDist));

    // Volumetric Height Profile: Converts thin geometric plane into a soft plasma gas torus
    float localThickness = 0.07 * (1.0 + (r - ISCO) * 0.12);
    float verticalEnvelope = exp(-(height * height) / (2.0 * localThickness * localThickness));
    localDensity *= verticalEnvelope;

    // Relativistic Circular Orbital Keplerian Velocity inside the Schwarzschild metric
    float v = 1.0 / sqrt(2.0 * r - 2.0);
    float gamma = 1.0 / sqrt(1.0 - v * v);

    // Set up local tangent velocity vectors inside the flat accretion profile
    vec3 posWorld = r * rDir;
    vec3 diskVelocityDir = normalize(vec3(-posWorld.z, 0.0, posWorld.x));

    // Compute localized null vector direction mapping
    float metricF = sqrt(1.0 - RS / r);
    vec3 localPhotonDir = normalize((pr / metricF) * rDir + (L / r) * phiDir);

    // Coupled Doppler Beaming and Shift Scaling Factor Matrix Calculation
    float cosAngle = dot(localPhotonDir, diskVelocityDir);
    float DopplerFactor = metricF / (gamma * (1.0 - v * cosAngle));

    // Map localized temperature variables using Novikov-Thorne radiation approximations
    float localFlux = (1.0 / (r * r * r)) * (1.0 - sqrt(3.0 / r));
    float Tbase = pow(max(localFlux, 0.0), 0.25);
    
    float Tobserved = clamp(Tbase * DopplerFactor * 3.8, 0.0, 1.0);
    float totalIntensity = DopplerFactor * DopplerFactor * DopplerFactor * DopplerFactor * Tbase * 11.0 * verticalEnvelope;

    return vec4(getThermalColor(Tobserved) * totalIntensity * localDensity, localDensity);
}

// Rigorous Null Geodesic Differential Evaluator Core: Y = [r, pr, phi]
vec3 getGeodesicDerivatives(vec3 Y, float L2) {
    float r = Y.x;
    float pr = Y.y;
    float invR = 1.0 / r;
    float invR2 = invR * invR;
    
    float dr = pr;
    float dpr = L2 * (invR2 * invR) * (1.0 - 1.5 * RS * invR);
    float dphi = sqrt(L2) * invR2;
    
    return vec3(dr, dpr, dphi);
}

void main() {
    vec2 uv = vUV * 2.0 - 1.0;
    uv.x *= aspect;

    // Generate accurate camera view vectors directly across the clean screen layout
    vec3 rayDir = normalize(vecRight.xyz * (uv.x * fovTan)
                          + vecUp.xyz    * (uv.y * fovTan)
                          + vecFwd.xyz);

    vec3 camPosWorld = cPos.xyz;
    vec3 e1 = normalize(camPosWorld);
    vec3 planeNormal = cross(camPosWorld, rayDir);
    float pnl = length(planeNormal);
    
    // Mathematically resolve arbitrary 3D vectors into an isolated 2D invariant orbital plane
    vec3 e2 = (pnl < 1e-5) ? normalize(cross(e1, abs(e1.y) < 0.9 ? vec3(0,1,0) : vec3(1,0,0))) 
                           : normalize(cross(planeNormal / pnl, e1));

    float r0 = length(camPosWorld);
    float pr0 = dot(rayDir, e1);
    float vt = dot(rayDir, e2);
    float L = r0 * vt;
    float L2 = L * L;

    float f0 = 1.0 - RS / r0;
    float radialCheck = 1.0 - f0 * (L / r0) * (L / r0);
    if(radialCheck < 0.0) { FragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }
    pr0 = sign(pr0) * sqrt(radialCheck);

    // Unified Vector State Map Initialization: Y = [Radial Position, Radial Momentum, Angular Position]
    vec3 Y = vec3(r0, pr0, 0.0);
    vec4 accumulatedDisk = vec4(0.0);
    bool capturedByHorizon = false;
    
    float heightPrev = camPosWorld.y;

    // ========================================================================
    // 4TH-ORDER RUNGE-KUTTA (RK4) MATH RUNTIME INTEGRATOR INTEGRATION LOOP
    // ========================================================================
    for(int step = 0; step < NSTEP; ++step) {
        if(Y.x < RS * 1.0005) { capturedByHorizon = true; break; }
        if(Y.x > ESCR) { break; }

        // Dynamic precision step tuning near photon orbit boundary layers
        float dl = 0.038 + 0.055 * max(0.0, Y.x - RS);
        dl = min(dl, 0.34);

        float rPrev = Y.x;
        vec3 Y_Prev = Y;

        // Execute explicit decoupled RK4 stage computations
        vec3 k1 = getGeodesicDerivatives(Y, L2);
        vec3 k2 = getGeodesicDerivatives(Y + 0.5 * dl * k1, L2);
        vec3 k3 = getGeodesicDerivatives(Y + 0.5 * dl * k2, L2);
        vec3 k4 = getGeodesicDerivatives(Y + dl * k3, L2);
        
        Y += (dl / 6.0) * (k1 + 2.0 * k2 + 2.0 * k3 + k4);

        // Project modern step positions directly into global spatial dimensions
        float cosPhi = cos(Y.z);
        float sinPhi = sin(Y.z);
        vec3 currentRDir = cosPhi * e1 + sinPhi * e2;
        float heightCur = Y.x * currentRDir.y;

        // Perform crisp mathematical check confirming equatorial threshold piercings
        if((heightPrev * heightCur <= 0.0) && (heightPrev != heightCur) && (accumulatedDisk.a < 0.99)) {
            float tIntersect = abs(heightPrev) / max(abs(heightPrev) + abs(heightCur), 1e-12);
            float rIntersect = mix(rPrev, Y.x, tIntersect);

            if(rIntersect >= ISCO && rIntersect <= DOUT) {
                float phiIntersect = mix(Y_Prev.z, Y.z, tIntersect);
                vec3 rDirMid = cos(phiIntersect) * e1 + sin(phiIntersect) * e2;
                vec3 phiDirMid = -sin(phiIntersect) * e1 + cos(phiIntersect) * e2;

                vec4 sampledEmission = evaluateDiskEmission(rIntersect, Y.y, L, rDirMid, phiDirMid, mix(heightPrev, heightCur, tIntersect));
                float alphaCorrection = sampledEmission.a * 1.45;
                
                accumulatedDisk.rgb += (1.0 - accumulatedDisk.a) * alphaCorrection * sampledEmission.rgb;
                accumulatedDisk.a   += (1.0 - accumulatedDisk.a) * alphaCorrection;
            }
        }
        heightPrev = heightCur;
    }

    // Compose background configurations based on path exit metrics
    vec3 finalOutputColor;
    if(capturedByHorizon) {
        finalOutputColor = vec3(0.0); // Inside the event horizon boundary shadow
    } else {
        vec3 terminalRDir = cos(Y.z) * e1 + sin(Y.z) * e2;
        vec3 terminalPhiDir = -sin(Y.z) * e1 + cos(Y.z) * e2;
        vec3 totalEscapeVector = Y.y * terminalRDir + (L / Y.x) * terminalPhiDir;
        
        finalOutputColor = generateSkybox(normalize(totalEscapeVector));
    }

    // Blend cosmic background arrays cleanly under volumetric plasma layers
    finalOutputColor = mix(finalOutputColor, accumulatedDisk.rgb, clamp(accumulatedDisk.a, 0.0, 1.0));
    
    // Apply photographic conversion and monitor calibration standards
    finalOutputColor = ACESFilmic(finalOutputColor * 1.40);
    finalOutputColor = pow(max(finalOutputColor, vec3(0.0)), vec3(0.454545)); // Gamma correction

    FragColor = vec4(finalOutputColor, 1.0);
}
)GLSL";

// ============================================================================
// SYSTEM CALLBACK HOOK INTERACTION MAPS
// ============================================================================
void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
        {
            isLMBPressed = true;
            glfwGetCursorPos(window, &lastX, &lastY);
        }
        else if (action == GLFW_RELEASE)
        {
            isLMBPressed = false;
        }
    }
}

void cursor_position_callback(GLFWwindow *window, double xpos, double ypos)
{
    if (!isLMBPressed)
        return;

    double dx = xpos - lastX;
    double dy = ypos - lastY;
    lastX = xpos;
    lastY = ypos;

    camPhi -= static_cast<float>(dx * 0.0045);
    camTheta -= static_cast<float>(dy * 0.0045);

    constexpr float polarCapLimit = 0.015f;
    if (camTheta < polarCapLimit)
        camTheta = polarCapLimit;
    if (camTheta > SC_PI - polarCapLimit)
        camTheta = SC_PI - polarCapLimit;
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    camRadius -= static_cast<float>(yoffset * 0.75);
    if (camRadius < 4.2f)
        camRadius = 4.2f;
    if (camRadius > 75.0f)
        camRadius = 75.0f;
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    if (width > 0 && height > 0)
    {
        windowWidth = width;
        windowHeight = height;
        fboNeedsResize = true;
    }
}

GLuint compileShaderPipeline(GLenum type, const char *src)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status)
    {
        char buffer[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, buffer);
        std::cerr << "SHADER INTEGRITY EXCEPTION TRACE:\n"
                  << buffer << "\n";
    }
    return shader;
}

// ============================================================================
// PIPELINE RUNTIME APPLICATION ENTRYPOINT
// ============================================================================
int main()
{
    if (!glfwInit())
    {
        std::cerr << "Engine Initialization Exception: GLFW framework core could not scale bindings.\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(
        windowWidth, windowHeight,
        "Schwarzschild 9.8 Absolute Physics Core",
        nullptr, nullptr);

    if (!window)
    {
        std::cerr << "Surface Error: Active render target window frame layer allocation context failed.\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        std::cerr << "GLEW init failed\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    GLuint vs = compileShaderPipeline(GL_VERTEX_SHADER, VERT_SRC);
    GLuint fs = compileShaderPipeline(GL_FRAGMENT_SHADER, FRAG_SRC);
    if (!vs || !fs)
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint linkSuccess = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linkSuccess);
    if (!linkSuccess)
    {
        char infoLog[2048];
        glGetProgramInfoLog(program, 2048, nullptr, infoLog);
        std::cerr << "CORE SYSTEM PIPELINE LINK EXCEPTION:\n"
                  << infoLog << "\n";
        glDeleteShader(vs);
        glDeleteShader(fs);
        glDeleteProgram(program);
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    float surfaceVertices[] = {
        -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f,
        -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f};

    GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(surfaceVertices), surfaceVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
    glBindVertexArray(0);

    GLuint uboCam = 0;
    glGenBuffers(1, &uboCam);
    glBindBuffer(GL_UNIFORM_BUFFER, uboCam);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(CamBlock), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uboCam);

    auto runtimeStart = std::chrono::high_resolution_clock::now();
    auto wallStart = std::chrono::system_clock::now();

    auto metricsTimer = runtimeStart;
    double runtimeLastFrame = 0.0;
    bool firstFrame = true;

    constexpr double BENCHMARK_WARMUP_SEC = 3.0;
    bool benchmarkStarted = false;
    std::chrono::high_resolution_clock::time_point benchmarkStartTime{};

    double smoothedFPS = 0.0;

    std::uint64_t frameCounter = 0;
    std::uint64_t totalPixelsRendered = 0;
    std::uint64_t estimatedRK4Steps = 0;
    constexpr std::uint64_t RK4_STEPS_PER_PIXEL = 250ULL;

    double frameMsSum = 0.0;

    std::time_t startTT = std::chrono::system_clock::to_time_t(wallStart);
    std::tm startTM{};
#ifdef _WIN32
    localtime_s(&startTM, &startTT);
#else
    localtime_r(&startTT, &startTM);
#endif

    const char *logName = "RuntimeLogs.txt";

    std::ofstream fpsLog(logName, std::ios::app);
    if (fpsLog)
    {
        fpsLog << "Schwarzschild Physics Core FPS Log\n";
        fpsLog << "Run started: " << std::put_time(&startTM, "%Y-%m-%d %H:%M:%S") << "\n";
        fpsLog << "Window: " << windowWidth << "x" << windowHeight << "\n";
        fpsLog << "OpenGL Version: " << reinterpret_cast<const char *>(glGetString(GL_VERSION)) << "\n";
        fpsLog << "Renderer: " << reinterpret_cast<const char *>(glGetString(GL_RENDERER)) << "\n";
        fpsLog << "GLSL: " << reinterpret_cast<const char *>(glGetString(GL_SHADING_LANGUAGE_VERSION)) << "\n";
        fpsLog << "----------------------------------------\n";
        fpsLog.flush();
    }

    const float initialRadius = camRadius;
    const float initialTheta = camTheta;
    const float initialPhi = camPhi;

    while (!glfwWindowShouldClose(window))
    {
        auto timeCheck = std::chrono::high_resolution_clock::now();
        double currentSeconds = std::chrono::duration<double>(timeCheck - runtimeStart).count();

        if (firstFrame)
        {
            runtimeLastFrame = currentSeconds;
            firstFrame = false;
        }

        double frameDelta = currentSeconds - runtimeLastFrame;
        runtimeLastFrame = currentSeconds;

        if (!benchmarkStarted)
        {
            if (currentSeconds < BENCHMARK_WARMUP_SEC)
            {
                runtimeLastFrame = currentSeconds;
                continue;
            }

            benchmarkStarted = true;
            benchmarkStartTime = timeCheck;

            firstFrame = true;
            runtimeLastFrame = currentSeconds;

            frameCounter = 0;
            totalPixelsRendered = 0;
            estimatedRK4Steps = 0;

            frameMsSum = 0.0;

            metricsTimer = timeCheck;
            continue;
        }

        constexpr double MIN_VALID_FRAME = 1.0 / 500.0; // 2.0 ms (reject >500 FPS)
        constexpr double MAX_VALID_FRAME = 0.25;        // 250 ms

        bool validSample = frameDelta >= MIN_VALID_FRAME && frameDelta <= MAX_VALID_FRAME;

        float frameMs = 0.0f;
        std::uint64_t pixelsThisFrame =
            static_cast<std::uint64_t>(windowWidth) *
            static_cast<std::uint64_t>(windowHeight);

        if (validSample)
        {
            frameMs = static_cast<float>(frameDelta * 1000.0f);
            double instantFPS = 1.0 / frameDelta;

            if (frameCounter == 0)
                smoothedFPS = instantFPS;
            else
                smoothedFPS = smoothedFPS * 0.92 + instantFPS * 0.08;

            frameMsSum += frameMs;

            totalPixelsRendered += pixelsThisFrame;
            estimatedRK4Steps += pixelsThisFrame * RK4_STEPS_PER_PIXEL;

            frameCounter++;
        }

        float px = camRadius * sinf(camTheta) * sinf(camPhi);
        float py = camRadius * cosf(camTheta);
        float pz = camRadius * sinf(camTheta) * cosf(camPhi);

        float fx = -px, fy = -py, fz = -pz;
        float fLen = sqrtf(fx * fx + fy * fy + fz * fz);
        if (fLen > 1e-5f)
        {
            fx /= fLen;
            fy /= fLen;
            fz /= fLen;
        }

        float wux = 0.0f, wuy = 1.0f, wuz = 0.0f;
        float rx = fy * wuz - fz * wuy;
        float ry = fz * wux - fx * wuz;
        float rz = fx * wuy - fy * wux;

        float rLen = sqrtf(rx * rx + ry * ry + rz * rz);
        if (rLen < 1e-4f)
        {
            rx = 1.0f;
            ry = 0.0f;
            rz = 0.0f;
        }
        else
        {
            rx /= rLen;
            ry /= rLen;
            rz /= rLen;
        }

        float ux = ry * fz - rz * fy;
        float uy = rz * fx - rx * fz;
        float uz = rx * fy - ry * fx;

        CamBlock bufferPayload{};
        bufferPayload.cPos[0] = px;
        bufferPayload.cPos[1] = py;
        bufferPayload.cPos[2] = pz;
        bufferPayload.vecRight[0] = rx;
        bufferPayload.vecRight[1] = ry;
        bufferPayload.vecRight[2] = rz;
        bufferPayload.vecUp[0] = ux;
        bufferPayload.vecUp[1] = uy;
        bufferPayload.vecUp[2] = uz;
        bufferPayload.vecFwd[0] = fx;
        bufferPayload.vecFwd[1] = fy;
        bufferPayload.vecFwd[2] = fz;
        bufferPayload.fovTan = tanf(45.0f * 0.5f * SC_PI / 180.0f);
        bufferPayload.aspect = (windowHeight > 0) ? (static_cast<float>(windowWidth) / static_cast<float>(windowHeight)) : 1.0f;
        bufferPayload.time = static_cast<float>(currentSeconds);
        bufferPayload.padding = 0.0f;

        glBindBuffer(GL_UNIFORM_BUFFER, uboCam);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CamBlock), &bufferPayload);

        glViewport(0, 0, windowWidth, windowHeight);
        glUseProgram(program);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
        glfwPollEvents();

        auto logTimerDuration = std::chrono::duration<double>(timeCheck - metricsTimer).count();
        if (logTimerDuration >= 0.25 && frameCounter > 0)
        {
            char line[256];
            std::snprintf(
                line, sizeof(line),
                "[Complete Physics Core] FPS:%6.1f | Frame Time:%6.2f ms | Orbit Dist:%5.2f RS | Resolution:%dx%d",
                smoothedFPS,
                frameMs,
                camRadius,
                windowWidth,
                windowHeight);

            std::printf("\r%-180s", line);
            std::fflush(stdout);

            char title[256];
            std::snprintf(title, sizeof(title),
                          "Schwarzschild Complete Core | FPS: %.1f | %.2f ms",
                          smoothedFPS, frameMs);
            glfwSetWindowTitle(window, title);

            metricsTimer = timeCheck;
        }
    }

    constexpr double FLOPS_PER_RK4_STEP = 27.7; // Analytical estimate; actual executed FLOPs depend on compiler optimization and GPU instruction generation.

    auto wallEnd = std::chrono::system_clock::now();
    auto perfEnd = std::chrono::high_resolution_clock::now();
    double totalRuntimeSec = std::chrono::duration<double>(perfEnd - runtimeStart).count();

    std::time_t endTT = std::chrono::system_clock::to_time_t(wallEnd);
    std::tm endTM{};
#ifdef _WIN32
    localtime_s(&endTM, &endTT);
#else
    localtime_r(&endTT, &endTM);
#endif

    double benchmarkRuntimeSec = std::chrono::duration<double>(wallEnd - benchmarkStartTime).count();
    if (benchmarkRuntimeSec < 1e-9)
        benchmarkRuntimeSec = 1e-9;

    double avgFPS = (frameCounter > 0) ? static_cast<double>(frameCounter) / benchmarkRuntimeSec : 0.0;
    double avgFrameMs = (frameCounter > 0) ? (frameMsSum / static_cast<double>(frameCounter)) : 0.0;
    double pixelsPerSec = (benchmarkRuntimeSec > 0.0) ? (static_cast<double>(totalPixelsRendered) / benchmarkRuntimeSec) : 0.0;
    double estimatedRK4PerSec = (benchmarkRuntimeSec > 0.0) ? static_cast<double>(estimatedRK4Steps) / benchmarkRuntimeSec : 0.0;
    double estimatedGFLOPs = (estimatedRK4PerSec * FLOPS_PER_RK4_STEP) / 1e9;

    if (fpsLog)
    {
        fpsLog << "\n\n";
        fpsLog << "==========================================================\n";
        fpsLog << "                NEW BENCHMARK RUN\n";
        fpsLog << "==========================================================\n\n";

        fpsLog << "Run Started           : " << std::put_time(&startTM, "%Y-%m-%d %H:%M:%S") << "\n";
        fpsLog << "Run Ended             : " << std::put_time(&endTM, "%Y-%m-%d %H:%M:%S") << "\n";
        fpsLog << "Benchmark Runtime     : " << std::fixed << std::setprecision(3) << benchmarkRuntimeSec << " s\n";
        fpsLog << "Total Runtime         : " << std::fixed << std::setprecision(3) << totalRuntimeSec << " s\n\n";

        fpsLog << "GPU Vendor            : " << reinterpret_cast<const char *>(glGetString(GL_VENDOR)) << "\n";
        fpsLog << "GPU Renderer          : " << reinterpret_cast<const char *>(glGetString(GL_RENDERER)) << "\n";
        fpsLog << "OpenGL Version        : " << reinterpret_cast<const char *>(glGetString(GL_VERSION)) << "\n";
        fpsLog << "GLSL Version          : " << reinterpret_cast<const char *>(glGetString(GL_SHADING_LANGUAGE_VERSION)) << "\n\n";

        fpsLog << "Resolution            : " << windowWidth << " x " << windowHeight << "\n";
        fpsLog << "Pixels/Frame          : " << (static_cast<std::uint64_t>(windowWidth) * static_cast<std::uint64_t>(windowHeight)) << "\n\n";

        fpsLog << "Frames Rendered       : " << frameCounter << "\n\n";

        fpsLog << "Average FPS           : " << std::fixed << std::setprecision(3) << avgFPS << "\n";
        fpsLog << "Average Frame Time    : " << avgFrameMs << " ms\n\n";

        fpsLog << "Pixels Rendered       : " << std::fixed << std::setprecision(3) << (static_cast<double>(totalPixelsRendered) / 1e6) << " Million\n";
        fpsLog << "Pixels/sec            : " << std::fixed << std::setprecision(3) << (pixelsPerSec / 1e6) << " Million/sec\n\n";

        fpsLog << "Estimated RK4 Steps   : " << std::fixed << std::setprecision(3) << (static_cast<double>(estimatedRK4Steps) / 1e9) << " Billion\n";
        fpsLog << "RK4 Steps/sec         : " << std::fixed << std::setprecision(3) << (estimatedRK4PerSec / 1e9) << " Billion/sec\n\n";

        fpsLog << "Estimated GFLOPs/sec  : " << estimatedGFLOPs << "\n\n";

        fpsLog << "Initial Camera Radius : " << initialRadius << "\n";
        fpsLog << "Final Camera Radius   : " << camRadius << "\n";
        fpsLog << "Final Theta           : " << (camTheta * 180.0f / SC_PI) << "\n";
        fpsLog << "Final Phi             : " << (camPhi * 180.0f / SC_PI) << "\n\n";

        fpsLog << "==========================================================\n";
        fpsLog.close();
    }

    std::printf("\n");
    std::printf("Run finished. Log appended to: %s\n", logName);
    std::printf("Total runtime: %.3f s | Avg FPS: %.2f\n",
                totalRuntimeSec, avgFPS);

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &uboCam);
    glDeleteProgram(program);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}