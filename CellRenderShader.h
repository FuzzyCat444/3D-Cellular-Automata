#ifndef CELL_RENDER_SHADER_H
#define CELL_RENDER_SHADER_H

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <iostream>

#include "Util.h"

class CellRenderShader
{
public:
	CellRenderShader(int width, int height, int depth, GLuint cellMeshSSBO);
	virtual ~CellRenderShader();

	void renderMesh(glm::mat4 mvp, int stage);
private:
	int width, height, depth;
	int cellsSize;
	GLint mvpMatrixUniformLocation;
	GLint stageUniformLocation;
	// Vertex array object for mesh vertex buffer
	GLuint vao;
	GLuint program;
};

#endif // CELL_RENDER_SHADER_H

