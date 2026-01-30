// Created by Carl Johan Gribel.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef ShaderLoader_H
#define ShaderLoader_H

#include <cstdlib>
#include <iostream>
#include "glcommon.h"

static void printShaderLog(GLuint obj,
						   GLuint shader)
{
	int infologLength = 0;
	int charsWritten = 0;
	char *infoLog;

	glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &infologLength);

	if (infologLength > 1)
	{
		infoLog = (char *)malloc(infologLength);
		glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
		std::cerr << infoLog << std::endl;
		free(infoLog);
		throw std::runtime_error("shader compilation failed");
	}

	glGetShaderiv(shader,
				  GL_INFO_LOG_LENGTH,
				  &infologLength);

	if (infologLength > 1)
	{
		infoLog = (char *)malloc(infologLength);
		glGetShaderInfoLog(shader, infologLength, &charsWritten, infoLog);
		std::cerr << infoLog << std::endl;
		free(infoLog);
		throw std::runtime_error("shader compilation failed");
	}
}

static GLuint createShaderProgram(const char *vertexShaderSource,
								  const char *fragmentShaderSource)
{
	// Make sure GL-errors has not already been thrown elsewhere
	CheckAndThrowGLErrors();

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(vertexShader, 1, &vertexShaderSource, 0);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, 0);

	std::cout << "Compiling vertex shader..." << std::endl;
	glCompileShader(vertexShader);

	std::cout << "Compiling fragment shader..." << std::endl;
	glCompileShader(fragmentShader);

	GLuint program = glCreateProgram();

	glAttachShader(program, vertexShader);
	printShaderLog(program, vertexShader);

	glAttachShader(program, fragmentShader);
	printShaderLog(program, fragmentShader);

	glLinkProgram(program);
	if (glGetError() != GL_NO_ERROR)
	{
		std::cerr << "errors:\n";
		printShaderLog(program, vertexShader);
		printShaderLog(program, fragmentShader);
		throw std::runtime_error("shader compilation failed");
	}

#if 0
    int params[9];
    glGetProgramiv(program, GL_DELETE_STATUS, params+0);
    std::cout << "GL_DELETE_STATUS " << params[0] << std::endl;
    glGetProgramiv(program, GL_LINK_STATUS, params+1);
    std::cout << "GL_LINK_STATUS " << params[1] << std::endl;
    glGetProgramiv(program, GL_VALIDATE_STATUS, params+2);
    std::cout << "GL_VALIDATE_STATUS " << params[2] << std::endl;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, params+3);
    std::cout << "GL_INFO_LOG_LENGTH " << params[3] << std::endl;
    glGetProgramiv(program, GL_ATTACHED_SHADERS, params+4);
    std::cout << "GL_ATTACHED_SHADERS " << params[4] << std::endl;
    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, params+5);
    std::cout << "GL_ACTIVE_ATTRIBUTES " << params[5] << std::endl;
    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, params+6);
    std::cout << "GL_ACTIVE_ATTRIBUTE_MAX_LENGTH " << params[6] << std::endl;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, params+7);
    std::cout << "GL_ACTIVE_UNIFORMS " << params[7] << std::endl;
    glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, params+8);
    std::cout << "GL_ACTIVE_UNIFORM_MAX_LENGTH " << params[8] << std::endl;
#endif

	return program;
}

#endif
