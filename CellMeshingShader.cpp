#include "CellMeshingShader.h"

CellMeshingShader::CellMeshingShader(int width, int height, int depth)
{
	this->width = width;
	this->height = height;
	this->depth = depth;

	glGenBuffers(1, &meshSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, meshSSBO);

	// 4 bytes per component * 4 components per vertex * 6 vertices per face * 1 face per stage (also used in CellRenderShader.cpp)
	glBufferData(GL_SHADER_STORAGE_BUFFER, 4 * 4 * 6 * static_cast<GLsizeiptr>(width * height * depth), nullptr, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	std::string computeShaderSource = 
R"(
#version 430 core

layout(local_size_x = 2, local_size_y = 2, local_size_z = 2) in;

uniform int stage;
uniform uint colorScheme[128];

// Input cell state
layout(std430, binding = 0) buffer State
{
	uint cells[];
} state;

struct Vertex
{
	float x;
	float y;
	float z;
	uint rgb;
};

// Face = 2 triangles = 6 vertices
struct Face
{
	Vertex vertices[6];
};

// Output mesh
layout(std430, binding = 1) buffer Mesh
{
	Face faces[];
} mesh;

const int WIDTH = $$WIDTH;
const int HEIGHT = $$HEIGHT;
const int DEPTH = $$DEPTH;
const int WIDTH_HEIGHT = WIDTH * HEIGHT;
const int WIDTH_HEIGHT_DEPTH = WIDTH_HEIGHT * DEPTH;

// Vertices of a cube's face, not including duplicate vertices (like when we use 2 triangles = 6 vertices)
struct CubeFace
{
	Vertex bottomLeft;
	Vertex bottomRight;
	Vertex topRight;
	Vertex topLeft;
};

// Untranslated vertex data for each face (left, right, bottom, top, back, front)
const CubeFace CUBE_FACES[6] =
{
	CubeFace(Vertex(0.0, 0.0, 0.0, 0), Vertex(0.0, 0.0, 1.0, 0), Vertex(0.0, 1.0, 1.0, 0), Vertex(0.0, 1.0, 0.0, 0)),
	CubeFace(Vertex(1.0, 0.0, 0.0, 0), Vertex(1.0, 1.0, 0.0, 0), Vertex(1.0, 1.0, 1.0, 0), Vertex(1.0, 0.0, 1.0, 0)),
	CubeFace(Vertex(0.0, 0.0, 0.0, 0), Vertex(1.0, 0.0, 0.0, 0), Vertex(1.0, 0.0, 1.0, 0), Vertex(0.0, 0.0, 1.0, 0)),
	CubeFace(Vertex(0.0, 1.0, 0.0, 0), Vertex(0.0, 1.0, 1.0, 0), Vertex(1.0, 1.0, 1.0, 0), Vertex(1.0, 1.0, 0.0, 0)),
	CubeFace(Vertex(0.0, 0.0, 0.0, 0), Vertex(0.0, 1.0, 0.0, 0), Vertex(1.0, 1.0, 0.0, 0), Vertex(1.0, 0.0, 0.0, 0)),
	CubeFace(Vertex(0.0, 0.0, 1.0, 0), Vertex(1.0, 0.0, 1.0, 0), Vertex(1.0, 1.0, 1.0, 0), Vertex(0.0, 1.0, 1.0, 0))
};

void main()
{
	int index = int(gl_GlobalInvocationID.x + gl_GlobalInvocationID.y * WIDTH + gl_GlobalInvocationID.z * WIDTH_HEIGHT);
	// Do not execute shader on out of bounds cell index
	if (gl_GlobalInvocationID.x >= WIDTH || gl_GlobalInvocationID.y >= HEIGHT || gl_GlobalInvocationID.z >= DEPTH)
		return;

	uint currentCell = state.cells[index];
	if (currentCell == 0)
	{
		// Micro optimization to prevent writing to memory if cell won't be rendered (aka == 0) and the geometry is already in a "null state"
		if (mesh.faces[index].vertices[0].x == 1e38)
			return;
	}

	const int LEFT = gl_GlobalInvocationID.x == 0 ? 0 : -1;
	const int RIGHT = gl_GlobalInvocationID.x == WIDTH - 1 ? 0 : 1;
	const int DOWN = gl_GlobalInvocationID.y == 0 ? 0 : -WIDTH;
	const int UP = gl_GlobalInvocationID.y == HEIGHT - 1 ? 0 : WIDTH;
	const int BACKWARD = gl_GlobalInvocationID.z == 0 ? 0 : -WIDTH_HEIGHT;
	const int FORWARD = gl_GlobalInvocationID.z == DEPTH - 1 ? 0 : WIDTH_HEIGHT;
	const int DIRECTIONS[6] = { LEFT, RIGHT, DOWN, UP, BACKWARD, FORWARD };
	
	Face face;
	for (int i = 0; i < 6; i++)
	{
		// Initialize with null/dummy quads to prevent vertex shader processing if this face doesn't exist
		face.vertices[i] = Vertex(1e38, 1e38, 1e38, 0xffffffff);
	}

	uint color = colorScheme[currentCell];
	int direction = DIRECTIONS[stage];
	// If direction == 0, we're at the edge of the cell buffer, so render a face, else render a face if this one is alive and neighbor is dead.
	bool needsFace = direction == 0 ? true : state.cells[index + direction] == 0;
	if (currentCell > 0 && needsFace)
	{
		CubeFace localCubeFace = CUBE_FACES[stage];
		localCubeFace = CUBE_FACES[stage];
		vec3 giid = vec3(gl_GlobalInvocationID);
		// Translate the cube face vertex data to this cell's position and set the color
		CubeFace globalCubeFace = CubeFace
		(
			Vertex
			(
				giid.x + localCubeFace.bottomLeft.x,
				giid.y + localCubeFace.bottomLeft.y,
				giid.z + localCubeFace.bottomLeft.z,
				color
			),
			Vertex
			(
				giid.x + localCubeFace.bottomRight.x,
				giid.y + localCubeFace.bottomRight.y,
				giid.z + localCubeFace.bottomRight.z,
				color
			),
			Vertex
			(
				giid.x + localCubeFace.topRight.x,
				giid.y + localCubeFace.topRight.y,
				giid.z + localCubeFace.topRight.z,
				color
			),
			Vertex
			(
				giid.x + localCubeFace.topLeft.x,
				giid.y + localCubeFace.topLeft.y,
				giid.z + localCubeFace.topLeft.z,
				color
			)
		);

		// Bottom triangle
		face.vertices[0] = globalCubeFace.bottomLeft;
		face.vertices[1] = globalCubeFace.bottomRight;
		face.vertices[2] = globalCubeFace.topLeft;

		// Top triangle
		face.vertices[3] = globalCubeFace.topRight;
		face.vertices[4] = globalCubeFace.topLeft;
		face.vertices[5] = globalCubeFace.bottomRight;
	}

	// Store the meshed face. This is the most expensive operation due to limited memory bandwidth (each face is 96 bytes).
	mesh.faces[index] = face;
}
)";
	stringReplace(computeShaderSource, "$$WIDTH", std::to_string(width));
	stringReplace(computeShaderSource, "$$HEIGHT", std::to_string(height));
	stringReplace(computeShaderSource, "$$DEPTH", std::to_string(depth));

	const char* shaderSourceStr = computeShaderSource.c_str();

	GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(shader, 1, &shaderSourceStr, nullptr);
	glCompileShader(shader);
	std::cout << "CellMeshingShader compilation:" << std::endl;
	printShaderCompileErrors(shader);
	std::cout << std::endl;

	computeProgram = glCreateProgram();
	glAttachShader(computeProgram, shader);
	glLinkProgram(computeProgram);

	glDeleteShader(shader);

	stageUniformLocation = glGetUniformLocation(computeProgram, "stage");
	colorSchemeUniformLocation = glGetUniformLocation(computeProgram, "colorScheme");
}

CellMeshingShader::~CellMeshingShader()
{
	glDeleteBuffers(1, &meshSSBO);
	glDeleteProgram(computeProgram);
}

void CellMeshingShader::meshCells(GLuint cellSSBO, int stage, const std::vector<GLuint>& colorScheme)
{
	glUseProgram(computeProgram);
	glUniform1i(stageUniformLocation, stage);
	glUniform1uiv(colorSchemeUniformLocation, colorScheme.size(), colorScheme.data());

	// Allow compute program to access these buffers at binding points 0 and 1
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, cellSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, meshSSBO);

	// Dispath shader in groups of 2x2x2 to align with 'layout' declaration in shader source. (x + 1) / 2 rounds up in case of odd dimensions.
	glDispatchCompute((width + 1) / 2, (height + 1) / 2, (depth + 1) / 2);
	// Prevents future operations on buffers until shader is done writing to them.
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);

	glUseProgram(0);
}

GLuint CellMeshingShader::getMeshSSBO()
{
	return meshSSBO;
}
