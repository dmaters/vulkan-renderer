#pragma once

#include <optional>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

struct PipelineConfiguration {
	std::vector<vk::Format> attachmentFormats = {};

	bool depthWrite = false;
	vk::CompareOp depthOp = vk::CompareOp::eAlways;

	vk::CullModeFlags cullMode = vk::CullModeFlagBits::eBack;

	bool stencilEnabled = false;
	vk::StencilOpState stencilOp = vk::StencilOpState {};
};
struct PipelineMetadata {
	struct Modules {
		std::string_view vertex = "";
		std::string_view fragment = "";
		std::string_view compute = "";
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
	struct ComputePipelineBuildInfo {
		vk::PipelineShaderStageCreateInfo stage;
		std::vector<vk::DescriptorSetLayout> setLayouts;
	};
	static std::optional<Pipeline> BuildComputePipeline(
		const ComputePipelineBuildInfo& info
	);

	struct GraphicPipelineBuildInfo {
		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
		std::vector<vk::DescriptorSetLayout> setLayouts;
		PipelineConfiguration configuration;
	};
	static std::optional<Pipeline> BuildGraphicPipeline(
		const GraphicPipelineBuildInfo& info
	);
};
