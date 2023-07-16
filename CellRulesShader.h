#ifndef CELL_RULES_SHADER_H
#define CELL_RULES_SHADER_H

#include <GL/glew.h>
#include <string>
#include <cstdint>
#include <cctype>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <iostream>

#include "Util.h"

class CellRulesShader
{
public:
	CellRulesShader(int width, int height, int depth, std::string rule);
	virtual ~CellRulesShader();

	/* The rule determines how the automaton will behave. The format is:   B <numbers> / S <numbers> / <states>
   where <numbers> is a comma separated list of numbers between 0 and 26 inclusive. Each number between 0 and 26
   can either appear once or not appear at all. The <numbers> after B determine the number of cells a dead cell must be
   surrounded by to be reborn, and the <numbers> after S determine the number of cells a living cell must be surrounded
   by to survive until the next iteration. <states> represents the refractory period, which is at least 2. 2 includes
   the dead and alive states. Any number N large than 2 will provide the automaton with N-2 additional "dead" states
   in which it is impossible for a cell to be born in one of those cells until the cell cycles back to a true dead state.
   Example rule: B 2 / S 2,3 / 5 = dead cell comes to life when it has 2 live neighbors, living cell stays alive only if
   it has 2 or 3 live neighbors, there are 3 extra states a cell cycles through after it dies before it is ready to become
   a true dead cell. Once it is a true dead cell, it can be reborn in the next iteration. That is a total of 4 unalive states.
   The rule string may omit the second '/' and <states> for a default of 2 states (dead or alive). */
	void setRule(std::string rule);
	// Convert rule back to string
	std::string getRule();

	// Get cells pointer (CPU side)
	uint32_t* getCells();
	// Update CPU side cells pointer with GPU data
	void updateGPUCells();
	// Update GPU data with CPU side cells pointer
	void fetchGPUCells();
	// Simulate GPU primary cell buffer using rules, and store result in secondary CPU cell buffer
	void simulate();
	GLuint getCellSSBO();

	int getWidth();
	int getHeight();
	int getDepth();
private:
	// First 27 bits are B (born) rules, second 27 bits are S (stay alive) rules, next 9 bits are number of refractory states, last bit is whether this rule is set or not.
	uint64_t ruleFlags;
	bool hasRuleFlagStayAliveBit(uint64_t flags, int flagBit);
	bool hasRuleFlagBornBit(uint64_t flags, int flagBit);
	int getNumStates(uint64_t flags);
	void cleanup();

	int width, height, depth;
	int cellsSize;
	// This is the local cell buffer, which we update on the CPU side when we want to change cells
	uint32_t* cells;
	/* These are the GPU cell buffers, which we only update after we change the CPU side buffer(seldom).
	   We can also fetch the GPU cell buffer and store it in the "cells" member variable using fetchGPUCells(). */
	GLuint cellSSBO[2];
	GLuint computeProgram;
};

#endif // CELL_RULES_SHADER_H

