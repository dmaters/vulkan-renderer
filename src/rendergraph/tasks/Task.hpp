#pragma once

#include <map>
#include <string>
#include <vulkan/vulkan.hpp>

#include "../RenderGraphResourceSolver.hpp"

struct Resources;
class Task {
private:
	std::string m_name;

public:
	virtual void setup(RenderGraphResourceSolver& graph) = 0;
	virtual void execute(
		vk::CommandBuffer& buffer, const Resources& resources
	) = 0;
};