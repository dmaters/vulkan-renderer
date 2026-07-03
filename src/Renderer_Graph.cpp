#include <vector>

#include "Renderer.hpp"
#include "material/MaterialDefinitions.hpp"
#include "rendergraph/ResourceUsage.hpp"
#include "rendergraph/SetupContext.hpp"
#include "rendergraph/Task.hpp"
#include "rendergraph/tasks/ComputePass.hpp"
#include "rendergraph/tasks/DrawPassQuad.hpp"
#include "rendergraph/tasks/FrustumCulling.hpp"
#include "rendergraph/tasks/GBufferPass.hpp"
#include "rendergraph/tasks/RenderPass.hpp"
#include "rendergraph/tasks/SceneUpdatePass.hpp"
#include "rendergraph/tasks/ShadowPass.hpp"
#include "rendergraph/tasks/UIPass.hpp"
#include "resources/ResourceManager.hpp"

struct ProceduralSkyOutputPasses {
	TaskIndex skyLighting;
};

struct ProceduralSkyRequiredPasses {
	TaskIndex sceneUpdatePass;
	TaskIndex hdrOutput;
};
ProceduralSkyOutputPasses proceduralSky(
	std::vector<TaskIndex>& passes,
	RenderGraph& graph,
	MaterialManager& materialManager,
	ProceduralSkyRequiredPasses previousPasses
) {
	TaskIndex transmittanceLUTPass = graph.addTask(
		"transmittanceLUT",
		Task::Type::Compute,
		{
			.setup =
				[](Task::SetupContext& context) {
					auto transmittanceLUT =
						context.resourceProvider.createImage(
							"transmittanceLUT",
							{
								.width = 256,
								.height = 64,
								.depth = 1,
								.miplevels = 1,
								.format = vk::Format::eR16G16B16A16Sfloat,
								.usage = vk::ImageUsageFlagBits::eStorage |
		                                 vk::ImageUsageFlagBits::eSampled,

							}
						);

					context.registerOutput(
						transmittanceLUT, ResourceUsage::Type::ShaderWrite
					);
				},
			.build =
				[](Task::BuildContext& context) {
					auto material = context.getData<MaterialIndex>();
					ComputePass(context, material, { 256, 64, 1 });
				},
		},
		materialManager.getMaterialIndex("transmittanceLUT")
	);
	passes.push_back(transmittanceLUTPass);

	struct MultiScatteringLUTData {
		TaskIndex transmittanceLUT;
		MaterialIndex material;
	};

	TaskIndex multiscatteringLUTPass = graph.addTask(
		"multiscatteringLUT",
		Task::Type::Compute,
		{
			.setup =
				[](Task::SetupContext& context) {
					auto data = context.getData<MultiScatteringLUTData>();

					context.registerInput(
						data.transmittanceLUT,
						0,
						ResourceUsage::Type::SampledRead
					);

					auto scratch_buffer = context.resourceProvider.createBuffer(
						"scratch_buffer",

						{
							.size = 1 << 23,  // 8MB
							.usage = vk::BufferUsageFlagBits::eStorageBuffer,

						}
					);
					auto multiscatteringLUT =
						context.resourceProvider.createImage(
							"multiscatteringLUT",
							{
								.width = 64,
								.height = 64,
								.depth = 1,
								.miplevels = 1,
								.format = vk::Format::eR16G16B16A16Sfloat,
								.usage = vk::ImageUsageFlagBits::eStorage |
		                                 vk::ImageUsageFlagBits::eSampled,

							}
						);
					context.registerOutput(
						multiscatteringLUT, ResourceUsage::Type::ShaderWrite
					);
					context.registerOutput(
						scratch_buffer, ResourceUsage::Type::ShaderWrite
					);
				},
			.build =
				[](Task::BuildContext& context) {
					auto material =
						context.getData<MultiScatteringLUTData>().material;
					ComputePass(context, material, { 64, 64, 1 });
				},
		},
		MultiScatteringLUTData {
			.transmittanceLUT = transmittanceLUTPass,
			.material = materialManager.getMaterialIndex("multiscatteringLUT"),
		}
	);

	passes.push_back(multiscatteringLUTPass);

	struct SkyViewLUTData {
		MaterialIndex material;
		TaskIndex transmittanceLUTPass;
		TaskIndex multiscatteringLUTPass;
		TaskIndex scenePass;
	};

	TaskIndex skyviewLUTPass = graph.addTask(
		"skyviewLUT",
		Task::Type::Compute,
		{
			.setup =
				[](Task::SetupContext& context) {
					auto data = context.getData<SkyViewLUTData>();

					context.registerInput(
						data.scenePass, 1, ResourceUsage::Type::UniformBuffer
					);

					context.registerInput(
						data.transmittanceLUTPass,
						0,
						ResourceUsage::Type::SampledRead
					);

					context.registerInput(
						data.multiscatteringLUTPass,
						0,
						ResourceUsage::Type::SampledRead
					);

					auto lut = context.createImage(
						"skyviewLUT",
						{
							.width = 200,
							.height = 100,
							.depth = 1,
							.miplevels = 1,
							.format = vk::Format::eR16G16B16A16Sfloat,
							.usage = vk::ImageUsageFlagBits::eStorage |
		                             vk::ImageUsageFlagBits::eSampled,

						}
					);
					context.registerOutput(
						lut, ResourceUsage::Type::ShaderWrite
					);
				},

			.build =
				[](Task::BuildContext& context) {
					ComputePass(
						context,
						context.getData<SkyViewLUTData>().material,
						{ 200, 100, 1 }
					);
				},
		},
		SkyViewLUTData {
			.material = materialManager.getMaterialIndex("skyviewLUT"),
			.transmittanceLUTPass = transmittanceLUTPass,
			.multiscatteringLUTPass = multiscatteringLUTPass,
			.scenePass = previousPasses.sceneUpdatePass,
		}
	);

	passes.push_back(skyviewLUTPass);

	struct SkyLightingData {
		MaterialIndex material;
		TaskIndex transmittanceLUTPass;
	};
	TaskIndex skyLighting = graph.addTask(
		"skyLighting",
		Task::Type::Compute,
		{
			.setup =
				[](Task::SetupContext& context) {
					auto transmittanceLUTPass =
						context.getData<SkyLightingData>().transmittanceLUTPass;

					context.registerInput(
						transmittanceLUTPass,
						0,
						ResourceUsage::Type::SampledRead
					);

					auto skyLightingSH = context.createBuffer(
						"skyLightingSH",
						{
							.size = sizeof(glm::vec4) * 9,
							.usage = vk::BufferUsageFlagBits::eStorageBuffer |
		                             vk::BufferUsageFlagBits::eUniformBuffer,
						}
					);

					context.registerOutput(
						skyLightingSH, ResourceUsage::Type::StorageBufferWrite
					);
				},

			.build =
				[](Task::BuildContext& context) {
					ComputePass(
						context,
						context.getData<SkyLightingData>().material,
						{ 1, 1, 1 }
					);
				},
		},
		SkyLightingData {
			.material = materialManager.getMaterialIndex("sky_lighting"),
			.transmittanceLUTPass = transmittanceLUTPass,
		}
	);
	passes.push_back(skyLighting);

	struct SkyboxData {
		TaskIndex sceneUpdatePass;
		TaskIndex hdrOutputPass;
		TaskIndex skyviewLUTPass;

		MaterialIndex skyboxMaterial;
	};

	TaskIndex skyboxPass = graph.addTask(
		"skybox",
		Task::Type::Graphic,
		{

			.setup =
				[](Task::SetupContext& context) {
					auto data = context.getData<SkyboxData>();
					context.registerInput(
						data.sceneUpdatePass,
						0,
						ResourceUsage::Type::UniformBuffer
					);
					context.registerInput(
						data.skyviewLUTPass, 0, ResourceUsage::Type::SampledRead
					);

					context.registerOutput(
						data.hdrOutputPass,
						0,
						ResourceUsage::Type::ColorAttachmentWrite
					);
					context.registerOutput(
						data.hdrOutputPass,
						1,
						ResourceUsage::Type::DepthStencilRead
					);
				},

			.build =
				[](Task::BuildContext& context) {
					RenderPass::Begin(
						context, AttachmentOp::ReadWrite, AttachmentOp::Read
					);
					DrawPassQuad(
						context, context.getData<SkyboxData>().skyboxMaterial
					);

					RenderPass::End(context.commandBuffer);
				},
		},
		SkyboxData {
			.sceneUpdatePass = previousPasses.sceneUpdatePass,
			.hdrOutputPass = previousPasses.hdrOutput,
			.skyviewLUTPass = skyviewLUTPass,
			.skyboxMaterial = materialManager.getMaterialIndex("skybox"),
		}
	);
	passes.push_back(skyboxPass);
}

