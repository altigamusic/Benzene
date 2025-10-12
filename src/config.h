#pragma once

#include <vector>
#include "uniform.h"

struct UniformConfig
{
	std::vector<Uniform> uniformList;
};

bool saveConfig(const UniformConfig& config, const std::string& filename);
UniformConfig loadConfig(const std::string& filename);