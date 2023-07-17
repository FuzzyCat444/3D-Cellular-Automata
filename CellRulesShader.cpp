#include "CellRulesShader.h"

CellRulesShader::CellRulesShader(int width, int height, int depth, std::string rule)
{
	ruleFlags = 0;
	this->width = width;
	this->height = height;
	this->depth = depth;
	cellsSize = width * height * depth;
	cells = nullptr;
	cellSSBO[0] = 0;
	cellSSBO[1] = 0;
	computeProgram = 0;
	setRule(rule);
}

CellRulesShader::~CellRulesShader()
{
	cleanup();
}

void CellRulesShader::setRule(std::string rule)
{
	ruleFlags = 0;

	// *** BEGIN PARSE RULE STRING ***

	rule.erase(std::remove_if(rule.begin(), rule.end(), isspace), rule.end());
	for (auto it = rule.begin(); it != rule.end(); it++)
	{
		if (isalpha(*it))
			*it = toupper(*it);
	}

	size_t i0 = rule.find('/', 0);
	size_t i1 = rule.find('/', i0 + 1);
	if (i0 == std::string::npos)
		return;

	std::string bStr = rule.substr(0, i0);
	std::string sStr;
	std::string nStr;
	if (i1 == std::string::npos)
	{
		sStr = rule.substr(i0 + 1);
		nStr = "2";
	} 
	else
	{
		sStr = rule.substr(i0 + 1, i1 - (i0 + 1));
		nStr = rule.substr(i1 + 1);
		if (nStr.length() == 0)
			nStr = "2";
	}

	if (bStr[0] != 'B' || sStr[0] != 'S')
		return;

	bStr.erase(0, 1);
	sStr.erase(0, 1);

	uint64_t newRuleFlags = 0;
	size_t bStrPos = -1;
	do 
	{
		size_t startPos = bStrPos + 1;
		bStrPos = bStr.find(',', startPos);
		if (bStrPos == std::string::npos)
			bStrPos = bStr.length();

		std::string digitsStr = bStr.substr(startPos, bStrPos - startPos);
		if (!std::all_of(digitsStr.begin(), digitsStr.end(), isdigit))
			return;

		int ruleNumber = -1;
		try
		{
			ruleNumber = std::stoi(digitsStr);
		}
		catch (const std::invalid_argument& ex)
		{
			return;
		}
		if (ruleNumber < 0 || ruleNumber > 26)
			return;

		newRuleFlags |= (uint64_t) 1 << ruleNumber;
	} while (bStrPos != bStr.length());

	size_t sStrPos = -1;
	do
	{
		size_t startPos = sStrPos + 1;
		sStrPos = sStr.find(',', startPos);
		if (sStrPos == std::string::npos)
			sStrPos = sStr.length();

		std::string digitsStr = sStr.substr(startPos, sStrPos - startPos);
		if (!std::all_of(digitsStr.begin(), digitsStr.end(), isdigit))
			return;

		int ruleNumber = -1;
		try
		{
			ruleNumber = std::stoi(digitsStr);
		}
		catch (const std::invalid_argument& ex)
		{
			return;
		}
		if (ruleNumber < 0 || ruleNumber > 26)
			return;

		newRuleFlags |= (uint64_t) 1 << (ruleNumber + 27);
	} while (sStrPos != sStr.length());

	if (!std::all_of(nStr.begin(), nStr.end(), isdigit))
		return;
	int numStates = 0;
	try
	{
		numStates = std::stoi(nStr);
	}
	catch (const std::invalid_argument& ex)
	{
		return;
	}

	if (numStates < 2 || numStates > 255)
		return;

	newRuleFlags |= (uint64_t) numStates << 54;

	newRuleFlags |= (uint64_t) 1 << 63;

	// *** END PARSE RULE STRING ***

	// *** BEGIN OPENGL BUFFER/SHADER SETUP ***

	// We are generating a new rule so we need a new shader. Delete the old shader and buffers.
	cleanup();

	// Initialize cell buffer to 0 so the OpenGL buffers will be initialized to an empty state.
	cells = new uint32_t[cellsSize];
	for (int i = 0; i < cellsSize; i++)
		cells[i] = 0;

	/* We generate 2 int buffers, one for the previous state of the cellular automaton, and one for the future state. 
	   The compute shader will read data from the previous state buffer and write data to the future state buffer.
	   These buffers are designed to be swapped before each simulation so that buffer data never has to be copied. */
	glGenBuffers(2, cellSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellSSBO[0]);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(uint32_t) * static_cast<GLsizeiptr>(cellsSize), cells, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellSSBO[1]);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(uint32_t) * static_cast<GLsizeiptr>(cellsSize), cells, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// To better understand this compute shader, print the formatted source string to the console.
	std::string computeShaderSource = 
R"(
#version 430 core

layout(local_size_x = 2, local_size_y = 2, local_size_z = 2) in;

