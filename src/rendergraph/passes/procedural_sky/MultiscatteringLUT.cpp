#include "../ComputePass.hpp"
#include "rendergraph/BuildContext.hpp"
#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/SetupContext.hpp"

struct MultiScatteringLUTData {
	TaskIndex transmittanceLUTPass;
	MaterialIndex material;
};

TaskIndex rendergraph::passes::procedural_sky::multiscatteringLUT(
	PassBuildContext& context, TaskIndex transmittanceLUT
) {
	return context.renderGraph.addTask(
		"multiscatteringLUT",
		{
			.setup = [](Task::SetupContext& context) -> Task::Dependencies {
				auto data = context.getData<MultiScatteringLUTData>();

				auto transmittanceLUT = context.getReference(data.transmittanceLUTPass, 0);
				auto scratch_buffer = context.resourceProvider.createBuffer(
					"scratch_buffer",

					{
						.size = 1 << 23,  // 8MB
						.usage = vk::BufferUsageFlagBits::eStorageBuffer,

					}
				);
				auto multiscatteringLUT = context.resourceProvider.createImage(
					"multiscatteringLUT",
					{
						.width = 64,
						.height = 64,
						.depth = 1,
						.miplevels = 1,
						.format = vk::Format::eR16G16B16A16Sfloat,
						.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,

					}
				);

				return {
					.inputs = { { transmittanceLUT, ResourceUsage::Type::SampledRead } },
					.outputs = { { multiscatteringLUT, ResourceUsage::Type::ShaderWrite },
								 { scratch_buffer, ResourceUsage::Type::ShaderWrite } }
				};
			},
			.build =
				[](Task::BuildContext& context) {
					auto material = context.getData<MultiScatteringLUTData>().material;
					ComputePass(context, material, { 64, 64, 1 });
				},
		},
		MultiScatteringLUTData {
			.transmittanceLUTPass = transmittanceLUT,
			.material = context.materialManager.getMaterialIndex("multiscatteringLUT"),
		}
	);
}
