#include "SDL_main.h"
#include "SDL.h"
#include "SDL_opengles2.h"

#include "ktx.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <jni.h>
#include <android/log.h>
#define  LOG_TAG    "libgl2jni"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

static void printGLString(const char *name, GLenum s) {
    const char *v = (const char *) glGetString(s);
    LOGI("GL %s = %s\n", name, v);
}

static void checkGlError(const char* op) {
    for (GLint error = glGetError(); error; error
            = glGetError()) {
        LOGE("after %s() glError (0x%x)\n", op, error);
    }
}

static const char gVertexShader[] =
    "attribute vec4 vPosition;\n"
    "varying highp vec2 vTexcoord;\n"
    "void main() {\n"
    // "  vTexcoord = vPosition.xy * 0.5 + 0.5;\n"
    "  vTexcoord = vec2(vPosition.x * 0.5 + 0.5, 0.5 - vPosition.y * 0.5);\n"
    "  gl_Position = vPosition;\n"
    "}\n";

static const char gFragmentShader[] =
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

GLuint loadKtxTexture(const char* filename, int* width = NULL, int* height = NULL)
{
	SDL_RWops* rw = SDL_RWFromFile(filename, "rb");
	if (!rw)
	{
		SDL_Log("loadKtxTexture '%s' failed. cannot open file", filename);
		return 0;
	}

	GLsizei size = SDL_RWsize(rw);

	void* mem = SDL_malloc(size);
	if (!size)
	{
		SDL_RWclose(rw);
		SDL_Log("loadKtxTexture '%s' failed. malloc failed", filename);
		return 0;
	}

	if (SDL_RWread(rw, mem, size, 1) != 1)
	{
		SDL_free(mem);
		SDL_RWclose(rw);
		SDL_Log("loadKtxTexture '%s' failed. read file failed", filename);
		return 0;
	}

	GLuint tex;
	GLenum target;
	GLenum glerr;
	GLboolean isMipmap;
	KTX_dimensions dimensions;
	KTX_error_code ktxErr = ktxLoadTextureM(mem, size, &tex, &target, &dimensions, &isMipmap, &glerr, 0, NULL);

	SDL_free(mem);
	mem = NULL;
	SDL_RWclose(rw);
	rw = NULL;

	if (ktxErr != KTX_SUCCESS || glerr != GL_NO_ERROR)
	{
		SDL_Log("loadKtxTexture '%s' failed. ktx error = %d, gl error = 0x%X", filename, ktxErr, glerr);
		return 0;
	}

	if (target != GL_TEXTURE_2D)
	{
		glBindTexture(target, 0);
		glDeleteTextures(1, &target);
		SDL_Log("loadKtxTexture '%s' failed. not GL_TEXTURE_2D", filename);
		return 0;
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, isMipmap ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	if (width)
	{
		*width = dimensions.width;
	}

	if (height)
	{
		*height = dimensions.height;
	}

	return tex;
}

GLuint gProgram;
GLuint gvPositionHandle;

bool setupGraphics(int w, int h) {
    printGLString("Version", GL_VERSION);
    printGLString("Vendor", GL_VENDOR);
    printGLString("Renderer", GL_RENDERER);
    printGLString("Extensions", GL_EXTENSIONS);

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

const GLfloat gTriangleVertices[] = { 0.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f };

void renderFrame() {
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    checkGlError("glClearColor");
    glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    checkGlError("glClear");

    glUseProgram(gProgram);
    checkGlError("glUseProgram");

    glVertexAttribPointer(gvPositionHandle, 2, GL_FLOAT, GL_FALSE, 0, gTriangleVertices);
    checkGlError("glVertexAttribPointer");
    glEnableVertexAttribArray(gvPositionHandle);
    checkGlError("glEnableVertexAttribArray");
    glDrawArrays(GL_TRIANGLES, 0, 3);
    checkGlError("glDrawArrays");
}

int main(int argc, char* argv[]) // the function 'main' is actually 'SDL_main'
{
	SDL_Init(SDL_INIT_EVERYTHING);

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

	SDL_Window* window = SDL_CreateWindow("gles study", 0, 0, 0, 0, SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL);
	if (!window)
	{
		SDL_Log("create window failed: %s", SDL_GetError());
		abort();
	}

	SDL_GLContext context = SDL_GL_CreateContext(window);
	if (!context)
	{
		SDL_Log("create context failed: %s", SDL_GetError());
		abort();
	}

	SDL_GL_MakeCurrent(window, context);

	int width, height;
	SDL_GetWindowSize(window, &width, &height);
	setupGraphics(width, height);
	checkGlError("setupGraphics");

	if (loadKtxTexture("10001.ktx") == 0)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "loadKtxTexture failed");
		abort();
	}

	SDL_Log("gProgram = %d", gProgram);

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
				willQuit = true;
			}
		}

		// glClear(GL_COLOR_BUFFER_BIT);
		renderFrame();
		SDL_GL_SwapWindow(window);
	}

	SDL_Quit();
	return 0;
}
