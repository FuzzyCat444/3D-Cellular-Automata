#ifndef AUTOMATON_H
#define AUTOMATON_H

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

struct Automaton
{
	Automaton(std::string name, std::string rule, std::vector<uint32_t> colorScheme, std::function<void(uint32_t*, int, int, int)> seedFunction);
	std::string name;
	std::string rule;
	std::vector<uint32_t> colorScheme;
	std::function<void(uint32_t*, int, int, int)> seedFunction;
};

#endif // AUTOMATON_H