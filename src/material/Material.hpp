#pragma once

#include <unordered_map>

#include "AttachmentGroup.hpp"
#include "Pipeline.hpp"
#include "resources/ResourceManager.hpp"

struct Material {
	Pipeline pipeline;
	vk::DescriptorSet globalSet;
	vk::DescriptorSet materialSet;
	vk::DescriptorSet attachmentSet;
	std::vector<vk::DescriptorSet> instanceSets;

	std::optional<AttachmentGroup> frameBuffer;
};

struct MaterialDescription {
	std::string pipelineName;
	std::unordered_map<uint32_t, ImageHandle> textures;
	std::unordered_map<uint32_t, BufferHandle> buffers;
};
