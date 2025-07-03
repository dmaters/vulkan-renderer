

#include "Pipeline.hpp"

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

struct PipelineStateCreateInfo {
	vk::PipelineInputAssemblyStateCreateInfo inputAssembly() {
		vk::PipelineInputAssemblyStateCreateInfo info {
			.topology = vk::PrimitiveTopology::eTriangleList,

		};

		return info;
	};

	vk::PipelineViewportStateCreateInfo viewport() {
		// Dynamic state viewport
		vk::PipelineViewportStateCreateInfo info {
			.viewportCount = 1,
			.scissorCount = 1,
		};

		return info;
	};
	std::array<vk::VertexInputAttributeDescription, 3> vertexAttributes;
	std::array<vk::VertexInputBindingDescription, 1> vertexBindings;
	vk::PipelineVertexInputStateCreateInfo vertex() {
		vertexBindings = {
			vk::VertexInputBindingDescription {
											   .binding = 0,
											   .stride = 32,
											   .inputRate = vk::VertexInputRate::eVertex },
		};

		vertexAttributes = {
			vk::VertexInputAttributeDescription {
												 .location = 0,
												 .binding = 0,
												 .format = vk::Format::eR32G32B32Sfloat,
												 .offset = 0  },
			vk::VertexInputAttributeDescription {
												 .location = 1,
												 .binding = 0,
												 .format = vk::Format::eR32G32B32Sfloat,
												 .offset = 12 },

			vk::VertexInputAttributeDescription { .location = 2,
                                                 .binding = 0,
                                                 .format =
			                                          vk::Format::eR32G32Sfloat,
                                                 .offset = 24 }
		};

		vk::PipelineVertexInputStateCreateInfo info {
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = vertexBindings.data(),
			.vertexAttributeDescriptionCount = 3,
			.pVertexAttributeDescriptions = vertexAttributes.data()
		};

		return info;
	};

	std::array<vk::DynamicState, 2> states;
	vk::PipelineDynamicStateCreateInfo dynamicState() {
		states[0] = vk::DynamicState::eViewport;
		states[1] = vk::DynamicState::eScissor;

		vk::PipelineDynamicStateCreateInfo info {
			.dynamicStateCount = 2, .pDynamicStates = states.data()
		};

		return info;
	};

	vk::PipelineRasterizationStateCreateInfo rasterization() {
		vk::PipelineRasterizationStateCreateInfo info {
			.polygonMode = vk::PolygonMode::eFill,
			.cullMode = vk::CullModeFlagBits::eBack,
			.frontFace = vk::FrontFace::eCounterClockwise,
			.lineWidth = 1,
		};

		return info;
	};

	std::array<vk::PipelineColorBlendAttachmentState, 1> colorblendStates;
	vk::PipelineColorBlendStateCreateInfo colorBlend() {
		colorblendStates = {
			vk::PipelineColorBlendAttachmentState {

												   .blendEnable = false,
												   .colorWriteMask = vk::ColorComponentFlagBits(0xf),
												   },
		};
		return {

			.attachmentCount = (uint32_t)colorblendStates.size(),
			.pAttachments = colorblendStates.data(),
		};
	};

	vk::PipelineMultisampleStateCreateInfo multiSample() {
		return {
			.rasterizationSamples = vk::SampleCountFlagBits::e1,
			.sampleShadingEnable = false,
		};
	}

	vk::PipelineDepthStencilStateCreateInfo depthStencil() {
		return vk::PipelineDepthStencilStateCreateInfo {
			.depthTestEnable = true,
			.depthWriteEnable = true,
			.depthCompareOp = vk::CompareOp::eLess,
			.depthBoundsTestEnable = false,
			.stencilTestEnable = false,

		};
	}
};

vk::PipelineLayout getLayout(
	vk::Device& device, std::vector<vk::DescriptorSetLayout> layouts
);

std::optional<Pipeline> PipelineBuilder::BuildPipeline(
	vk::Device& device, const PipelineBuildInfo& info
) {
	vk::GraphicsPipelineCreateInfo pipelineInfo;
	PipelineStateCreateInfo helper;

	pipelineInfo.flags = {};
	pipelineInfo.stageCount = 1;
	pipelineInfo.setStages(info.shaderStages);

	auto vertex = helper.vertex();
	pipelineInfo.pVertexInputState = &vertex;

	auto assembly = helper.inputAssembly();
	pipelineInfo.pInputAssemblyState = &assembly;
	pipelineInfo.pTessellationState = {};

	auto viewport = helper.viewport();
	pipelineInfo.pViewportState = &viewport;
	auto rasterization = helper.rasterization();
	pipelineInfo.pRasterizationState = &rasterization;

	auto multisampling = helper.multiSample();
	pipelineInfo.pMultisampleState = &multisampling;
	auto depthStencilState = helper.depthStencil();
	pipelineInfo.pDepthStencilState = &depthStencilState;
	auto colorBlendState = helper.colorBlend();
	pipelineInfo.pColorBlendState = &colorBlendState;

	auto dynamicState = helper.dynamicState();
	pipelineInfo.pDynamicState = &dynamicState;

	pipelineInfo.layout = getLayout(device, info.setLayouts);

	pipelineInfo.renderPass = nullptr;
	vk::PipelineRenderingCreateInfoKHR renderingInfo {
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats =
			std::array<vk::Format, 1> { vk::Format::eB8G8R8A8Unorm }.data(),
		.depthAttachmentFormat = vk::Format::eD16Unorm,

	};
	pipelineInfo.pNext = &renderingInfo;

	pipelineInfo.subpass = 0;

	pipelineInfo.basePipelineHandle = nullptr;
	pipelineInfo.basePipelineIndex = 0;

	auto res = device.createGraphicsPipeline(vk::PipelineCache(), pipelineInfo);
	if (res.result != vk::Result::eSuccess) return std::nullopt;

	return Pipeline {
		.pipeline = res.value,
		.pipelineLayout = pipelineInfo.layout,
	};
}

vk::PipelineLayout getLayout(
	vk::Device& device, std::vector<vk::DescriptorSetLayout> layouts
) {
	std::array<vk::PushConstantRange, 1> ranges {
		vk::PushConstantRange {
							   .stageFlags = vk::ShaderStageFlagBits::eVertex |
		                  vk::ShaderStageFlagBits::eFragment,
							   .offset = 0,
							   .size = 68,
							   },
	};
	vk::PipelineLayoutCreateInfo info {
		.setLayoutCount = (uint32_t)layouts.size(),
		.pSetLayouts = layouts.data(),
		.pushConstantRangeCount = ranges.size(),
		.pPushConstantRanges = ranges.data(),

	};
	return device.createPipelineLayout(info);
}