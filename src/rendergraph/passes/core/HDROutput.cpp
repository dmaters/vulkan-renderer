#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/SetupContext.hpp"

static Task::Dependencies setup(Task::SetupContext& context) {
	auto resolution = context.renderingConfiguration.resolution;
	rendergraph::ResourceIndex hdr_output = context.createImage(
		"hdr_output",
		{
			.width = static_cast<uint32_t>(resolution.x),
			.height = static_cast<uint32_t>(resolution.y),
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eR16G16B16A16Sfloat,
			.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled |
					 vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc,
		}
	);

	return {
		.outputs = { { hdr_output, ResourceUsage::Type::None } },
	};
}

TaskIndex rendergraph::passes::core::hdrOutput(PassBuildContext& context) {
	return context.renderGraph.addTask(
		"hdr_output",
		{
			.setup = setup,
		}
	);
}
