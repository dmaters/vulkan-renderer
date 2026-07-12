#include "../ComputePass.hpp"
#include "rendergraph/BuildContext.hpp"
#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/SetupContext.hpp"

TaskIndex rendergraph::passes::procedural_sky::transmittanceLUT(PassBuildContext& context) {
	return context.renderGraph.addTask(
		"transmittanceLUT",
		{
			.setup = [](Task::SetupContext& context) -> Task::Dependencies {
				auto transmittanceLUT = context.resourceProvider.createImage(
					"transmittanceLUT",
					{
						.width = 256,
						.height = 64,
						.depth = 1,
						.miplevels = 1,
						.format = vk::Format::eR16G16B16A16Sfloat,
						.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,

					}
				);

				return {
					.outputs = { { transmittanceLUT, ResourceUsage::Type::ShaderWrite } },
				};
			},
			.build =
				[](Task::BuildContext& context) {
					auto material = context.getData<MaterialIndex>();
					ComputePass(context, material, { 256, 64, 1 });
				},
		},
		context.materialManager.getMaterialIndex("transmittanceLUT")
	);
}
