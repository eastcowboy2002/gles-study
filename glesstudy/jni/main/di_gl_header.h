#ifndef DI_GL_HEADER_H_INCLUDED
#define DI_GL_HEADER_H_INCLUDED

#include "ktx.h"    // in WIN32, ktx.h will include <windows.h> and <GL/glew.h>

#ifdef _WIN32
#include "SDL_opengl.h"
#else
#include "SDL_opengles2.h"
#endif

#include "DiBase.h"

#ifdef _DEBUG
namespace di
{
inline void DbgGlErrors(const char* file, int line)
{
    GLenum err;
    if ((err = glGetError()) != GL_NO_ERROR)
    {
        DI_LOG_ERROR("OpenGL error 0x%X at file: %s, line: %d", err, file, line);
    }
}
}
#   define DI_DBG_CHECK_GL_ERRORS()     di::DbgGlErrors(__FILE__, __LINE__)
#else
#   define DI_DBG_CHECK_GL_ERRORS()
#endif

#endif // DI_GL_HEADER_H_INCLUDED
