#include "SDL_main.h"
#include "SDL.h"
#include "SDL_image.h"

#include "di_gl_header.h"
#include "DiResource.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>

#include <signal.h>

#ifdef WIN32
#include <xmmintrin.h>
#else
#include <arm_neon.h>
#endif

using namespace di;

#define LOGI(...) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define LOGE(...) SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)

static void printGLString(const char *name, GLenum s) {
    const char *v = (const char *) glGetString(s);
    LOGI("GL %s = %s\n", name, v);
}

static void printGLInteger(const char* name, GLenum s) {
    GLint i;
    glGetIntegerv(s, &i);
    LOGI("%s = %d\n", name, i);
}

static void checkGlError(const char* op) {
    for (GLint error = glGetError(); error; error
            = glGetError()) {
        LOGE("after %s() glError (0x%x)\n", op, error);
    }
}

static const char gVertexShader[] =
#ifdef WIN32
    "#define highp\n"
    "#define mediump\n"
    "#define lowp\n"
#endif
    "attribute vec4 vPosition;\n"
    "varying highp vec2 vTexcoord;\n"
    "void main() {\n"
    // "  vTexcoord = vPosition.xy * 0.5 + 0.5;\n"
    "  vTexcoord = vec2(vPosition.x * 0.5 + 0.5, 0.5 - vPosition.y * 0.5);\n"
    "  gl_Position = vPosition;\n"
    "}\n";

static const char gFragmentShader[] =
#ifdef WIN32
    "#define highp\n"
    "#define mediump\n"
    "#define lowp\n"
#endif
    "varying highp vec2 vTexcoord;\n"
    "uniform lowp sampler2D tex;\n"
    // "precision mediump float;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(tex, vTexcoord);\n"
	// "  gl_FragColor = texture2D(tex, vec2(0.5, 0.5));\n"
    // "  gl_FragColor = vec4(vTexcoord, 0.0, 1.0);\n"
    // "  gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
    "}\n";

GLuint loadShader(GLenum shaderType, const char* pSource) {
    DI_SAVE_CALLSTACK();

    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char* buf = (char*) malloc(infoLen);
                if (buf) {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    LOGE("Could not compile shader %d:\n%s\n",
                            shaderType, buf);
                    free(buf);
                }
                glDeleteShader(shader);
                shader = 0;
            }
        }
    }
    return shader;
}

