#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace glm;

// ----------------------------------------------------------------------------
// Config
// ----------------------------------------------------------------------------
static int gW = 1280;
static int gH = 720;
static bool gDirtyCamera = true;
static bool gDirtyScale = true;
static double gLastMoveT = -99.0;

struct Camera
{
    float r = 15.0f;
    float theta = float(M_PI) * 0.38f;
    float phi = 0.0f;
    double lx = 0.0, ly = 0.0;
    bool drag = false;
    double lastMoveT = -99.0;

    vec3 pos() const
    {
        return vec3(r * sinf(theta) * cosf(phi),
                    r * cosf(theta),
                    r * sinf(theta) * sinf(phi));
    }
    vec3 fwd() const { return normalize(-pos()); }
    vec3 right() const { return normalize(cross(fwd(), vec3(0, 1, 0))); }
    vec3 up() const { return normalize(cross(right(), fwd())); }
    float fovTan() const { return tanf(radians(65.0f) * 0.5f); }
    bool moving(double now) const { return now - lastMoveT < 0.20; }
} gCam;

// ----------------------------------------------------------------------------
// GL resources
// ----------------------------------------------------------------------------
static GLuint gVAO = 0, gVBO = 0;
static GLuint gUBO = 0;
static GLuint gProg = 0;
static GLuint gFBO = 0, gFBOtex = 0;
static int gFBOw = 0, gFBOh = 0;

// ----------------------------------------------------------------------------
// Shader sources
// ----------------------------------------------------------------------------
static const char *VERT_SRC = R"GLSL(
#version 430 core
layout(location=0) in vec2 aPos;
out vec2 vUV;
void main(){
    vUV = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)GLSL";

static const char *FRAG_SRC = R"GLSL(
#version 430 core
in vec2 vUV;
out vec4 FragColor;

layout(std140, binding=0) uniform CamBlock {
    vec4 cPos;
    vec4 cRight;
    vec4 cUp;
    vec4 cFwd;
    float fovTan;
    float aspect;
    float time;
    float _pad;
};

const float RS = 1.0;
const float ISCO = 3.0;
const float DOUT = 12.0;
const float ESCR = 120.0;
const float B2CRIT = 6.75;
const int   NSTEP = 200;
const float BASE_DL = 0.52;

float h3(vec3 p){
    p = fract(p * vec3(443.897, 441.423, 437.195));
    p += dot(p, p.zyx + 19.19);
    return fract((p.x + p.y) * p.z);
}

