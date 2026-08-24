#pragma once

#include <optional>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

struct ShaderModule {
	std::string_view path;
	std::string_view entryPoint = "main";
};
struct GraphicPipelineModules {
	ShaderModule vertex;
	ShaderModule fragment;
};

struct GraphicPipelineConfiguration {
	std::vector<vk::Format> colorAttachmentFormats = {};
	vk::Format depthFormat = vk::Format::eUndefined;

	bool depthWrite = false;
	vk::CompareOp depthOp = vk::CompareOp::eAlways;

	vk::CullModeFlags cullMode = vk::CullModeFlagBits::eBack;

	bool stencilEnabled = false;
	vk::StencilOpState stencilOp = vk::StencilOpState {};
};

struct Pipeline {
	vk::Pipeline pipeline = nullptr;
	vk::PipelineLayout pipelineLayout = nullptr;
};

class PipelineBuilder {
public:
	struct ComputePipelineBuildInfo {
		vk::PipelineShaderStageCreateInfo stage;
		std::vector<vk::DescriptorSetLayout>& setLayouts;
	};
	static std::optional<Pipeline> BuildComputePipeline(const ComputePipelineBuildInfo& info);

	struct GraphicPipelineBuildInfo {
		std::vector<vk::PipelineShaderStageCreateInfo>& shaderStages;
		std::vector<vk::DescriptorSetLayout>& setLayouts;
		GraphicPipelineConfiguration& configuration;
	};
	static std::optional<Pipeline> BuildGraphicPipeline(const GraphicPipelineBuildInfo& info);
};