GLuint createProgram(const char* pVertexSource, const char* pFragmentSource) {
    DI_SAVE_CALLSTACK();

    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        return 0;
    }

    GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader) {
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        checkGlError("glAttachShader");
        glAttachShader(program, pixelShader);
        checkGlError("glAttachShader");
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char* buf = (char*) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(program, bufLength, NULL, buf);
                    LOGE("Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }
    return program;
}


GLuint gProgram;
GLuint gvPositionHandle;

bool setupGraphics(int w, int h) {
    DI_SAVE_CALLSTACK();

    printGLString("Version", GL_VERSION);
    printGLString("Vendor", GL_VENDOR);
    printGLString("Renderer", GL_RENDERER);
    printGLString("Extensions", GL_EXTENSIONS);

    printGLInteger("GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS", GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
    printGLInteger("GL_MAX_CUBE_MAP_TEXTURE_SIZE", GL_MAX_CUBE_MAP_TEXTURE_SIZE);
    printGLInteger("GL_MAX_FRAGMENT_UNIFORM_VECTORS", GL_MAX_FRAGMENT_UNIFORM_VECTORS);
    printGLInteger("GL_MAX_RENDERBUFFER_SIZE", GL_MAX_RENDERBUFFER_SIZE);
    printGLInteger("GL_MAX_TEXTURE_IMAGE_UNITS", GL_MAX_TEXTURE_IMAGE_UNITS);
    printGLInteger("GL_MAX_TEXTURE_SIZE", GL_MAX_TEXTURE_SIZE);
    printGLInteger("GL_MAX_VARYING_VECTORS", GL_MAX_VARYING_VECTORS);
    printGLInteger("GL_MAX_VERTEX_ATTRIBS", GL_MAX_VERTEX_ATTRIBS);
    printGLInteger("GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS", GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS);
    printGLInteger("GL_MAX_VERTEX_UNIFORM_VECTORS", GL_MAX_VERTEX_UNIFORM_VECTORS);
    // printGLInteger("GL_MAX_VIEWPORT_DIMS", GL_MAX_VIEWPORT_DIMS);
    printGLInteger("GL_NUM_COMPRESSED_TEXTURE_FORMATS", GL_NUM_COMPRESSED_TEXTURE_FORMATS);
    // printGLInteger("GL_NUM_SHADER_BINARY_FORMATS", GL_NUM_SHADER_BINARY_FORMATS);
    // printGLInteger("GL_NUM_PROGRAM_BINARY_FORMATS", GL_NUM_PROGRAM_BINARY_FORMATS);

    GLint maxVertexAttribs = 0;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
    if (maxVertexAttribs < 16)
    {
        LOGI("maxVertexAttribs = %d is less than 16\n", maxVertexAttribs);
        abort();
    }

    LOGI("setupGraphics(%d, %d)", w, h);
    gProgram = createProgram(gVertexShader, gFragmentShader);
    if (!gProgram) {
        LOGE("Could not create program.");
        return false;
    }
    gvPositionHandle = glGetAttribLocation(gProgram, "vPosition");
    checkGlError("glGetAttribLocation");
    LOGI("glGetAttribLocation(\"vPosition\") = %d\n",
            gvPositionHandle);

    glViewport(0, 0, w, h);
    checkGlError("glViewport");
    return true;
}

// const GLfloat gTriangleVertices[] = { 0.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f };
const GLfloat gTriangleVertices[] = { -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f };



Vec3 matrix_transform_simple(const di::Mat4& m, const di::Vec3& v)
{
    return di::Vec3{
        v[0] * m.m_data[0] + v[1] * m.m_data[4] + v[2] * m.m_data[8] + m.m_data[12],
        v[0] * m.m_data[1] + v[1] * m.m_data[5] + v[2] * m.m_data[9] + m.m_data[13],
        v[0] * m.m_data[2] + v[1] * m.m_data[6] + v[2] * m.m_data[10] + m.m_data[14],
    };
}

#ifdef WIN32
Vec3 matrix_transform_sse(const di::Mat4& m, const di::Vec3& v)
{
    __m128 mm[4];
    memcpy(mm[0].m128_f32, m.m_data + 0, 16);
    memcpy(mm[1].m128_f32, m.m_data + 4, 16);
    memcpy(mm[2].m128_f32, m.m_data + 8, 16);
    memcpy(mm[3].m128_f32, m.m_data + 12, 16);

    __m128 vv;
    vv.m128_f32[0] = v[0];
    vv.m128_f32[1] = v[1];
    vv.m128_f32[2] = v[2];
    vv.m128_f32[3] = 1.0f;

    __m128 col1 = _mm_shuffle_ps(vv, vv, _MM_SHUFFLE(0, 0, 0, 0));
    __m128 col2 = _mm_shuffle_ps(vv, vv, _MM_SHUFFLE(1, 1, 1, 1));
    __m128 col3 = _mm_shuffle_ps(vv, vv, _MM_SHUFFLE(2, 2, 2, 2));
    __m128 col4 = _mm_shuffle_ps(vv, vv, _MM_SHUFFLE(3, 3, 3, 3));

    __m128 ret = _mm_add_ps(
        _mm_add_ps(_mm_mul_ps(mm[0], col1), _mm_mul_ps(mm[1], col2)),
        _mm_add_ps(_mm_mul_ps(mm[2], col3), _mm_mul_ps(mm[3], col4))
        );

    return di::Vec3{ ret.m128_f32[0], ret.m128_f32[1], ret.m128_f32[2] };
}

void matrix_transform_sse_4x(const float* matrices,
                             const __m128& vx, const __m128& vy, const __m128& vz,
                             __m128* outVx, __m128* outVy, __m128* outVz)
{
    __m128 tmp1, tmp2;

    // component X
    tmp1 = _mm_load_ps(matrices);
    tmp1 = _mm_mul_ps(vx, tmp1);

    tmp2 = _mm_load_ps(matrices + 3 * 4);
    tmp2 = _mm_mul_ps(vy, tmp2);
    tmp1 = _mm_add_ps(tmp1, tmp2);

    tmp2 = _mm_load_ps(matrices + 6 * 4);
    tmp2 = _mm_mul_ps(vz, tmp2);
    tmp1 = _mm_add_ps(tmp1, tmp2);

    tmp2 = _mm_load_ps(matrices + 9 * 4);
    *outVx = _mm_add_ps(tmp1, tmp2);

    // component Y
    tmp1 = _mm_load_ps(matrices + 1 * 4);
    tmp1 = _mm_mul_ps(vx, tmp1);

    tmp2 = _mm_load_ps(matrices + 4 * 4);
    tmp2 = _mm_mul_ps(vy, tmp2);
    tmp1 = _mm_add_ps(tmp1, tmp2);

    tmp2 = _mm_load_ps(matrices + 7 * 4);
    tmp2 = _mm_mul_ps(vz, tmp2);
    tmp1 = _mm_add_ps(tmp1, tmp2);

    tmp2 = _mm_load_ps(matrices + 10 * 4);
    *outVy = _mm_add_ps(tmp1, tmp2);

    // component Z
    tmp1 = _mm_load_ps(matrices + 2 * 4);
    tmp1 = _mm_mul_ps(vx, tmp1);

    tmp2 = _mm_load_ps(matrices + 5 * 4);
    tmp2 = _mm_mul_ps(vy, tmp2);
    tmp1 = _mm_add_ps(tmp1, tmp2);

    tmp2 = _mm_load_ps(matrices + 8 * 4);
    tmp2 = _mm_mul_ps(vz, tmp2);
    tmp1 = _mm_add_ps(tmp1, tmp2);

    tmp2 = _mm_load_ps(matrices + 11 * 4);
    *outVz = _mm_add_ps(tmp1, tmp2);
}
#else
// android neon
#include <cpu-features.h>
void matrix_transform_neon_impl(const float* m, float x, float y, float z, float w, float* dst)
{
    asm volatile(
                 "vld1.32    {d0[0]},        [%1]    \n\t"    // V[x]
                 "vld1.32    {d0[1]},        [%2]    \n\t"    // V[y]
                 "vld1.32    {d1[0]},        [%3]    \n\t"    // V[z]
                 "vld1.32    {d1[1]},        [%4]    \n\t"    // V[w]
                 "vld1.32    {d18 - d21},    [%5]!   \n\t"    // M[m0-m7]
                 "vld1.32    {d22 - d25},    [%5]    \n\t"    // M[m8-m15]
                 
                 "vmul.f32 q13,  q9, d0[0]           \n\t"    // DST->V = M[m0-m3] * V[x]
                 "vmla.f32 q13, q10, d0[1]           \n\t"    // DST->V += M[m4-m7] * V[y]
                 "vmla.f32 q13, q11, d1[0]           \n\t"    // DST->V += M[m8-m11] * V[z]
                 "vmla.f32 q13, q12, d1[1]           \n\t"    // DST->V += M[m12-m15] * V[w]
                 
                 "vst1.32 {d26}, [%0]!               \n\t"    // DST->V[x, y]
                 "vst1.32 {d27[0]}, [%0]             \n\t"    // DST->V[z]
                 :
                 : "r"(dst), "r"(&x), "r"(&y), "r"(&z), "r"(&w), "r"(m)
                 : "q0", "q9", "q10","q11", "q12", "q13", "memory"
                 );
}

Vec3 matrix_transform_neon(const di::Mat4& m, const di::Vec3& v)
{
    float ret[4];
    matrix_transform_neon_impl(m.m_data, v[0], v[1], v[2], 1.0f, ret);
    return Vec3{ret[0], ret[1], ret[2]};
}


void matrix_transform_neon_4x(const float* matrices,
    const float32x4_t& vx, const float32x4_t& vy, const float32x4_t& vz,
    float32x4_t* outVx, float32x4_t* outVy, float32x4_t* outVz)
{
    float32x4_t tmp1, tmp2;

#define _mm_load_ps vld1q_f32
#define _mm_mul_ps vmulq_f32
#define _mm_add_ps vaddq_f32

    // component X
    tmp1 = _mm_load_ps(matrices);
    tmp1 = _mm_mul_ps(vx, tmp1);

    tmp2 = _mm_load_ps(matrices + 3 * 4);
    tmp2 = _mm_mul_ps(vy, tmp2);
    tmp1 = _mm_add_ps(tmp1, tmp2);

    tmp2 = _mm_load_ps(matrices + 6 * 4);
    tmp2 = _mm_mul_ps(vz, tmp2);
    tmp1 = _mm_add_ps(tmp1, tmp2);

    tmp2 = _mm_load_ps(matrices + 9 * 4);
    *outVx = _mm_add_ps(tmp1, tmp2);

    // component Y
    tmp1 = _mm_load_ps(matrices + 1 * 4);
    tmp1 = _mm_mul_ps(vx, tmp1);

    tmp2 = _mm_load_ps(matrices + 4 * 4);
    tmp2 = _mm_mul_ps(vy, tmp2);
    tmp1 = _mm_add_ps(tmp1, tmp2);

    tmp2 = _mm_load_ps(matrices + 7 * 4);
    tmp2 = _mm_mul_ps(vz, tmp2);
    tmp1 = _mm_add_ps(tmp1, tmp2);

    tmp2 = _mm_load_ps(matrices + 10 * 4);
    *outVy = _mm_add_ps(tmp1, tmp2);

    // component Z
    tmp1 = _mm_load_ps(matrices + 2 * 4);
    tmp1 = _mm_mul_ps(vx, tmp1);

    tmp2 = _mm_load_ps(matrices + 5 * 4);
    tmp2 = _mm_mul_ps(vy, tmp2);
    tmp1 = _mm_add_ps(tmp1, tmp2);

    tmp2 = _mm_load_ps(matrices + 8 * 4);
    tmp2 = _mm_mul_ps(vz, tmp2);
    tmp1 = _mm_add_ps(tmp1, tmp2);

    tmp2 = _mm_load_ps(matrices + 11 * 4);
    *outVz = _mm_add_ps(tmp1, tmp2);
}
#endif

void testMatrixPerformance()
{
    di::Mat4 m = {
        0.992813885f,
        0.0570071787f,
        0.105218470f,
        0.000000000f,
        -0.0518955290f,
        0.997364700f,
        -0.0506977588f,
        0.000000000f,
        -0.107831299f,
        0.0448730439f,
        0.993155956f,
        0.000000000f,
        24.9629993f,
        0.977380276f,
        18.1229401f,
        1.00000000f,
    };

    di::Vec3 v = { -6.74947977f, 16.9563999f, 1.47899997f };
    di::Vec3 vr = { 17.2225780f, 17.5706940f, 18.0219955f };

    static di::Vec3* v_from = nullptr;
    static di::Vec3* v_to = nullptr;

    const int COUNT = 300000;
    if (v_from == nullptr)
    {
        v_from = new di::Vec3[COUNT];
        v_to = new di::Vec3[COUNT];
        for (int i = 0; i < COUNT; ++i)
        {
            v_from[i] = v;
        }
    }

    Uint64 counter1 = SDL_GetPerformanceCounter();
    for (int i = 0; i < COUNT; ++i)
    {
        v_to[i] = di::matrix_transform(m, v_from[i]);
    }
    Uint64 counter2 = SDL_GetPerformanceCounter();
    LOGI("original matrix transform tooks %lld ms", (counter2 - counter1) * 1000 / SDL_GetPerformanceFrequency());
    LOGI("result [0]: %f, %f, %f", v_to[0][0], v_to[0][1], v_to[0][2]);
    LOGI("result [1]: %f, %f, %f", v_to[1][0], v_to[1][1], v_to[1][2]);
    LOGI("result [2]: %f, %f, %f", v_to[2][0], v_to[2][1], v_to[2][2]);

    counter1 = SDL_GetPerformanceCounter();
    for (int i = 0; i < COUNT; ++i)
    {
        v_to[i] = matrix_transform_simple(m, v_from[i]);
    }
    counter2 = SDL_GetPerformanceCounter();
    LOGI("simple matrix transform tooks %lld ms", (counter2 - counter1) * 1000 / SDL_GetPerformanceFrequency());
    LOGI("result [0]: %f, %f, %f", v_to[0][0], v_to[0][1], v_to[0][2]);
    LOGI("result [1]: %f, %f, %f", v_to[1][0], v_to[1][1], v_to[1][2]);
    LOGI("result [2]: %f, %f, %f", v_to[2][0], v_to[2][1], v_to[2][2]);

    counter1 = SDL_GetPerformanceCounter();
    for (int i = 0; i < COUNT; ++i)
    {
#ifdef WIN32
        v_to[i] = matrix_transform_sse(m, v_from[i]);
#else
        v_to[i] = matrix_transform_neon(m, v_from[i]);
#endif
    }
    counter2 = SDL_GetPerformanceCounter();
    LOGI("SIMD matrix transform tooks %lld ms", (counter2 - counter1) * 1000 / SDL_GetPerformanceFrequency());
    LOGI("result [0]: %f, %f, %f", v_to[0][0], v_to[0][1], v_to[0][2]);
    LOGI("result [1]: %f, %f, %f", v_to[1][0], v_to[1][1], v_to[1][2]);
    LOGI("result [2]: %f, %f, %f", v_to[2][0], v_to[2][1], v_to[2][2]);

        const Mat4& m1 = m;
        const Mat4& m2 = m;
        const Mat4& m3 = m;
        const Mat4& m4 = m;

    counter1 = SDL_GetPerformanceCounter();
    for (int i = 0; i + 4 <= COUNT; i += 4)
    {
#ifdef WIN32
        __m128 vx = _mm_set_ps(v_from[i][0], v_from[i+1][0], v_from[i+2][0], v_from[i+3][0]);
        __m128 vy = _mm_set_ps(v_from[i][1], v_from[i+1][1], v_from[i+2][1], v_from[i+3][1]);
        __m128 vz = _mm_set_ps(v_from[i][2], v_from[i+1][2], v_from[i+2][2], v_from[i+3][2]);

        _CRT_ALIGN(16) float matrices[12 * 4] = {
            m1.m_data[0], m2.m_data[0], m3.m_data[0], m4.m_data[0],
            m1.m_data[1], m2.m_data[1], m3.m_data[1], m4.m_data[1],
            m1.m_data[2], m2.m_data[2], m3.m_data[2], m4.m_data[2],
            m1.m_data[4], m2.m_data[4], m3.m_data[4], m4.m_data[4],
            m1.m_data[5], m2.m_data[5], m3.m_data[5], m4.m_data[5],
            m1.m_data[6], m2.m_data[6], m3.m_data[6], m4.m_data[6],
            m1.m_data[8], m2.m_data[8], m3.m_data[8], m4.m_data[8],
            m1.m_data[9], m2.m_data[9], m3.m_data[9], m4.m_data[9],
            m1.m_data[10], m2.m_data[10], m3.m_data[10], m4.m_data[10],
            m1.m_data[12], m2.m_data[12], m3.m_data[12], m4.m_data[12],
            m1.m_data[13], m2.m_data[13], m3.m_data[13], m4.m_data[13],
            m1.m_data[14], m2.m_data[14], m3.m_data[14], m4.m_data[14]
        };

        __m128 outVx, outVy, outVz;
        matrix_transform_sse_4x(matrices, vx, vy, vz, &outVx, &outVy, &outVz);
        v_to[i][0] = outVx.m128_f32[0];
        v_to[i+1][0] = outVx.m128_f32[1];
        v_to[i+2][0] = outVx.m128_f32[2];
        v_to[i+3][0] = outVx.m128_f32[3];
        v_to[i][1] = outVy.m128_f32[0];
        v_to[i+1][1] = outVy.m128_f32[1];
        v_to[i+2][1] = outVy.m128_f32[2];
        v_to[i+3][1] = outVy.m128_f32[3];
        v_to[i][2] = outVz.m128_f32[0];
        v_to[i+1][2] = outVz.m128_f32[1];
        v_to[i+2][2] = outVz.m128_f32[2];
        v_to[i+3][2] = outVz.m128_f32[3];
#else
        float vx_arr[] = {v_from[i][0], v_from[i+1][0], v_from[i+2][0], v_from[i+3][0]};
        float32x4_t vx = vld1q_f32(vx_arr);
        float vy_arr[] = {v_from[i][1], v_from[i+1][1], v_from[i+2][1], v_from[i+3][1]};
        float32x4_t vy = vld1q_f32(vy_arr);
        float vz_arr[] = {v_from[i][2], v_from[i+1][2], v_from[i+2][2], v_from[i+3][2]};
        float32x4_t vz = vld1q_f32(vz_arr);

        float matrices[12 * 4] = {
            m1.m_data[0], m2.m_data[0], m3.m_data[0], m4.m_data[0],
            m1.m_data[1], m2.m_data[1], m3.m_data[1], m4.m_data[1],
            m1.m_data[2], m2.m_data[2], m3.m_data[2], m4.m_data[2],
            m1.m_data[4], m2.m_data[4], m3.m_data[4], m4.m_data[4],
            m1.m_data[5], m2.m_data[5], m3.m_data[5], m4.m_data[5],
            m1.m_data[6], m2.m_data[6], m3.m_data[6], m4.m_data[6],
            m1.m_data[8], m2.m_data[8], m3.m_data[8], m4.m_data[8],
            m1.m_data[9], m2.m_data[9], m3.m_data[9], m4.m_data[9],
            m1.m_data[10], m2.m_data[10], m3.m_data[10], m4.m_data[10],
            m1.m_data[12], m2.m_data[12], m3.m_data[12], m4.m_data[12],
            m1.m_data[13], m2.m_data[13], m3.m_data[13], m4.m_data[13],
            m1.m_data[14], m2.m_data[14], m3.m_data[14], m4.m_data[14]
        };

        float32x4_t outVx, outVy, outVz;
        matrix_transform_neon_4x(matrices, vx, vy, vz, &outVx, &outVy, &outVz);
        vst1q_f32(vx_arr, outVx);
        vst1q_f32(vy_arr, outVy);
        vst1q_f32(vz_arr, outVz);
        v_to[i][0] = vx_arr[0];
        v_to[i+1][0] = vx_arr[1];
        v_to[i+2][0] = vx_arr[2];
        v_to[i+3][0] = vx_arr[3];
        v_to[i][1] = vy_arr[0];
        v_to[i+1][1] = vy_arr[1];
        v_to[i+2][1] = vy_arr[2];
        v_to[i+3][1] = vy_arr[3];
        v_to[i][2] = vz_arr[0];
        v_to[i+1][2] = vz_arr[1];
        v_to[i+2][2] = vz_arr[2];
        v_to[i+3][2] = vz_arr[3];
#endif
    }
    counter2 = SDL_GetPerformanceCounter();
    LOGI("SIMD(4x) matrix transform tooks %lld ms", (counter2 - counter1) * 1000 / SDL_GetPerformanceFrequency());
    LOGI("result [0]: %f, %f, %f", v_to[0][0], v_to[0][1], v_to[0][2]);
    LOGI("result [1]: %f, %f, %f", v_to[1][0], v_to[1][1], v_to[1][2]);
    LOGI("result [2]: %f, %f, %f", v_to[2][0], v_to[2][1], v_to[2][2]);
}


void renderFrame() {
    DI_SAVE_CALLSTACK();

	glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    checkGlError("glClearColor");
    glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    checkGlError("glClear");

    testMatrixPerformance();

#if 1
    // test resource load (some Android devices assert failed, don't know why)
    //                                                             ^--- possible reason: program exit when resource is loading
    ResourceManager::Singleton().CheckAsyncFinishedResources();

    // shared_ptr<ImageAsTexture> texture = ResourceManager::Singleton().GetResource<ImageAsTexture>("10001.ktx");
    shared_ptr<ImageAsTexture> texture = ResourceManager::Singleton().GetResource<ImageAsTexture>("main_bg.webp");
    // texture->SetTimeoutTicks(0);

    if (texture->IsResourceOK())
    {
        glUseProgram(gProgram);
        checkGlError("glUseProgram");

        glBindTexture(GL_TEXTURE_2D, texture->GetGlTexture());
        glVertexAttribPointer(gvPositionHandle, 2, GL_FLOAT, GL_FALSE, 0, gTriangleVertices);
        checkGlError("glVertexAttribPointer");
        glEnableVertexAttribArray(gvPositionHandle);
        checkGlError("glEnableVertexAttribArray");
        glDrawArrays(GL_TRIANGLES, 0, sizeof(gTriangleVertices) / sizeof(gTriangleVertices[0]) / 2);
        checkGlError("glDrawArrays");

        texture->UpdateTimeoutTick();
    }
#endif
}

void onSignal(int sig)
{
    LogError("onSignal %d", sig);
    FuncCallInfoStack::GetThreadStack().OutputToLog();

#ifdef _WIN32
    string title = String_Format("signal fault %d", sig);
    string message = FuncCallInfoStack::GetThreadStack().OutputToString();

    SDL_MessageBoxButtonData button;
    button.flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
    button.buttonid = 1;
    button.text = "OK I know";

    SDL_MessageBoxData msgboxData;
    msgboxData.flags = SDL_MESSAGEBOX_ERROR;
    msgboxData.window = nullptr;
    msgboxData.title = title.c_str();
    msgboxData.message = message.c_str();
    msgboxData.numbuttons = 1;
    msgboxData.buttons = &button;
    msgboxData.colorScheme = nullptr;

    int buttonId;
    SDL_ShowMessageBox(&msgboxData, &buttonId);
#endif

    exit(0);
}


#include <vector>
#include <string>

std::vector<std::string> performance_test_logs;


void TestImagePerformance(const char* filename, int times)
{
#ifdef WIN32
    Uint64 minTicks = UINT64_MAX;
#else
    Uint64 minTicks = ULONG_LONG_MAX;
#endif

    Uint64 maxTicks = 0;
    Uint64 totalTicks = 0;

    for (int i = 0; i < times; ++i)
    {
        Uint64 startTick = SDL_GetPerformanceCounter();
        SDL_Surface* surface = IMG_Load(filename);
        Uint64 ticks = SDL_GetPerformanceCounter() - startTick;
        if (!surface)
        {
            LOGE("failed loading '%s'", filename);
            return;
        }

        SDL_FreeSurface(surface);

        if (minTicks > ticks)
        {
            minTicks = ticks;
        }

        if (maxTicks < ticks)
        {
            maxTicks = ticks;
        }

        totalTicks += ticks;
    }

    Uint64 freq = SDL_GetPerformanceFrequency();

    performance_test_logs.push_back(String_Format("load '%s' %d times. min = %llu, max = %llu, avg = %llu",
            filename, times, minTicks * 1000 / freq, maxTicks * 1000 / freq, totalTicks * 1000 / times / freq));

    LOGI("load '%s' %d times. min = %llu, max = %llu, avg = %llu",
        filename, times, minTicks * 1000 / freq, maxTicks * 1000 / freq, totalTicks * 1000 / times / freq);
}


int main(int argc, char* argv[]) // the function 'main' is actually 'SDL_main'
{
#ifndef _DEBUG
    signal(SIGSEGV, onSignal);
#endif

    DI_SAVE_CALLSTACK();

	SDL_Init(SDL_INIT_EVERYTHING);
	LOGI("SDL_Init OK");

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	LOGI("SDL_GL_SetAttribute OK");

#ifdef WIN32
	SDL_Window* window = SDL_CreateWindow("gles study", 100, 100, 854, 480, SDL_WINDOW_OPENGL);
#else
    SDL_Window* window = SDL_CreateWindow("gles study", 0, 0, 0, 0, SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL);
#endif

	if (!window)
	{
		SDL_Log("create window failed: %s", SDL_GetError());
		abort();
	}

	LOGI("SDL_CreateWindow OK");

	SDL_GLContext context = SDL_GL_CreateContext(window);
	if (!context)
	{
		SDL_Log("create context failed: %s", SDL_GetError());
		abort();
	}

	LOGI("SDL_GL_CreateContext OK");

	SDL_GL_MakeCurrent(window, context);

#ifdef _WIN32
    glewInit();
#endif

	int width, height;
	SDL_GetWindowSize(window, &width, &height);
	setupGraphics(width, height);
	checkGlError("setupGraphics");

#if 0
    SDL_Delay(300);

    int times = 50;
    TestImagePerformance("main_bg.webp", times);
    TestImagePerformance("test_png.webp", times);
    TestImagePerformance("test_png.png", times);
    TestImagePerformance("test_jpg_full.webp", times);
    TestImagePerformance("test_jpg_full.jpg", times);
    TestImagePerformance("test_jpg_mid.webp", times);
    TestImagePerformance("test_jpg_80.jpg", times);
    TestImagePerformance("test_jpg_70.jpg", times);
    TestImagePerformance("test_jpg_60.jpg", times);
    TestImagePerformance("test_jpg_small.webp", times);
    TestImagePerformance("test_jpg_small.jpg", times);

    for (const std::string& log : performance_test_logs)
    {
    	LOGI("%s", log.c_str());
    }
#endif

	glUseProgram(gProgram);
	checkGlError("glUseProgram");

	GLint loc = glGetUniformLocation(gProgram, "tex");
	SDL_Log("loc = %d", loc);
	checkGlError("glGetUniformLocation1");

	glUniform1i(loc, 0);
	checkGlError("glUniform1i");

	SDL_Event ev;
	bool willQuit = false;
	while (!willQuit)
	{
		while (SDL_PollEvent(&ev))
		{
			if (ev.type == SDL_QUIT)
			{
                LogInfo("Event SDL_QUIT");
				willQuit = true;
                break;
			}
            else if (ev.type == SDL_APP_TERMINATING)
            {
                LogInfo("Event SDL_APP_TERMINATING");
				willQuit = true;
                break;
            }
            else if (ev.type == SDL_APP_WILLENTERBACKGROUND)
            {
                LogInfo("Event SDL_APP_WILLENTERBACKGROUND");
            }
            else if (ev.type == SDL_APP_DIDENTERBACKGROUND)
            {
                LogInfo("Event SDL_APP_DIDENTERBACKGROUND");
            }
            else if (ev.type == SDL_APP_WILLENTERFOREGROUND)
            {
                LogInfo("Event SDL_APP_WILLENTERFOREGROUND");
            }
            else if (ev.type == SDL_APP_DIDENTERFOREGROUND)
            {
                LogInfo("Event SDL_APP_DIDENTERFOREGROUND");
            }
            else if (ev.type == SDL_KEYUP)
            {
                LogInfo("Event SDL_KEYUP, scancode = %d", ev.key.keysym.scancode);
                if (ev.key.keysym.scancode == SDL_SCANCODE_AC_BACK)
                {
                    LogInfo("Back button pressed");
				    willQuit = true;
                    break;
                }
                else if (ev.key.keysym.scancode == SDL_SCANCODE_MENU)
                {
                    LogInfo("Menu button pressed");
                }
            }
		}

        if (willQuit)
        {
            break;
        }

        // glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
		// glClear(GL_COLOR_BUFFER_BIT);
		renderFrame();
		SDL_GL_SwapWindow(window);

		SDL_WaitEventTimeout(NULL, 1000 / 30);
	}

    ResourceManager::DestroySingleton();
    PerformanceProfileData::Singleton().OutputToLog();
    PerformanceProfileData::DestroySingleton();

    SDL_GL_MakeCurrent(nullptr, nullptr);
    SDL_GL_DeleteContext(context);
    context = nullptr;
    SDL_DestroyWindow(window);
    window = nullptr;

	SDL_Quit();

	return 0;
}
