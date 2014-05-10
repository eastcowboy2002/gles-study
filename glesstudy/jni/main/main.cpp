#include "SDL_main.h"
#include "SDL.h"
#include "SDL_image.h"

#include "di_gl_header.h"

#include <stdio.h>
#include <stdlib.h>
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

// GLuint loadKtxTexture(const char* filename, int* width = NULL, int* height = NULL)
// {
// 	SDL_RWops* rw = SDL_RWFromFile(filename, "rb");
// 	if (!rw)
// 	{
// 		SDL_Log("loadKtxTexture '%s' failed. cannot open file", filename);
// 		return 0;
// 	}
// 
// 	GLsizei size = SDL_RWsize(rw);
// 
// 	void* mem = SDL_malloc(size);
// 	if (!size)
// 	{
// 		SDL_RWclose(rw);
// 		SDL_Log("loadKtxTexture '%s' failed. malloc failed", filename);
// 		return 0;
// 	}
// 
// 	if (SDL_RWread(rw, mem, size, 1) != 1)
// 	{
// 		SDL_free(mem);
// 		SDL_RWclose(rw);
// 		SDL_Log("loadKtxTexture '%s' failed. read file failed", filename);
// 		return 0;
// 	}
// 
// 	GLuint tex;
// 	GLenum target;
// 	GLenum glerr;
// 	GLboolean isMipmap;
// 	KTX_dimensions dimensions;
// 	KTX_error_code ktxErr = ktxLoadTextureM(mem, size, &tex, &target, &dimensions, &isMipmap, &glerr, 0, NULL);
// 
// 	SDL_free(mem);
// 	mem = NULL;
// 	SDL_RWclose(rw);
// 	rw = NULL;
// 
// 	if (ktxErr != KTX_SUCCESS || glerr != GL_NO_ERROR)
// 	{
// 		SDL_Log("loadKtxTexture '%s' failed. ktx error = %d, gl error = 0x%X", filename, ktxErr, glerr);
// 		return 0;
// 	}
// 
// 	if (target != GL_TEXTURE_2D)
// 	{
// 		glBindTexture(target, 0);
// 		glDeleteTextures(1, &target);
// 		SDL_Log("loadKtxTexture '%s' failed. not GL_TEXTURE_2D", filename);
// 		return 0;
// 	}
// 
// 	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, isMipmap ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
// 	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
// 	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
// 	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
// 
// 	if (width)
// 	{
// 		*width = dimensions.width;
// 	}
// 
// 	if (height)
// 	{
// 		*height = dimensions.height;
// 	}
// 
// 	return tex;
// }

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

const GLfloat gTriangleVertices[] = { 0.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f };

void renderFrame() {
    DI_SAVE_CALLSTACK();

	glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
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

// 	if (loadKtxTexture("10001.ktx") == 0)
// 	{
// 		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "loadKtxTexture failed");
// 		abort();
// 	}

	uint64_t tick = SDL_GetPerformanceCounter();
    SDL_Surface* t = IMG_Load("main_bg.webp");
    // SDL_Surface* t = IMG_Load("10000.jpg");
    // SDL_Surface* t = IMG_Load("10000.webp");
    // SDL_Surface* t = IMG_Load("advBegin.png");
    // SDL_Surface* t = IMG_Load("advBegin.webp");
	if (t)
	{
		SDL_Log("IMG_Load OK");
		SDL_Log("time cost: %llu ms", (SDL_GetPerformanceCounter() - tick) * 1000 / SDL_GetPerformanceFrequency());
//		SDL_Delay(1000);
//		SDL_Log("time cost: %llu ms", (SDL_GetPerformanceCounter() - tick) * 1000 / SDL_GetPerformanceFrequency());

		GLuint textureObjOpenGLlogo;
		glGenTextures(1, &textureObjOpenGLlogo);

		glBindTexture(GL_TEXTURE_2D, textureObjOpenGLlogo);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		SDL_Log("image format: %d (0x%X)", t->format->format, t->format->format);
		SDL_Log("image masks: 0x%X, 0x%X, 0x%X, 0x%X", t->format->Rmask, t->format->Gmask, t->format->Bmask, t->format->Amask);
		SDL_Log("image losses: %d, %d, %d, %d", t->format->Rloss, t->format->Gloss, t->format->Bloss, t->format->Aloss);
		SDL_Log("image shifts: %d, %d, %d, %d", t->format->Rshift, t->format->Gshift, t->format->Bshift, t->format->Ashift);
		SDL_Log("SDL_PIXELFORMAT_RGB24 = 0x%X", SDL_PIXELFORMAT_RGB24);
		SDL_Log("SDL_PIXELFORMAT_BGR24 = 0x%X", SDL_PIXELFORMAT_BGR24);
		SDL_Log("SDL_PIXELFORMAT_RGB888 = 0x%X", SDL_PIXELFORMAT_RGB888);
		SDL_Log("SDL_PIXELFORMAT_RGBA8888 = 0x%X", SDL_PIXELFORMAT_RGBA8888);
		SDL_Log("SDL_PIXELFORMAT_BGR888 = 0x%X", SDL_PIXELFORMAT_BGR888);
		SDL_Log("SDL_PIXELFORMAT_BGRA8888 = 0x%X", SDL_PIXELFORMAT_BGRA8888);
		SDL_Log("SDL_PIXELFORMAT_BGRX8888 = 0x%X", SDL_PIXELFORMAT_BGRX8888);
		SDL_Log("SDL_PIXELFORMAT_RGBX8888 = 0x%X", SDL_PIXELFORMAT_RGBX8888);
        SDL_Log("SDL_PIXELFORMAT_ABGR8888 = 0x%X", SDL_PIXELFORMAT_ABGR8888);

		bool needLock = SDL_MUSTLOCK(t);
		if (needLock)
		{
			SDL_LockSurface(t);
		}

		if (t->format->format == SDL_PIXELFORMAT_RGB24)
		{
			SDL_Log("format RGB");
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, t->w, t->h, 0, GL_RGB, GL_UNSIGNED_BYTE, t->pixels);
			checkGlError("glTexImage2D");
		}
        else if (t->format->format == SDL_PIXELFORMAT_ABGR8888)
        {
			SDL_Log("format RGBA");
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t->w, t->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, t->pixels);
			checkGlError("glTexImage2D");
        }

		if (needLock)
		{
			SDL_UnlockSurface(t);
		}

		SDL_FreeSurface(t);
	}
	else
	{
		SDL_Log("IMG_Load failed");
		abort();
	}

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

		SDL_WaitEvent(NULL);
	}

    SDL_GL_MakeCurrent(nullptr, nullptr);
    SDL_GL_DeleteContext(context);
    context = nullptr;
    SDL_DestroyWindow(window);
    window = nullptr;

	SDL_Quit();

	return 0;
}
