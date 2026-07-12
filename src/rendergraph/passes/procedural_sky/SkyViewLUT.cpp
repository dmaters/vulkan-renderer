#include "../ComputePass.hpp"
#include "rendergraph/BuildContext.hpp"
#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/SetupContext.hpp"

struct SkyViewLUTData {
	MaterialIndex material;
	TaskIndex transmittanceLUTPass;
	TaskIndex multiscatteringLUTPass;
	TaskIndex scenePass;
};
TaskIndex rendergraph::passes::procedural_sky::skyviewLUT(
	PassBuildContext& context, TaskIndex sceneData, TaskIndex transmittanceLUT, TaskIndex multiscatteringLUT
) {
	return context.renderGraph.addTask(
		"skyviewLUT",
		{
			.setup = [](Task::SetupContext& context) -> Task::Dependencies {
				auto data = context.getData<SkyViewLUTData>();

				auto lightBuffer = context.getReference(data.scenePass, core::SceneDataSlots::Lights);
				auto transmittanceLUT = context.getReference(data.transmittanceLUTPass, 0);
				auto multiscatteringLUT = context.getReference(data.multiscatteringLUTPass, 0);

				auto skyviewLUT = context.createImage(
					"skyviewLUT",
					{
						.width = 200,
						.height = 100,
						.depth = 1,
						.miplevels = 1,
						.format = vk::Format::eR16G16B16A16Sfloat,
						.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
					}
				);
				return {
					.inputs =
					    {
							 { lightBuffer, ResourceUsage::Type::UniformBuffer },
							 { transmittanceLUT, ResourceUsage::Type::SampledRead },
							 { multiscatteringLUT, ResourceUsage::Type::SampledRead },
						},
					.outputs =
				        {
							 { skyviewLUT, ResourceUsage::Type::ShaderWrite },
						},
				};
			},

			.build = [](
						 Task::BuildContext& context
					 ) { ComputePass(context, context.getData<SkyViewLUTData>().material, { 200, 100, 1 }); },
		},
		SkyViewLUTData {
			.material = context.materialManager.getMaterialIndex("skyviewLUT"),
			.transmittanceLUTPass = transmittanceLUT,
			.multiscatteringLUTPass = multiscatteringLUT,
			.scenePass = sceneData,
		}
	);
}
