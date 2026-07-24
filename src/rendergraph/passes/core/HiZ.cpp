#include "../ComputePass.hpp"
#include "rendergraph/BuildContext.hpp"
#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/SetupContext.hpp"

struct HiZData {
	TaskIndex gbuffer;
	MaterialIndex material;
};

Task::Dependencies setup(Task::SetupContext& context) {
	auto resolution = context.scene.camera.getResolution();

	uint32_t minSize = std::min(resolution.x, resolution.y);
	uint32_t mipLevels = std::floor(std::log2(minSize)) + 1;
	mipLevels = std::max(1u, mipLevels - 1);

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

void build(Task::BuildContext& context) {
	auto hiz = context.getOutput<Image&>(0);
	auto material = context.getData<HiZData>().material;

	for (int mip = 1; mip < std::log2(std::min(hiz.size.width, hiz.size.height)); mip++) {
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
