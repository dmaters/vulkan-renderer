#pragma once

#include "Pipeline.hpp"

struct DescriptorSet {
	vk::DescriptorSet set = nullptr;
	vk::DescriptorSetLayout layout = nullptr;
};
struct Material {
	Pipeline pipeline;

	DescriptorSet globalSet;
	DescriptorSet materialSet;
	vk::DescriptorSetLayout instanceLayout;
	std::vector<vk::DescriptorSet> instanceSets;
};

struct MaterialInstance {
	uint32_t instanceIndex;
};

struct MaterialDescription {
	struct Resource {
		uint32_t binding;
		uint32_t count;
		vk::DescriptorType type;
		vk::ShaderStageFlags stage;
		std::filesystem::path path;
	};
	std::filesystem::path vertex;
	std::filesystem::path fragment;
	std::vector<Resource> materialResources;
	std::vector<Resource> instanceResources;

	struct DefaultMaterialTextures {
		std::filesystem::path albedo;
	};

	static MaterialDescription Default(DefaultMaterialTextures definition) {
		return { .vertex = "resources/shaders/main.vert.spv",
			     .fragment = "resources/shaders/main.frag.spv",
			     .instanceResources = {
					{ .binding = 0,
			                    .count = 1,
			                    .type = vk::DescriptorType::eCombinedImageSampler,
			                    .stage = vk::ShaderStageFlagBits::eFragment,
							.path = definition.albedo, 
						}, 
					}, 
				};
	}
};