vec3 ACES(vec3 x){
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

vec3 starColor(float t){
    t = clamp(t, 0.0, 1.0);
    const vec3 M = vec3(1.00, 0.55, 0.25);
    const vec3 K = vec3(1.00, 0.77, 0.48);
    const vec3 G = vec3(1.00, 0.95, 0.80);
    const vec3 F = vec3(0.98, 0.98, 0.95);
    const vec3 A = vec3(0.88, 0.92, 1.00);
    const vec3 B = vec3(0.70, 0.80, 1.00);

    float w0 = clamp((0.17 - t) * 5.88, 0.0, 1.0);
    float w1 = clamp((0.33 - t) * 6.25, 0.0, 1.0);
    float w2 = clamp((0.50 - t) * 5.88, 0.0, 1.0);
    float w3 = clamp((0.67 - t) * 5.88, 0.0, 1.0);
    float w4 = clamp((0.83 - t) * 6.25, 0.0, 1.0);

    vec3 col = mix(B, A, w4);
    col = mix(col, F, w3);
    col = mix(col, G, w2);
    col = mix(col, K, w1);
    col = mix(col, M, w0);
    return col;
}

vec3 stars(vec3 dir){
    vec3 col = vec3(0.0);
    float s = 40.0;
    for(int k = 0; k < 5; ++k){
        vec3 q = floor(dir * s);
        float hv = h3(q + float(k) * 7.3);
        float bright = step(0.9965, hv) * (hv - 0.9965) * 280.0;
        bright *= bright;
        vec3 sc = starColor(h3(q + float(k) * 91.3));
        col += sc * bright;
        s *= 2.6;
    }
    return col;
}

vec3 sky(vec3 dir){
    const vec3 GN = vec3(0.1946, -0.2458, 0.9494);
    const vec3 GC = vec3(-0.0550, -0.8734, 0.4840);

    float lat = dot(dir, GN);
    float cAng = dot(dir, GC);

    float gTight = max(0.0, 1.0 - lat * lat * 48.0);
    float gWide  = max(0.0, 1.0 - lat * lat *  6.5);
    gTight *= gTight;
    gWide  *= gWide;

    float coreDist = 1.0 - cAng;
    float gCore = max(0.0, 1.0 - coreDist * coreDist * 4.5) * gTight;

    float dust1 = h3(floor(dir * 12.0)) * 0.12;
    float dust2 = h3(floor(dir * 28.0)) * 0.06;
    float dust = 0.82 + dust1 + dust2;

    vec3 c = vec3(0.003, 0.004, 0.007)
           + vec3(0.24, 0.20, 0.14) * gTight * 0.55
           + vec3(0.18, 0.15, 0.10) * gWide  * 0.10
           + vec3(0.40, 0.26, 0.12) * gCore  * 1.60;

    c *= dust;
    return c + stars(dir);
}

vec3 bbCol(float T){
    T = clamp(T, 0.0, 1.0);
    const vec3 c0 = vec3(0.90, 0.32, 0.05);
    const vec3 c1 = vec3(1.00, 0.65, 0.20);
    const vec3 c2 = vec3(1.00, 0.95, 0.80);
    const vec3 c3 = vec3(0.82, 0.90, 1.00);
    if(T < 0.33) return mix(c0, c1, T * 3.0);
    if(T < 0.66) return mix(c1, c2, (T - 0.33) * 3.03);
    return mix(c2, c3, (T - 0.66) * 3.03);
}

vec4 diskEmit(float r, vec3 er, vec3 ephi, float pr, float L, float cphi, float sphi){
    if(r < ISCO || r > DOUT) return vec4(0.0);

    float u = (DOUT - r) / (DOUT - ISCO);

    float F = (1.0 / (r * r * r)) * (1.0 - sqrt(3.0 / max(r, 3.0001)));
    float Tbase = 0.85 * sqrt(sqrt(max(F, 0.0)));

    float vk = sqrt(RS / max(2.0 * r - 3.0 * RS, 0.15));
    vk = clamp(vk, 0.0, 0.93);

    vec3 pos3 = r * er;
    float rxz2 = max(dot(pos3.xz, pos3.xz), 1e-10);
    vec3 vDir = vec3(-pos3.z, 0.0, pos3.x) * inversesqrt(rxz2);

    vec3 pDir = normalize(pr * er + (L / r) * ephi);
    float cosA = dot(pDir, vDir);

    float gam = inversesqrt(max(1.0 - vk * vk, 1e-5));
    float D = 1.0 / max(gam * (1.0 - vk * cosA), 0.02);
    float gRed = sqrt(max(1.0 - RS / r, 0.002));

    float Tobs = clamp(Tbase * D * gRed * 0.72, 0.0, 1.0);

    float D2 = D * D;
    float D3 = D2 * D;
    float T2 = Tbase * Tbase;
    float T3 = T2 * Tbase;
    float T35 = T3 * sqrt(max(Tbase, 0.0));
    float intense = D3 * gRed * T35 * 3.2;

    float dens = u * max(0.0, 1.0 - 0.75 * u);
    dens = clamp(dens * 2.5, 0.0, 1.0);

    float c2 = cphi * cphi - sphi * sphi;
    float s2 = 2.0 * cphi * sphi;
    float phaseR = u * 2.0;
    float arm = (c2 * cos(phaseR) + s2 * sin(phaseR)) * 0.10 + 0.90;

    vec3 col = bbCol(Tobs);
    return vec4(col * intense * dens * arm, dens * 0.85);
}

void main(){
    vec2 uv = vUV * 2.0 - 1.0;
    uv.x *= aspect;

    vec3 rayDir = normalize(cRight.xyz * (uv.x * fovTan)
                          + cUp.xyz    * (uv.y * fovTan)
                          + cFwd.xyz);

    vec3 cam = cPos.xyz;
    vec3 e1 = normalize(cam);
    vec3 Np = cross(cam, rayDir);
    float NpL = length(Np);
    vec3 e2;
    if(NpL < 1e-5)
        e2 = normalize(cross(e1, abs(e1.y) < 0.9 ? vec3(0,1,0) : vec3(1,0,0)));
    else
        e2 = normalize(cross(Np / NpL, e1));

    float r0 = length(cam);
    float pr = dot(rayDir, e1);
    float vt = dot(rayDir, e2);
    float L = r0 * vt;
    float L2 = L * L;

    float f0 = 1.0 - RS / r0;
    float vr2 = 1.0 - f0 * (L / r0) * (L / r0);
    if(vr2 < 0.0){ FragColor = vec4(0,0,0,1); return; }
    pr = sign(pr) * sqrt(vr2);
    float r = r0;

    float cphi = 1.0, sphi = 0.0;
    vec4 disk = vec4(0.0);
    bool absorbed = false;
    bool escaped = false;
    float y_prev = cam.y;

    for(int i = 0; i < NSTEP; ++i){
        if(absorbed || escaped) continue;

        if(r < RS * 1.005){ absorbed = true; continue; }
        if(r > ESCR){ escaped = true; continue; }
        if(r < 1.52 * RS && pr < 0.0 && L2 < B2CRIT + 0.12){ absorbed = true; continue; }

        float err = abs(pr * pr + (1.0 - RS / r) * (L2 / (r * r)) - 1.0);
        float dlAdjust = 1.0 / (1.0 + err * 100.0);

        float u_rs = RS / r;
        float dl = BASE_DL / (1.0 + 18.0 * u_rs / (1.0 - u_rs + 1e-5));
        float xps = (r - 1.5) / 1.5;
        float x2 = xps * xps;
        float psRefine = 1.0 + 15.0 / (1.0 + 25.0 * x2);
        dl /= psRefine;
        dl *= clamp(dlAdjust, 0.4, 1.0);

        float r_safe = max(r, 0.1);
        float k1r = pr;
        float k1p = L2 / (r_safe * r_safe * r_safe) * (1.0 - 1.5 / r_safe);

        float r2 = max(r + 0.5 * dl * k1r, 0.1);
        float pr2 = pr + 0.5 * dl * k1p;
        float k2p = L2 / (r2 * r2 * r2) * (1.0 - 1.5 / r2);

        float r3 = max(r + 0.5 * dl * pr2, 0.1);
        float pr3 = pr + 0.5 * dl * k2p;
        float k3p = L2 / (r3 * r3 * r3) * (1.0 - 1.5 / r3);

        float r4 = max(r + dl * pr3, 0.1);
        float pr4 = pr + dl * k3p;
        float k4p = L2 / (r4 * r4 * r4) * (1.0 - 1.5 / r4);

        float w = dl * (1.0 / 6.0);
        r  += w * (k1r + 2.0 * pr2 + 2.0 * pr3 + pr4);
        pr += w * (k1p + 2.0 * k2p + 2.0 * k3p + k4p);

        float dphi = (L / (r * r)) * dl;
        float c_d, s_d;
        if(abs(dphi) < 0.09){
            float dp2 = dphi * dphi;
            c_d = 1.0 - dp2 * (0.5 - dp2 * (1.0 / 24.0));
            s_d = dphi * (1.0 - dp2 * (1.0 / 6.0 - dp2 * (1.0 / 120.0)));
        } else {
            c_d = cos(dphi);
            s_d = sin(dphi);
        }
        float nc = cphi * c_d - sphi * s_d;
        float ns = sphi * c_d + cphi * s_d;
        cphi = nc; sphi = ns;

        if((i & 31) == 0){
            float inv = inversesqrt(cphi * cphi + sphi * sphi);
            cphi *= inv; sphi *= inv;
        }

        if((i & 63) == 0 && r > 1.1 * RS){
            float fc = max(1.0 - RS / r, 1e-5);
            float vr2c = 1.0 - fc * (L / r) * (L / r);
            if(vr2c >= 0.0) pr = sign(pr) * sqrt(vr2c);
        }

        float disk_h = 0.04 * r;
        float y_cur = r * (cphi * e1.y + sphi * e2.y);

        if(abs(y_cur) < disk_h && disk.a < 0.97){
            if(r >= ISCO && r <= DOUT){
                float dist_plane = abs(y_cur) / disk_h;
                float vertical_attenuation = 1.0 - dist_plane * dist_plane;

                float tau = abs(y_prev) / max(abs(y_prev) + abs(y_cur), 1e-12);
                float r_mid = mix(r - dl * pr, r, tau);
                float cphi_mid = mix(cphi - dphi * sphi, cphi, tau);
                float sphi_mid = mix(sphi + dphi * cphi, sphi, tau);

                vec3 er_c = cphi_mid * e1 + sphi_mid * e2;
                vec3 ep_c = -sphi_mid * e1 + cphi_mid * e2;
                vec4 dc = diskEmit(r_mid, er_c, ep_c, pr, L, cphi_mid, sphi_mid);
                float step_absorb = dc.a * dl * vertical_attenuation * 2.2;

                disk.rgb += (1.0 - disk.a) * step_absorb * dc.rgb;
                disk.a   += (1.0 - disk.a) * step_absorb;
            }
        }
        y_prev = y_cur;
    }

    vec3 final;
    if(absorbed){
        final = vec3(0.0);
    } else {
        vec3 erf = cphi * e1 + sphi * e2;
        vec3 epf = -sphi * e1 + cphi * e2;
        vec3 rawEsc = pr * erf + (L / r) * epf;
        vec3 escDir = rawEsc * inversesqrt(max(dot(rawEsc, rawEsc), 1e-10));
        final = sky(escDir);
        float gb = 1.0 / sqrt(max(1.0 - RS / r0, 0.60));
        final *= min(gb, 1.35);
    }

    final = mix(final, disk.rgb, clamp(disk.a, 0.0, 1.0));
    final = ACES(final * 1.15);
    final = pow(max(final, vec3(0.0)), vec3(1.0 / 2.2));

    FragColor = vec4(final, 1.0);
}
)GLSL";

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------
static GLuint compileShader(GLenum type, const char *src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char buf[4096];
        glGetShaderInfoLog(s, 4096, nullptr, buf);
        std::cerr << (type == GL_VERTEX_SHADER ? "VS" : "FS") << " error:\n"
                  << buf << '\n';
        return 0;
    }
    return s;
}

