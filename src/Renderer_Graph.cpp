#include <vector>

#include "Renderer.hpp"
#include "rendergraph/RenderGraphPasses.hpp"
#include "resources/ResourceManager.hpp"

struct ProceduralSkyOutputPasses {
	TaskIndex skyLighting;
	TaskIndex skyviewLUT;
};

struct ProceduralSkyRequiredPasses {
	TaskIndex sceneUpdatePass;
	TaskIndex gbufferPass;
};

using namespace rendergraph::passes;
std::vector<TaskIndex> Renderer::createRenderGraph(Scene& scene) {
	std::vector<TaskIndex> optionalPasses;
	auto buffers = m_resourceManager.getBuffers(scene.allocation);

	m_graph.registerBuffer("vertex_positions_buffer", buffers[0]);

	m_graph.registerBuffer("vertex_attributes_buffer", buffers[1]);
	m_graph.registerBuffer("index_buffer", buffers[2]);
	m_graph.registerBuffer("instance_buffer", buffers[3]);

	auto pbrMaterialData = m_graph.registerBuffer("pbr_data_buffer", buffers[5]);
	auto pbrMaterialInstances = m_graph.registerBuffer("pbr_instances_buffer", buffers[4]);

	PassBuildContext context {
		.renderGraph = m_graph,
		.materialManager = m_materialManager,
		.renderingConfiguration = m_configuration,
	};
	auto sceneData = core::sceneData(context);

	auto transmittanceLUT = procedural_sky::transmittanceLUT(context);
	auto multiscatteringLUT = procedural_sky::multiscatteringLUT(context, transmittanceLUT);
	auto skyviewLUT = procedural_sky::skyviewLUT(context, sceneData, transmittanceLUT, multiscatteringLUT);
	auto skylighting = procedural_sky::skyLighting(context, skyviewLUT);

	auto shadowBuffer = core::shadows(context, sceneData);
	auto gbuffer = core::gbuffer(context, { pbrMaterialData, pbrMaterialInstances }, sceneData);

	auto hiz = core::hiz(context, gbuffer);
	auto hdrOutput = core::hdrOutput(context);
	auto lighting = core::deferredLighting(context, hdrOutput, gbuffer, shadowBuffer, sceneData, skylighting);
	optionalPasses.push_back(lighting);

	auto skybox = procedural_sky::skybox(context, sceneData, skyviewLUT, hdrOutput, gbuffer);
	optionalPasses.push_back(skybox);

	auto hdrCopyTask = core::hdrOutput(context);
	auto hdrCopy = util::imageCopy(context, hdrOutput, 0, hdrCopyTask, 0);
	auto ssr = core::ssr(context, sceneData, gbuffer, hdrCopy, hiz, hdrOutput);
	optionalPasses.push_back(ssr);

	auto sdrOutput = core::sdrOutput(context);
	auto composition = post_processing::composition(context, hdrOutput, sdrOutput);
	optionalPasses.push_back(composition);

	auto sdrCopyTask = core::sdrOutput(context);
	auto sdrCopy = util::imageCopy(context, sdrOutput, 0, sdrCopyTask, 0);

	auto fxaa = post_processing::fxaa(context, sdrCopy, sdrOutput);
	optionalPasses.push_back(fxaa);

	auto ui = core::ui(context, sdrOutput);
	optionalPasses.push_back(ui);
	m_optionalPasses = optionalPasses;
	m_graph.update(ui, m_optionalPasses, scene);

	return optionalPasses;
}
