
#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/SetupContext.hpp"

TaskIndex rendergraph::passes::core::sdrOutput(PassBuildContext &context) {
	return context.renderGraph.addTask(
		"sdr_output",
		{ .setup = [](Task::SetupContext &context) -> Task::Dependencies {
			auto resolution = context.scene.camera.getResolution();

			rendergraph::ResourceIndex sdrOutput = context.createImage(
				"sdr_output",
				{
					.width = static_cast<uint32_t>(resolution.x),
					.height = static_cast<uint32_t>(resolution.y),
					.depth = 1,
					.miplevels = 1,
					.format = vk::Format::eR8G8B8A8Unorm,
					.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled |
							 vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eColorAttachment |
							 vk::ImageUsageFlagBits::eTransferDst,
				}

			);

			return {
				.outputs = { { sdrOutput, ResourceUsage::Type::None }, },
			};
		}

		}
	);
}
