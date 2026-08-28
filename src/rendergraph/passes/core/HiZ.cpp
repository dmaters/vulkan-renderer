#include "../ComputePass.hpp"
#include "rendergraph/BuildContext.hpp"
#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/SetupContext.hpp"

struct HiZData {
	TaskIndex gbuffer;
	MaterialIndex material;
};

Task::Dependencies setup(Task::SetupContext& context) {
	auto resolution = context.renderingConfiguration.resolution;

	uint32_t minSize = std::min(resolution.x, resolution.y);
	minSize = minSize > 0 ? minSize : 1;
	uint32_t mipLevels = std::floor(std::log2(minSize)) + 1;

	auto hiz = context.createImage(
		"hiz",
		{
			.width = static_cast<uint32_t>(resolution.x),
			.height = static_cast<uint32_t>(resolution.y),
			.depth = 1,
			.miplevels = mipLevels,
			.format = vk::Format::eR32Sfloat,
			.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled |
					 vk::ImageUsageFlagBits::eTransferDst,
		}
	);

	return {
		.inputs = { {
			context.getReference(context.getData<HiZData>().gbuffer, rendergraph::passes::core::GBufferSlots::Depth),
			ResourceUsage::Type::SampledRead,
		} },
		.outputs = { { hiz, ResourceUsage::Type::ShaderWrite } }
	};
}

void setupBarrier(vk::CommandBuffer& commandBuffer, uint32_t mip, Image& image) {
	vk::ImageMemoryBarrier2 barrier {
		.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
		.srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
		.dstAccessMask = vk::AccessFlagBits2::eShaderRead,
		.oldLayout = vk::ImageLayout::eGeneral,
		.newLayout = vk::ImageLayout::eGeneral,
		.image = image.image,
		.subresourceRange = {
		    .aspectMask = image.getAspectFlags(),
			.baseMipLevel = mip - 1,
			.levelCount = 1,
			.layerCount = 1,
		},
	};

	commandBuffer.pipelineBarrier2(
		vk::DependencyInfo {
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &barrier,
		}
	);
}

void build(Task::BuildContext& context) {
	auto hiz = context.getOutput<Image&>(0);
	auto material = context.getData<HiZData>().material;

	for (int mip = 1; mip < hiz.mipLevels; mip++) {
		if (mip > 1) setupBarrier(context.commandBuffer, mip, hiz);

		glm::uvec3 mipSize(hiz.size.width >> mip, hiz.size.height >> mip, 1);

		context.commandBuffer.pushConstants(
			context.materialManager.getPipeline(material).pipelineLayout,
			vk::ShaderStageFlagBits::eCompute,
			0,
			sizeof(uint32_t),
			&mip
		);

		ComputePass(context, material, mipSize);
	}
}

TaskIndex rendergraph::passes::core::hiz(PassBuildContext& context, TaskIndex gbuffer) {
	auto material = context.materialManager.registerComputeMaterial(
		"hiz",
		{
			"resources/shaders/hiz.slang"
	  },
		{
			{ vk::DescriptorType::eSampledImage },
			{ vk::DescriptorType::eStorageImage, 16 },
		}
	);
	return context.renderGraph.addTask(
		"hiz",
		{
			.setup = setup,
			.build = build,
		},
		HiZData {
			.gbuffer = gbuffer,
			.material = material,
		}
	);
}
