

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
	std::array<vk::VertexInputAttributeDescription, 4> vertexAttributes;
	std::array<vk::VertexInputBindingDescription, 1> vertexBindings;
	vk::PipelineVertexInputStateCreateInfo vertex() {
		vertexBindings = {
			vk::VertexInputBindingDescription {
											   .binding = 0,
											   .stride = 44,
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
			vk::VertexInputAttributeDescription {
												 .location = 2,
												 .binding = 0,
												 .format = vk::Format::eR32G32B32Sfloat,
												 .offset = 24 },

			vk::VertexInputAttributeDescription { .location = 3,
                                                 .binding = 0,
                                                 .format =
			                                          vk::Format::eR32G32Sfloat,
                                                 .offset = 36 }
		};

		vk::PipelineVertexInputStateCreateInfo info {
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = vertexBindings.data(),
			.vertexAttributeDescriptionCount =
				(uint32_t)vertexAttributes.size(),
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

	vk::PipelineRasterizationStateCreateInfo rasterization(vk::CullModeFlags cullMode) {
		vk::PipelineRasterizationStateCreateInfo info {
			.polygonMode = vk::PolygonMode::eFill,
			.cullMode = cullMode,
			.frontFace = vk::FrontFace::eCounterClockwise,
			.lineWidth = 1,
		};

		return info;
	};

	vk::PipelineMultisampleStateCreateInfo multiSample() {
		return {
			.rasterizationSamples = vk::SampleCountFlagBits::e1,
			.sampleShadingEnable = false,
		};
	}

	vk::PipelineDepthStencilStateCreateInfo depthStencil(
		const PipelineConfiguration& configuration
	) {
		return vk::PipelineDepthStencilStateCreateInfo {
			.depthTestEnable = configuration.depthOp != vk::CompareOp::eAlways,
			.depthWriteEnable = configuration.depthWrite,
			.depthCompareOp = configuration.depthOp,
			.depthBoundsTestEnable = false,
			.stencilTestEnable = configuration.stencilEnabled,
			.front = configuration.stencilOp,
			.back = configuration.stencilOp,

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
	auto rasterization = helper.rasterization(info.configuration.cullMode);
	pipelineInfo.pRasterizationState = &rasterization;

	auto multisampling = helper.multiSample();
	pipelineInfo.pMultisampleState = &multisampling;
	auto depthStencilState = helper.depthStencil(info.configuration);
	pipelineInfo.pDepthStencilState = &depthStencilState;

	std::vector<vk::PipelineColorBlendAttachmentState> colorblendStates(
		info.configuration.attachmentFormats.size(),
		vk::PipelineColorBlendAttachmentState {
			.blendEnable = false,
			.srcColorBlendFactor = vk::BlendFactor::eOne,
			.dstColorBlendFactor = vk::BlendFactor::eZero,
			.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
		}
	);
	vk::PipelineColorBlendStateCreateInfo colorBlendState {
		.attachmentCount = (uint32_t)colorblendStates.size(),
		.pAttachments = colorblendStates.data(),
	};

	pipelineInfo.pColorBlendState = &colorBlendState;

	auto dynamicState = helper.dynamicState();
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = getLayout(device, info.setLayouts);
	pipelineInfo.renderPass = nullptr;

	vk::PipelineRenderingCreateInfoKHR renderingInfo {
		.colorAttachmentCount =
			(uint8_t)info.configuration.attachmentFormats.size(),
		.pColorAttachmentFormats = info.configuration.attachmentFormats.data(),
		.depthAttachmentFormat = info.configuration.stencilEnabled
		                             ? vk::Format::eD24UnormS8Uint
		                             : vk::Format::eD16Unorm,
		.stencilAttachmentFormat = info.configuration.stencilEnabled
		                               ? vk::Format::eD24UnormS8Uint
		                               : vk::Format::eUndefined,


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