layout(std430, binding = 0) buffer PreviousState 
{
	uint cells[];
} previousState;

layout(std430, binding = 1) buffer FutureState 
{
	uint cells[];
} futureState;

const int WIDTH = $$WIDTH;
const int HEIGHT = $$HEIGHT;
const int DEPTH = $$DEPTH;
const int WIDTH_HEIGHT = WIDTH * HEIGHT;
const int WIDTH_HEIGHT_DEPTH = WIDTH_HEIGHT * DEPTH;

const int NUM_STATES = $$NUM_STATES;

void main() 
{
	int index = int(gl_GlobalInvocationID.x + gl_GlobalInvocationID.y * WIDTH + gl_GlobalInvocationID.z * WIDTH_HEIGHT);
	if (gl_GlobalInvocationID.x >= WIDTH || gl_GlobalInvocationID.y >= HEIGHT || gl_GlobalInvocationID.z >= DEPTH)
		return;

	// Values for incrementing index within 3D array by 1 unit in each direction. If index will be out of grid boundaries, wrap index back around to other side (that is what the ternary conditionals are for).
	// Bear in mind we are really using a 1D array, so we must increment by the appropriate offset in each dimension (i.e. going up 1 unit in the y direction means increment index by WIDTH, not 1).
	const int LEFT = gl_GlobalInvocationID.x == 0 ? WIDTH - 1 : -1;
	const int RIGHT = gl_GlobalInvocationID.x == WIDTH - 1 ? -(WIDTH - 1) : 1;
	const int DOWN = gl_GlobalInvocationID.y == 0 ? WIDTH_HEIGHT - WIDTH : -WIDTH;
	const int UP = gl_GlobalInvocationID.y == HEIGHT - 1 ? -(WIDTH_HEIGHT - WIDTH) : WIDTH;
	const int BACKWARD = gl_GlobalInvocationID.z == 0 ? WIDTH_HEIGHT_DEPTH - WIDTH_HEIGHT : -WIDTH_HEIGHT;
	const int FORWARD = gl_GlobalInvocationID.z == DEPTH - 1 ? -(WIDTH_HEIGHT_DEPTH - WIDTH_HEIGHT) : WIDTH_HEIGHT;

	// Count total number of live neighbors surrounding each cell (there are 26 neighboring cells, 3x3x3 - 1 = 27 - 1 = 26)
	// COUNT_NEIGHBORS is replaced by 26 statements to count each neighbor; view the code in the console window to see how this works.
	int n = 0;
	$$COUNT_NEIGHBORS

	// Prepare the previous cell and future cell states
	uint state = previousState.cells[index];
	uint newState = 0;

	// Alive
	if (state == 1)
	{
		// A live cell may stay alive if the appropriate number of neighboring cells are alive given by CellRulesShader instance rules
		if ($$STAY_ALIVE_RULES)
		{
			newState = 1;
		}
		// Either set cell to 0 (dead) or set to maximum refractory state (also dead)
		else
		{
			$$CELL_DIE
		}
	}
	// Dead
	else
	{
		// Cycle through dead refractory states 2 to (NUM_STATES-1)
		if (state > 2)
		{
			newState = state - 1;
		}
		// At last refractory state, skip 1 (alive) state and set to 0 (dead) state
		else if (state == 2)
		{
			newState = 0;
		}
		else
		{
			// A dead cell may be born if the appropriate number of neighboring cells are alive given by CellRulesShader instance rules
			if ($$BORN_RULES)
			{
				newState = 1;
			}
			else
			{
				newState = 0;
			}
		}
	}

	// Set output buffer cell
	futureState.cells[index] = newState;
}
)";
	stringReplace(computeShaderSource, "$$WIDTH", std::to_string(width));
	stringReplace(computeShaderSource, "$$HEIGHT", std::to_string(height));
	stringReplace(computeShaderSource, "$$DEPTH", std::to_string(depth));
	stringReplace(computeShaderSource, "$$NUM_STATES", std::to_string(getNumStates(newRuleFlags)));
	std::string countNeighborsStr;
	for (int z = 0; z < 3; z++)
	{
		for (int y = 0; y < 3; y++)
		{
			for (int x = 0; x < 3; x++)
			{
				if (!(x == 1 && y == 1 && z == 1))
				{
					countNeighborsStr += "\tn += int(previousState.cells[index + ";
					bool plus = false;
					if (x != 1)
					{
						countNeighborsStr += x == 0 ? "LEFT" : x == 2 ? "RIGHT" : "";
						plus = true;
					}
					if (y != 1)
					{
						if (plus)
							countNeighborsStr += " + ";
						countNeighborsStr += y == 0 ? "DOWN" : y == 2 ? "UP" : "";
						plus = true;
					}
					if (z != 1)
					{
						if (plus)
							countNeighborsStr += " + ";
						countNeighborsStr += z == 0 ? "BACKWARD" : z == 2 ? "FORWARD" : "";
					}
					countNeighborsStr += "] == 1);\n";
				}
			}
		}
	}
	stringReplace(computeShaderSource, "\t$$COUNT_NEIGHBORS", countNeighborsStr);

	std::string bornRulesStr;
	bool orFlag = false;
	for (int i = 0; i < 27; i++)
	{
		if (hasRuleFlagBornBit(newRuleFlags, i))
		{
			if (orFlag)
				bornRulesStr += " || ";
			bornRulesStr += "n == " + std::to_string(i);
			orFlag = true;
		}
	}
	stringReplace(computeShaderSource, "$$BORN_RULES", bornRulesStr);

	std::string stayAliveRulesStr;
	orFlag = false;
	for (int i = 0; i < 27; i++)
	{
		if (hasRuleFlagStayAliveBit(newRuleFlags, i))
		{
			if (orFlag)
				stayAliveRulesStr += " || ";
			stayAliveRulesStr += "n == " + std::to_string(i);
			orFlag = true;
		}
	}
	stringReplace(computeShaderSource, "$$STAY_ALIVE_RULES", stayAliveRulesStr);

	if (getNumStates(newRuleFlags) > 2)
		stringReplace(computeShaderSource, "$$CELL_DIE", "newState = uint(NUM_STATES) - 1;");
	else
		stringReplace(computeShaderSource, "$$CELL_DIE", "newState = 0;");

	const char* shaderSourceStr = computeShaderSource.c_str();

	GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(shader, 1, &shaderSourceStr, nullptr);
	glCompileShader(shader);
	std::cout << "CellRulesShader compilation:" << std::endl;
	printShaderCompileErrors(shader);
	std::cout << std::endl;

	computeProgram = glCreateProgram();
	glAttachShader(computeProgram, shader);
	glLinkProgram(computeProgram);

	glDeleteShader(shader);

	// *** END OPENGL BUFFER/SHADER SETUP ***

	ruleFlags = newRuleFlags;
}

