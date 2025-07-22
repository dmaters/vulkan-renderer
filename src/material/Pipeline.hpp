#pragma once

#include <optional>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

struct PipelineConfiguration {
	std::vector<vk::Format> attachmentFormats = {
		vk::Format::eR16G16B16A16Sfloat
	};

	bool depthWrite = false;
	vk::CompareOp depthOp = vk::CompareOp::eNever;

	bool stencilEnabled = false;
	vk::StencilOpState stencilOp = vk::StencilOpState {};
};
struct PipelineMetadata {
	struct Modules {
		std::string_view vertex;
		std::string_view fragment;
	};
	Modules modules;
	std::vector<vk::DescriptorSetLayout> layouts;
	PipelineConfiguration configuration;
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
		PipelineConfiguration configuration;
	};

	static std::optional<Pipeline> BuildPipeline(
		vk::Device& device, const PipelineBuildInfo& info
	);
};