static GLuint linkProg(GLuint vs, GLuint fs)
{
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char buf[4096];
        glGetProgramInfoLog(p, 4096, nullptr, buf);
        std::cerr << "Link error:\n"
                  << buf << '\n';
        return 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

struct alignas(16) CamUBO
{
    vec4 pos, right, up, fwd;
    float fovTan, aspect, time, pad;
};

static void ensureFBO(int w, int h)
{
    if (w == gFBOw && h == gFBOh)
        return;
    if (gFBOtex)
        glDeleteTextures(1, &gFBOtex);
    if (gFBO)
        glDeleteFramebuffers(1, &gFBO);
    gFBOtex = gFBO = 0;

    glGenTextures(1, &gFBOtex);
    glBindTexture(GL_TEXTURE_2D, gFBOtex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_HALF_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &gFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, gFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gFBOtex, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "FBO incomplete\n";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    gFBOw = w;
    gFBOh = h;
}

static void cbResize(GLFWwindow *, int w, int h)
{
    gW = std::max(1, w);
    gH = std::max(1, h);
    glViewport(0, 0, gW, gH);
    gDirtyCamera = true;
}

static void cbScroll(GLFWwindow *win, double, double dy)
{
    auto *c = static_cast<Camera *>(glfwGetWindowUserPointer(win));
    c->r = std::clamp(c->r * (dy > 0 ? 0.92f : 1.09f), 2.1f, 45.0f);
    c->lastMoveT = glfwGetTime();
}

static void cbMouse(GLFWwindow *win, int btn, int act, int)
{
    auto *c = static_cast<Camera *>(glfwGetWindowUserPointer(win));
    if (btn == GLFW_MOUSE_BUTTON_LEFT)
        c->drag = (act == GLFW_PRESS);
    if (act == GLFW_PRESS)
        glfwGetCursorPos(win, &c->lx, &c->ly);
}

static void cbCursor(GLFWwindow *win, double x, double y)
{
    auto *c = static_cast<Camera *>(glfwGetWindowUserPointer(win));
    if (c->drag)
    {
        c->phi -= float(x - c->lx) * 0.005f;
        c->theta = std::clamp(c->theta - float(y - c->ly) * 0.005f,
                              0.03f, float(M_PI) - 0.03f);
        c->lastMoveT = glfwGetTime();
    }
    c->lx = x;
    c->ly = y;
}

static void cbKey(GLFWwindow *win, int key, int, int act, int)
{
    if (act != GLFW_PRESS)
        return;
    auto *c = static_cast<Camera *>(glfwGetWindowUserPointer(win));
    if (key == GLFW_KEY_R)
    {
        c->r = 15.0f;
        c->theta = float(M_PI) * 0.38f;
        c->phi = 0.0f;
        c->lastMoveT = glfwGetTime();
    }
    if (key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(win, GLFW_TRUE);
}

// ----------------------------------------------------------------------------
// main
// ----------------------------------------------------------------------------
int main()
{
    std::puts("Schwarzschild Black Hole Simulator — OpenGL 4.3");
    std::puts("Controls: LMB drag=orbit  Scroll=zoom  R=reset  Esc=quit\n");

    if (!glfwInit())
    {
        std::fputs("GLFW init failed\n", stderr);
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *win = glfwCreateWindow(gW, gH, "Schwarzschild BH · OpenGL 4.3 · Quadro K2100M", nullptr, nullptr);
    if (!win)
    {
        std::fputs("Window creation failed\n", stderr);
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(win);
    glfwSwapInterval(0);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        std::fputs("GLEW init failed\n", stderr);
        glfwDestroyWindow(win);
        glfwTerminate();
        return 1;
    }

    std::printf("OpenGL  : %s\nRenderer: %s\n\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));

    glfwSetWindowUserPointer(win, &gCam);
    glfwSetFramebufferSizeCallback(win, cbResize);
    glfwSetScrollCallback(win, cbScroll);
    glfwSetMouseButtonCallback(win, cbMouse);
    glfwSetCursorPosCallback(win, cbCursor);
    glfwSetKeyCallback(win, cbKey);

    float quad[] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenVertexArrays(1, &gVAO);
    glGenBuffers(1, &gVBO);
    glBindVertexArray(gVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    glGenBuffers(1, &gUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, gUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(CamUBO), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, gUBO);

    GLuint vs = compileShader(GL_VERTEX_SHADER, VERT_SRC);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, FRAG_SRC);
    if (!vs || !fs)
    {
        glfwDestroyWindow(win);
        glfwTerminate();
        return 1;
    }
    gProg = linkProg(vs, fs);
    if (!gProg)
    {
        glfwDestroyWindow(win);
        glfwTerminate();
        return 1;
    }

    double fpsT = glfwGetTime();
    int fpsN = 0;

    while (!glfwWindowShouldClose(win))
    {
        double t = glfwGetTime();
        bool moving = gCam.moving(t);

        float scale = moving ? 0.5f : 1.0f;
        int rW = std::max(1, int(gW * scale));
        int rH = std::max(1, int(gH * scale));
        ensureFBO(rW, rH);

        CamUBO u{};
        u.pos = vec4(gCam.pos(), 0.0f);
        u.right = vec4(gCam.right(), 0.0f);
        u.up = vec4(gCam.up(), 0.0f);
        u.fwd = vec4(gCam.fwd(), 0.0f);
        u.fovTan = gCam.fovTan();
        u.aspect = float(rW) / float(rH);
        u.time = float(t);
        u.pad = 0.0f;
        glBindBuffer(GL_UNIFORM_BUFFER, gUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CamUBO), &u);

        glBindFramebuffer(GL_FRAMEBUFFER, gFBO);
        glViewport(0, 0, rW, rH);
        glUseProgram(gProg);
        glBindVertexArray(gVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, gFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glViewport(0, 0, gW, gH);
        glBlitFramebuffer(0, 0, rW, rH, 0, 0, gW, gH, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glfwSwapBuffers(win);
        glfwPollEvents();

        fpsN++;
        if (t - fpsT >= 1.0)
        {
            std::printf("\rFPS:%3d  r=%.1frs  θ=%.0f°  %s      ",
                        fpsN, gCam.r,
                        gCam.theta * 180.0f / float(M_PI),
                        moving ? "[½ res]" : "[full ]");
            std::fflush(stdout);
            fpsN = 0;
            fpsT = t;
        }
    }

    if (gFBOtex)
        glDeleteTextures(1, &gFBOtex);
    if (gFBO)
        glDeleteFramebuffers(1, &gFBO);
    glDeleteProgram(gProg);
    glDeleteBuffers(1, &gUBO);
    glDeleteBuffers(1, &gVBO);
    glDeleteVertexArrays(1, &gVAO);

    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
