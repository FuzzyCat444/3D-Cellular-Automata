#include "CellRenderShader.h"

CellRenderShader::CellRenderShader(int width, int height, int depth, GLuint cellMeshSSBO)
{
	this->width = width;
	this->height = height;
	this->depth = depth;
	cellsSize = width * height * depth;

	// We render the same mesh buffer generated by the CellMeshingShader to prevent expensive buffer copying operations.
	glBindBuffer(GL_ARRAY_BUFFER, cellMeshSSBO);
	// 4 bytes per component * 4 components per vertex * 6 vertices per face * 1 face per stage (also used in CellMeshingShader.cpp)
	glBufferData(GL_ARRAY_BUFFER, 4 * 4 * 6 * static_cast<GLsizeiptr>(cellsSize), nullptr, GL_DYNAMIC_DRAW);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	// Vertices are in format (x: float32, y: float32, z: float32, color: uint32)
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(GLuint), 0);
	glEnableVertexAttribArray(1);
	glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, 4 * sizeof(GLfloat), reinterpret_cast<void*>(3 * sizeof(GLfloat)));
	glBindVertexArray(0);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	std::string vertexShaderSource = 
R"(
#version 430 core

uniform mat4 mvpMatrix;
uniform int stage;

layout(location = 0) in vec3 position;
layout(location = 1) in uint color;

out vec3 vertexColor;

// How light the different cube sides will appear (fake lighting).
const float BRIGHTNESS[6] = { 0.8, 0.8, 0.6, 0.6, 1.0, 1.0 };

void main()
{
	/* If the vertex data is null (not visible), render a null triangle with all vertices at (0, 0, 0).
       1e38 is a large float value that we use as a flag for vertex data that shouldn't be rendered. */
	if (position.x == 1e38)
	{
		gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
		return;
	}
	
	// Extract color components from RGB888 format integer
	uint r = (color >> 16) & 0xff;
	uint g = (color >> 8) & 0xff;
	uint b = color & 0xff;
    gl_Position = mvpMatrix * vec4(position, 1.0);
	vertexColor = BRIGHTNESS[stage] * vec3
	(
		float(r) / 255.0,
		float(g) / 255.0,
		float(b) / 255.0
	);
}
)";
	std::string fragmentShaderSource = 
R"(
#version 430 core

in vec3 vertexColor;
out vec4 fragColor;

void main()
{
    fragColor = vec4(vertexColor, 1);
}
)";

	const char* vertexShaderSourceStr = vertexShaderSource.c_str();
	const char* fragmentShaderSourceStr = fragmentShaderSource.c_str();

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSourceStr, nullptr);
	glCompileShader(vertexShader);
	std::cout << "RenderShader Vertex compilation:" << std::endl;
	printShaderCompileErrors(vertexShader);
	std::cout << std::endl;

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSourceStr, nullptr);
	glCompileShader(fragmentShader);
	std::cout << "RenderShader Fragment compilation:" << std::endl;
	printShaderCompileErrors(fragmentShader);
	std::cout << std::endl;

	program = glCreateProgram();
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	mvpMatrixUniformLocation = glGetUniformLocation(program, "mvpMatrix");
	stageUniformLocation = glGetUniformLocation(program, "stage");
}

CellRenderShader::~CellRenderShader()
{
	glDeleteVertexArrays(1, &vao);
	glDeleteProgram(program);
}

void CellRenderShader::renderMesh(glm::mat4 mvp, int stage)
{
	glUseProgram(program);
	glUniformMatrix4fv(mvpMatrixUniformLocation, 1, GL_FALSE, glm::value_ptr(mvp));
	glUniform1i(stageUniformLocation, stage);
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLES, 0, 6 * static_cast<GLsizei>(cellsSize));
	glBindVertexArray(0);
	glUseProgram(0);
}