std::string CellRulesShader::getRule()
{
	if (ruleFlags == 0)
		return "UndefinedRule";
	std::string rule = "";

	bool dashFlag = false;
	rule += "B ";
	for (int i = 0; i < 27; i++)
	{
		if (hasRuleFlagBornBit(ruleFlags, i))
		{
			if (dashFlag)
				rule += "-";
			rule += std::to_string(i);
			dashFlag = true;
		}
	}
	dashFlag = false;
	rule += " / S ";
	for (int i = 0; i < 27; i++)
	{
		if (hasRuleFlagStayAliveBit(ruleFlags, i))
		{
			if (dashFlag)
				rule += "-";
			rule += std::to_string(i);
			dashFlag = true;
		}
	}
	int numStates = getNumStates(ruleFlags);
	if (numStates != 2)
	{
		rule += " / ";
		rule += std::to_string(numStates);
	}

	return rule;
}

uint32_t* CellRulesShader::getCells()
{
	return cells;
}

void CellRulesShader::updateGPUCells()
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellSSBO[0]);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(uint32_t) * cellsSize, cells, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellRulesShader::fetchGPUCells()
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellSSBO[0]);
	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t) * cellsSize, cells);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellRulesShader::simulate()
{
	// Allow compute program to access these buffers at binding points 0 and 1
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, cellSSBO[0]);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, cellSSBO[1]);

	glUseProgram(computeProgram);
	// Dispath shader in groups of 2x2x2 to align with 'layout' declaration in shader source. (x + 1) / 2 rounds up in case of odd dimensions.
	glDispatchCompute((width + 1) / 2, (height + 1) / 2, (depth + 1) / 2);
	// Prevents future operations on buffers until shader is done writing to them.
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	glUseProgram(0);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);

	// For next simulation, use the output buffer as the new input buffer
	std::swap(cellSSBO[0], cellSSBO[1]);
}

GLuint CellRulesShader::getCellSSBO()
{
	return cellSSBO[0];
}

int CellRulesShader::getWidth()
{
	return width;
}

int CellRulesShader::getHeight()
{
	return height;
}

int CellRulesShader::getDepth()
{
	return depth;
}

bool CellRulesShader::hasRuleFlagStayAliveBit(uint64_t flags, int flagBit)
{
	return (flags >> (flagBit + 27)) & 1;
}

bool CellRulesShader::hasRuleFlagBornBit(uint64_t flags, int flagBit)
{
	return (flags >> flagBit) & 1;
}

int CellRulesShader::getNumStates(uint64_t flags)
{
	return (flags >> 54) & 511;
}

void CellRulesShader::cleanup()
{
	if (ruleFlags != 0)
	{
		delete[] cells;
		glDeleteProgram(computeProgram);
		glDeleteBuffers(2, cellSSBO);
	}
}
