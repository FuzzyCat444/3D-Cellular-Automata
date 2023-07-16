#ifndef CELL_MESHING_SHADER_H
#define CELL_MESHING_SHADER_H

#include <GL/glew.h>
#include <string>
#include <vector>

#include "Util.h"

class CellMeshingShader
{
public:
	CellMeshingShader(int width, int height, int depth);
	virtual ~CellMeshingShader();

	void meshCells(GLuint cellSSBO, int stage, const std::vector<GLuint>& colorScheme);
	GLuint getMeshSSBO();
private:
	int width, height, depth;
	// The stage variable indicates whether meshing should be done for left, right, top, bottom, front, or back cube faces
	GLint stageUniformLocation;
	// Automata color scheme. An array of colors in RGB888 format
	GLint colorSchemeUniformLocation;
	// Output buffer. Vertices get directly written into this buffer.
	GLuint meshSSBO;
	GLuint computeProgram;
};

#endif // CELL_MESHING_SHADER_H