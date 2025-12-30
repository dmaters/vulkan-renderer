#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

#include "Pipeline.hpp"

struct Material {
	Pipeline pipeline;
	vk::DescriptorSet materialSet;
	bool instantiable;
};
