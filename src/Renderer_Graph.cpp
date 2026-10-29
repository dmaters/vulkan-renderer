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
void Renderer::registerPasses() {
	m_sharedSceneBuffers.vertices = m_taskRegister.registerStaticBuffer("vertex_positions_buffer");
	m_sharedSceneBuffers.vertexAttributes = m_taskRegister.registerStaticBuffer("vertex_attributes_buffer");
	m_sharedSceneBuffers.indices = m_taskRegister.registerStaticBuffer("index_buffer");
	m_sharedSceneBuffers.transforms = m_taskRegister.registerStaticBuffer("transforms_buffer");
	m_sharedSceneBuffers.materialData = m_taskRegister.registerStaticBuffer("pbr_data_buffer");
	m_sharedSceneBuffers.primitiveData = m_taskRegister.registerStaticBuffer("primitive_data");
	m_sharedSceneBuffers.tlas = m_taskRegister.registerStaticBuffer("tlas");

	PassBuildContext context {
		.taskRegister = m_taskRegister,
		.materialManager = m_materialManager,
		.renderingConfiguration = m_configuration,
	};

	auto sceneData = core::sceneData(context);

	auto transmittanceLUT = procedural_sky::transmittanceLUT(context);
	auto multiscatteringLUT = procedural_sky::multiscatteringLUT(context, transmittanceLUT);
	auto skyviewLUT = procedural_sky::skyviewLUT(context, sceneData, transmittanceLUT, multiscatteringLUT);
	auto skylighting = procedural_sky::skyLighting(context, skyviewLUT);

	auto shadowBuffer = core::shadows(context, sceneData);
	auto gbuffer =
		core::gbuffer(context, { m_sharedSceneBuffers.materialData, m_sharedSceneBuffers.primitiveData }, sceneData);

	auto hiz = core::hiz(context, gbuffer);
	auto hdrOutput = core::hdrOutput(context);
	auto lighting = core::deferredLighting(context, hdrOutput, gbuffer, shadowBuffer, sceneData, skylighting);

	auto hdrOutputFeature = m_featureRegister.addFeature("HDR Output");
	m_featureRegister.addTask(hdrOutputFeature, lighting, "Deferred Lighting");

	auto skybox = procedural_sky::skybox(context, sceneData, skyviewLUT, hdrOutput, gbuffer);
	m_featureRegister.addTask(hdrOutputFeature, skybox, "Procedural Skybox");

	auto hdrCopyTask = core::hdrOutput(context);
	auto hdrCopy = util::imageCopy(context, hdrOutput, 0, hdrCopyTask, 0);
	auto ssr = core::ssr(context, sceneData, gbuffer, hdrCopy, hiz, hdrOutput);
	m_featureRegister.addTask(hdrOutputFeature, ssr, "SSR");

	auto sdrOutput = core::sdrOutput(context);
	auto composition = post_processing::composition(context, hdrOutput, sdrOutput);
	auto compositionFeature = m_featureRegister.addFeature("Composition");
	m_featureRegister.addTask(compositionFeature, composition, "GT7 Tonemapping");

	auto sdrCopyTask = core::sdrOutput(context);
	auto sdrCopy = util::imageCopy(context, sdrOutput, 0, sdrCopyTask, 0);

	auto fxaa = post_processing::fxaa(context, sdrCopy, sdrOutput);
	auto postProcessingFeature = m_featureRegister.addFeature("Post Processing");
	m_featureRegister.addTask(postProcessingFeature, fxaa, "FXAA");

	auto ui = core::ui(context, sdrOutput);
}
