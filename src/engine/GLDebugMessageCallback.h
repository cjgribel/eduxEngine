// Header for GLDebugMessageCallback by Plasmoxy 2020
// Feel free to use this in any way.

// Dec 2021: Added inclusion guard and precompiler branch for Mac 

// Plasmoxy / GLDebugMessageCallback.cc
// https://gist.github.com/Plasmoxy/aec637b85e306f671339dcfd509efc82

#ifndef GL_DEBUG_MESSAGE_CALLBACK
#define GL_DEBUG_MESSAGE_CALLBACK
#pragma once

#include <iostream>
#include "config.h"
#include "glcommon.h"
//#include <GL/glew.h>
//#include <GLFW/glfw3.h>
//#include "config.h"

#ifdef EENG_PLATFORM_WINDOWS
#define APIENTRY __stdcall
#define CONSTNESS const
#else
#define APIENTRY
#define CONSTNESS const
#endif

void APIENTRY GLDebugMessageCallback(GLenum source,
                                     GLenum type,
                                     GLuint id,
                                     GLenum severity,
                                     GLsizei length,
                                     const GLchar* msg,
                                     CONSTNESS void* data);
#endif