#pragma once

#include <unordered_map>

#include "Pipeline.hpp"
#include "resources/ResourceManager.hpp"

struct Material {
	Pipeline pipeline;
	vk::DescriptorSet globalSet;
	vk::DescriptorSet materialSet;
	std::vector<vk::DescriptorSet> instanceSets;
};

struct MaterialDescription {
	std::string pipelineName;
	std::unordered_map<uint32_t, ImageHandle> textures;
	std::unordered_map<uint32_t, BufferHandle> buffers;
};
