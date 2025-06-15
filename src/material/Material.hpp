#pragma once

#include <unordered_map>
#include <vulkan/vulkan_handles.hpp>

#include "AttachmentGroup.hpp"
#include "Pipeline.hpp"
#include "resources/ResourceManager.hpp"

struct Sets {
	vk::DescriptorSet globalSet;
	vk::DescriptorSet materialSet;
	std::vector<vk::DescriptorSet> instanceSets;
}

struct Material {
	Pipeline pipeline;
	Sets sets;
};

struct MaterialDescription {
	std::string pipelineName;
	std::unordered_map<uint32_t, ImageHandle> textures;
	std::unordered_map<uint32_t, BufferHandle> buffers;
};
