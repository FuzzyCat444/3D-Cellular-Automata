#ifndef UTIL_H
#define UTIL_H

#include <GL/glew.h>
#include <string>
#include <iostream>

void printShaderCompileErrors(GLuint shader);

void stringReplace(std::string& input, const std::string& find, const std::string& replace);

#endif // UTIL_H
