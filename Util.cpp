#include "Util.h"

void printShaderCompileErrors(GLuint shader)
{
	GLint success;

	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

	if (success == GL_FALSE) 
	{
		GLint logLength;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

		std::string errorLog(logLength, ' ');
		glGetShaderInfoLog(shader, logLength, nullptr, &errorLog[0]);

		std::cout << "Shader compilation error:" << std::endl << errorLog << std::endl;
	}
	else
	{
		std::cout << "Shader compiled successfully." << std::endl;
	}
}

void stringReplace(std::string& input, const std::string& find, const std::string& replace)
{
	size_t pos = input.find(find);
	while (pos != std::string::npos)
	{
		input.replace(pos, find.length(), replace);
		pos = input.find(find);
	}
}
