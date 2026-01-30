// Created by Carl Johan Gribel.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef GLCOMMON_H
#define GLCOMMON_H

#include <iostream>
#include <stdexcept>
#include <GL/glew.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#endif

static inline const char *GetGLErrorString(GLenum error)
{
	switch (error)
	{
	case GL_NO_ERROR:
		return "GL_NO_ERROR";
	case GL_INVALID_ENUM:
		return "GL_INVALID_ENUM";
	case GL_INVALID_VALUE:
		return "GL_INVALID_VALUE";
	case GL_INVALID_OPERATION:
		return "GL_INVALID_OPERATION";
	case GL_INVALID_FRAMEBUFFER_OPERATION:
		return "GL_INVALID_FRAMEBUFFER_OPERATION";
	case GL_OUT_OF_MEMORY:
		return "GL_OUT_OF_MEMORY";
	case GL_STACK_UNDERFLOW:
		return "GL_STACK_UNDERFLOW";
	case GL_STACK_OVERFLOW:
		return "GL_STACK_OVERFLOW";
	default:
		return "Unknown GL error";
	}
}

static inline void CheckAndThrowGLErrors()
{
	GLenum err = glGetError();
	bool hasError = (err != GL_NO_ERROR);

	while (err != GL_NO_ERROR)
	{
		std::cerr << "GLError " << GetGLErrorString(err)
				  << " set in File: " << __FILE__
				  << " Line: " << __LINE__ << std::endl;
		err = glGetError();
	}

	if (hasError)
	{
		throw std::runtime_error("GL error(s) caught");
	}
}

static inline void FlushGLErrors()
{
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR)
	{
		// Discard the error
	}
}

#endif
