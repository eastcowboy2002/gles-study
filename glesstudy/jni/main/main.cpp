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

using namespace di;

#define LOGI(...) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define LOGE(...) SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)

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

void renderFrame() {
    DI_SAVE_CALLSTACK();

	glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    checkGlError("glClearColor");
    glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    checkGlError("glClear");

    ResourceManager::Singleton().CheckAsyncFinishedResources();

    // shared_ptr<ImageAsTexture> texture = ResourceManager::Singleton().GetResource<ImageAsTexture>("10001.ktx");
    shared_ptr<ImageAsTexture> texture = ResourceManager::Singleton().GetResource<ImageAsTexture>("main_bg.webp");
    // texture->SetTimeoutTicks(0);

    if (texture->IsResourceOK())
    {
        glUseProgram(gProgram);
        checkGlError("glUseProgram");

        glVertexAttribPointer(gvPositionHandle, 2, GL_FLOAT, GL_FALSE, 0, gTriangleVertices);
        checkGlError("glVertexAttribPointer");
        glEnableVertexAttribArray(gvPositionHandle);
        checkGlError("glEnableVertexAttribArray");
        glDrawArrays(GL_TRIANGLES, 0, sizeof(gTriangleVertices) / sizeof(gTriangleVertices[0]));
        checkGlError("glDrawArrays");

        texture->UpdateTimeoutTick();
    }

    ResourceManager::Singleton().CheckTimeoutResources();
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
    signal(SIGSEGV, onSignal);

    DI_SAVE_CALLSTACK();

	SDL_Init(SDL_INIT_EVERYTHING);
	LOGI("SDL_Init OK");

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
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