void Renderer::createRenderGraph(Scene& scene) {
	const std::vector<BufferHandle>& buffers =
		m_resourceManager.getBuffers(scene.allocation);

	m_graph.registerBuffer("vertex_positions_buffer", buffers[0]);
	m_graph.registerBuffer("vertex_attributes_buffer", buffers[1]);
	m_graph.registerBuffer("index_buffer", buffers[2]);
	m_graph.registerBuffer("instance_buffer", buffers[3]);

	std::vector<TaskIndex> passes;

	rendergraph::ResourceIndex pbrMaterialData =
		m_graph.registerBuffer("pbr_data_buffer", buffers[5]);
	rendergraph::ResourceIndex pbrMaterialInstances =
		m_graph.registerBuffer("pbr_instances_buffer", buffers[4]);

	TaskIndex sceneUpdate = m_graph.addTask(
		"scene_update",
		Task::Type::Transfer,
		{
			.setup = SceneData::Setup,
			.build = SceneData::Build,
		}
	);
	passes.push_back(sceneUpdate);
	TaskIndex shadowMapPass = m_graph.addTask(
		"shadow_map",
		Task::Type::Graphic,
		{
			.setup = ShadowPass::Setup,
			.build = ShadowPass::Build,

		}
	);
	passes.push_back(shadowMapPass);

	TaskIndex gbufferPass = m_graph.addTask(
		"gbuffer",
		Task::Type::Graphic,
		{
			.setup = GBUfferPass::Setup,
			.build = GBUfferPass::Build,
		},
		GBUfferPass { .sceneUpdatePass = sceneUpdate, .}
	);

	ResourceIndex hdr_output = m_graph.createImage(
		"hdr_output",
		{
			.width = 1280,
			.height = 720,
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eR16G16B16A16Sfloat,
			.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                 vk::ImageUsageFlagBits::eSampled |
	                 vk::ImageUsageFlagBits::eStorage,
		},
		1
	);
	TaskIndex lightingDeferred = m_graph.addTask(
		"lighting_deferred",
		TaskType::Graphic,
		{
			{ cameraBuffer,      ResourceUsage::Type::UniformBuffer },
			{ lightBuffer,       ResourceUsage::Type::UniformBuffer },
			{ albedo,            ResourceUsage::Type::SampledRead   },
			{ normal,            ResourceUsage::Type::SampledRead   },
			{ worldPos,          ResourceUsage::Type::SampledRead   },
			{ roughnessMetallic, ResourceUsage::Type::SampledRead   },
			{ shadowAtlas,       ResourceUsage::Type::SampledRead   },
			{ skyLightingSH,     ResourceUsage::Type::UniformBuffer },
    },
		{
			{ hdr_output, ResourceUsage::Type::ColorAttachmentWrite },
			{ depth, ResourceUsage::Type::DepthStencilRead },
		},
		[material = m_materialManager.getMaterialIndex("lighting_deferred")](
			TaskBuildContext& context
		) {
			RenderPassBegin(
				context, AttachmentOp::ClearWrite, AttachmentOp::Read
			);
			DrawPassQuad(context, material);
			RenderPassEnd(context);
		}
	);
	passes.push_back(lightingDeferred);

	ResourceIndex tonemapped = m_graph.createImage(
		"tonemapped",
		{
			.width = 1280,
			.height = 720,
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eR8G8B8A8Unorm,
			.usage = vk::ImageUsageFlagBits::eStorage |
	                 vk::ImageUsageFlagBits::eSampled,

		},
		1
	);
	ResourceIndex final = m_graph.createImage(
		"final",
		{
			.width = 1280,
			.height = 720,
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eR8G8B8A8Unorm,
			.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                 vk::ImageUsageFlagBits::eTransferSrc,

		},
		1
	);
	TaskIndex compositionPass = m_graph.addTask(
		"composition",
		TaskType::Compute,
		{
			{ hdr_output, ResourceUsage::Type::ShaderRead },
    },
		{
			{ tonemapped, ResourceUsage::Type::ShaderWrite },
		},
		[composition = m_materialManager.getMaterialIndex("composition")](
			TaskBuildContext& context
		) {
			ImageHandle input = context.images[context.inputs[0].resource];
			auto dispatch = context.resourceManager.getImage(input).size;

			ComputePass(
				context,
				composition,
				glm::uvec3(dispatch.width, dispatch.height, 1)
			);
		}
	);

	passes.push_back(compositionPass);

	TaskIndex fxaa = m_graph.addTask(
		"fxaa",
		TaskType::Graphic,
		{
			{ tonemapped, ResourceUsage::Type::SampledRead },
    },
		{
			{ final, ResourceUsage::Type::ColorAttachmentWrite },
		},
		[material = m_materialManager.getMaterialIndex("fxaa")](
			TaskBuildContext& context
		) {
			RenderPassBegin(
				context, AttachmentOp::ClearWrite, AttachmentOp::Read
			);
			DrawPassQuad(context, material);
			RenderPassEnd(context);
		}
	);
	passes.push_back(fxaa);

	TaskIndex ui = m_graph.addTask(
		"UI",
		TaskType::Graphic,
		{
    },
		{
			{ final, ResourceUsage::Type::ColorAttachmentWrite },
		},
		[](TaskBuildContext& context) {
			RenderPassBegin(
				context, AttachmentOp::ReadWrite, AttachmentOp::ReadWrite
			);
			UIPass(context);
			RenderPassEnd(context);
		}
	);
	passes.push_back(ui);

	m_graph.update(passes, final);
}
