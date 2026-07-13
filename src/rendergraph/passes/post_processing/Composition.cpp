#include "../ComputePass.hpp"
#include "rendergraph/BuildContext.hpp"
#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/SetupContext.hpp"

struct CompositionData {
	TaskIndex hdrOutput;
	TaskIndex sdrOutput;
	MaterialIndex material;
};

TaskIndex rendergraph::passes::post_processing::composition(
	PassBuildContext& context, TaskIndex hdrOutput, TaskIndex sdrOutput
) {
	return context.renderGraph.addTask(
		"composition",
		{ .setup = [](Task::SetupContext& context) -> Task::Dependencies {
			 return {
				.inputs = {
					 { context.getReference(context.getData<CompositionData>().hdrOutput, 0), ResourceUsage::Type::ShaderRead },
				},
				.outputs = {
					 { context.getReference(context.getData<CompositionData>().sdrOutput, 0), ResourceUsage::Type::ShaderWrite },
				},
			 };
		 },
		  .build =
			  [](Task::BuildContext& context) {
				  auto& size = context.getInput<Image&>(0).size;

				  ComputePass(
					  context, context.getData<CompositionData>().material, glm::uvec3(size.width, size.height, 1)
				  );
			  } },
		CompositionData {
			.hdrOutput = hdrOutput,
			.sdrOutput = sdrOutput,
			.material = context.materialManager.getMaterialIndex("composition"),
		}

	);
}
