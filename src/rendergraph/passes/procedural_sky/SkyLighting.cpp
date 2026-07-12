#include "../ComputePass.hpp"
#include "rendergraph/BuildContext.hpp"
#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/SetupContext.hpp"

struct SkyLightingData {
	MaterialIndex material;
	TaskIndex skyviewLUT;
};

TaskIndex rendergraph::passes::procedural_sky::skyLighting(PassBuildContext& context, TaskIndex skyviewLUT) {
	return context.renderGraph.addTask(
		"skyLighting",
		{
			.setup = [](Task::SetupContext& context) -> Task::Dependencies {
				auto skyviewLUTPass = context.getData<SkyLightingData>().skyviewLUT;

				auto skyviewLUT = context.getReference(skyviewLUTPass, 0);

				auto skyLightingSH = context.createBuffer(
					"skyLightingSH",
					{
						.size = sizeof(glm::vec4) * 9,
						.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eUniformBuffer,
					}
				);
				return {
					.inputs = {
						{ skyviewLUT, ResourceUsage::Type::SampledRead },
                       },
					.outputs = {
						{ skyLightingSH, ResourceUsage::Type::StorageBufferWrite },
					},
				};
			},

			.build = [](
						 Task::BuildContext& context
					 ) { ComputePass(context, context.getData<SkyLightingData>().material, { 1, 1, 1 }); },
		},
		SkyLightingData {
			.material = context.materialManager.getMaterialIndex("sky_lighting"),
			.skyviewLUT = skyviewLUT,
		}
	);
}
