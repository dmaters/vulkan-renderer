#pragma once

#include <unordered_map>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

#include "Pipeline.hpp"
#include "resources/ResourceManager.hpp"

struct Material {
	Pipeline pipeline;
	vk::DescriptorSet materialSet;
	bool instantiable;
};

struct MaterialDescription {
	std::string pipelineName;
	std::unordered_map<uint32_t, ImageHandle> textures;
	std::unordered_map<uint32_t, BufferHandle> buffers;
};
