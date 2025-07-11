#pragma once

#include <optional>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

struct PipelineMetadata {
	struct Modules {
		std::string_view vertex;
		std::string_view fragment;
	};
	Modules modules;
	std::vector<vk::DescriptorSetLayout> layouts;
};

struct Pipeline {
	vk::Pipeline pipeline = nullptr;
	vk::PipelineLayout pipelineLayout = nullptr;
};

class PipelineBuilder {
public:
	struct PipelineBuildInfo {
		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
		std::vector<vk::DescriptorSetLayout> setLayouts;
	};

	static std::optional<Pipeline> BuildPipeline(
		vk::Device& device, const PipelineBuildInfo& info
	);
};
