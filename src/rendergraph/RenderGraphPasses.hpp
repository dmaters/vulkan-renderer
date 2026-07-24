#pragma once
#include "RenderGraph.hpp"

namespace rendergraph::passes {
struct ExternalResources {
	ResourceIndex pbrMaterialData;
	ResourceIndex pbrMaterialInstances;
};
struct PassBuildContext {
	RenderGraph& renderGraph;
	MaterialManager& materialManager;
};

namespace util {
	TaskIndex imageCopy(
		PassBuildContext&, TaskIndex origin, std::size_t originSlot, TaskIndex destination, std::size_t destinationSlot
	);
};

namespace procedural_sky {
	TaskIndex transmittanceLUT(PassBuildContext&);
	TaskIndex multiscatteringLUT(PassBuildContext&, TaskIndex transmittanceLUT);
	TaskIndex skyviewLUT(
		PassBuildContext&, TaskIndex sceneData, TaskIndex transmittanceLUT, TaskIndex multiscatteringLUT
	);
	TaskIndex skyLighting(PassBuildContext&, TaskIndex skyviewLUT);
	TaskIndex skybox(
		PassBuildContext&, TaskIndex sceneData, TaskIndex skyviewLUT, TaskIndex hdrOutput, TaskIndex gbuffer
	);
};	// namespace procedural_sky

namespace core {
	enum SceneDataSlots {
		Camera,
		Lights,
	};
	TaskIndex sceneData(PassBuildContext&);

	enum GBufferSlots {
		Albedo,
		Normal,
		WorldPos,
		RoughnessMetallic,
		Depth,
	};

	TaskIndex gbuffer(PassBuildContext&, const ExternalResources& resources, TaskIndex sceneData);
	TaskIndex hiz(PassBuildContext&, TaskIndex gbuffer);
	TaskIndex shadows(PassBuildContext&, TaskIndex sceneData);
	TaskIndex deferredLighting(
		PassBuildContext&,
		TaskIndex hdrOutput,
		TaskIndex gbuffer,
		TaskIndex shadows,
		TaskIndex sceneData,
		TaskIndex skyLighting
	);
	TaskIndex ssrChainGen(PassBuildContext& context, TaskIndex hdrOutput);
	TaskIndex ssr(
		PassBuildContext& context,
		TaskIndex sceneData,
		TaskIndex gbuffer,
		TaskIndex ssrChain,
		TaskIndex hiz,
		TaskIndex hdrOutput
	);
	TaskIndex ui(PassBuildContext&, TaskIndex sdrOutput);

	TaskIndex sdrOutput(PassBuildContext&);
	TaskIndex hdrOutput(PassBuildContext&);

}  // namespace core

namespace post_processing {
	TaskIndex composition(PassBuildContext&, TaskIndex hdrOutput, TaskIndex sdrOutput);
	TaskIndex fxaa(PassBuildContext&, TaskIndex sdrCopy, TaskIndex sdrOutput);

}  // namespace post_processing

};	// namespace rendergraph::passes
