#include "Automaton.h"

Automaton::Automaton(std::string name, std::string rule, std::vector<uint32_t> colorScheme, std::function<void(uint32_t*, int, int, int)> seedFunction)
{
	this->name = name;
	this->rule = rule;
	this->colorScheme = colorScheme;
	this->seedFunction = seedFunction;
}
