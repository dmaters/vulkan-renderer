#include <vector>

#include "Renderer.hpp"
#include "rendergraph/ResourceUsage.hpp"
#include "rendergraph/SetupContext.hpp"
#include "rendergraph/Task.hpp"
#include "rendergraph/tasks/ComputePass.hpp"
#include "rendergraph/tasks/DrawPass.hpp"
#include "rendergraph/tasks/GBufferPass.hpp"
#include "rendergraph/tasks/RenderPass.hpp"
#include "rendergraph/tasks/SceneData.hpp"
#include "rendergraph/tasks/ShadowPass.hpp"
#include "rendergraph/tasks/UIPass.hpp"
#include "resources/ResourceManager.hpp"

struct ProceduralSkyOutputPasses {
	TaskIndex skyLighting;
	TaskIndex skyviewLUT;
};

struct ProceduralSkyRequiredPasses {
	TaskIndex sceneUpdatePass;
	TaskIndex gbufferPass;
};
ProceduralSkyOutputPasses proceduralSky(
	std::vector<TaskIndex>& passes,
	RenderGraph& graph,
	MaterialManager& materialManager,
	ProceduralSkyRequiredPasses previousPasses
) {
	TaskIndex transmittanceLUTPass = graph.addTask(
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
		materialManager.getMaterialIndex("transmittanceLUT")
	);
	passes.push_back(transmittanceLUTPass);

	struct MultiScatteringLUTData {
		TaskIndex transmittanceLUTPass;
		MaterialIndex material;
	};

	TaskIndex multiscatteringLUTPass = graph.addTask(
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
			.transmittanceLUTPass = transmittanceLUTPass,
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
		{
			.setup = [](Task::SetupContext& context) -> Task::Dependencies {
				auto data = context.getData<SkyViewLUTData>();

				auto lightBuffer = context.getReference(data.scenePass, SceneData::Slot::Lights);
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
			.material = materialManager.getMaterialIndex("skyviewLUT"),
			.transmittanceLUTPass = transmittanceLUTPass,
			.multiscatteringLUTPass = multiscatteringLUTPass,
			.scenePass = previousPasses.sceneUpdatePass,
		}
	);

	passes.push_back(skyviewLUTPass);

	struct SkyLightingData {
		MaterialIndex material;
		TaskIndex skyviewLUT;
	};
	TaskIndex skyLighting = graph.addTask(
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
			.material = materialManager.getMaterialIndex("sky_lighting"),
			.skyviewLUT = skyviewLUTPass,
		}
	);
	passes.push_back(skyLighting);

	return {
		.skyLighting = skyLighting,
		.skyviewLUT = skyviewLUTPass,
	};
}

std::vector<TaskIndex> Renderer::createRenderGraph(Scene& scene) {
	auto buffers = m_resourceManager.getBuffers(scene.allocation);

	m_graph.registerBuffer("vertex_positions_buffer", buffers[0]);
	m_graph.registerBuffer("vertex_attributes_buffer", buffers[1]);
	m_graph.registerBuffer("index_buffer", buffers[2]);
	m_graph.registerBuffer("instance_buffer", buffers[3]);

	std::vector<TaskIndex> passes;

	auto pbrMaterialData = m_graph.registerBuffer("pbr_data_buffer", buffers[5]);
	auto pbrMaterialInstances = m_graph.registerBuffer("pbr_instances_buffer", buffers[4]);

	TaskIndex sceneUpdate = m_graph.addTask(
		"scene_update",
		{
			.setup = SceneData::Setup,
			.build = SceneData::Build,
		}
	);
	passes.push_back(sceneUpdate);
	TaskIndex shadowMapPass = m_graph.addTask(
		"shadow_map",
		{
			.setup = ShadowPass::Setup,
			.build = ShadowPass::Build,

		},
		ShadowPass {
			.sceneUpdateTask = sceneUpdate,
			.material = m_materialManager.getMaterialIndex("shadowmap"),
		}
	);
	passes.push_back(shadowMapPass);

	TaskIndex gbufferPass = m_graph.addTask(
		"gbuffer",
		{
			.setup = GBUfferPass::Setup,
			.build = GBUfferPass::Build,
		},
		GBUfferPass {
			.sceneUpdatePass = sceneUpdate,
			.material = m_materialManager.getMaterialIndex("gbuffer"),
			.pbrMaterialData = pbrMaterialData,
			.pbrMaterialInstances = pbrMaterialInstances,
		}
	);
	passes.push_back(gbufferPass);

	auto proceduralSkyPasses = proceduralSky(
		passes,
		m_graph,
		m_materialManager,
		{
			.sceneUpdatePass = sceneUpdate,
			.gbufferPass = gbufferPass,
		}
	);

	struct LightingDeferredData {
		TaskIndex sceneUpdatePass;
		TaskIndex gbufferPass;
		TaskIndex shadowPass;
		TaskIndex skyLightingPass;

		MaterialIndex material;
	};

	TaskIndex lightingDeferred = m_graph.addTask(
		"lighting_deferred",
		{ .setup = [](Task::SetupContext& context) -> Task::Dependencies {
			 auto data = context.getData<LightingDeferredData>();

			 auto resolution = context.scene.camera.getResolution();
			 rendergraph::ResourceIndex hdr_output = context.createImage(
				 "hdr_output",
				 {
					 .width = static_cast<uint32_t>(resolution.x),
					 .height = static_cast<uint32_t>(resolution.y),
					 .depth = 1,
					 .miplevels = 1,
					 .format = vk::Format::eR16G16B16A16Sfloat,
					 .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled |
							  vk::ImageUsageFlagBits::eStorage,
				 }
			 );

			 return {
				 // Inputs
				 {
					 { context.getReference(data.sceneUpdatePass, SceneData::Slot::Camera),
					   ResourceUsage::Type::UniformBuffer },
					 { context.getReference(data.sceneUpdatePass, SceneData::Slot::Lights),
					   ResourceUsage::Type::UniformBuffer },
					 { context.getReference(data.gbufferPass, GBUfferPass::Slot::Albedo),
					   ResourceUsage::Type::SampledRead },
					 { context.getReference(data.gbufferPass, GBUfferPass::Slot::Normal),
					   ResourceUsage::Type::SampledRead },
					 { context.getReference(data.gbufferPass, GBUfferPass::Slot::WorldPos),
					   ResourceUsage::Type::SampledRead },
					 { context.getReference(data.gbufferPass, GBUfferPass::Slot::RoughnessMetallic),
					   ResourceUsage::Type::SampledRead },
					 { context.getReference(data.shadowPass, 0), ResourceUsage::Type::SampledRead },
					 { context.getReference(data.skyLightingPass, 0), ResourceUsage::Type::UniformBuffer },
				  },
				 // Outputs
				 {
					 { hdr_output, ResourceUsage::Type::ColorAttachmentWrite },
					 { context.getReference(data.gbufferPass, GBUfferPass::Slot::Depth),
					   ResourceUsage::Type::DepthStencilRead },
				  },
			 };
		 },
		  .build =
			  [](Task::BuildContext& context) {
				  RenderPass::Begin(context, AttachmentOp::ClearWrite, AttachmentOp::Read);
				  DrawPass::Quad(context, context.getData<LightingDeferredData>().material);
				  RenderPass::End(context.commandBuffer);
			  }

		},
		LightingDeferredData {
			.sceneUpdatePass = sceneUpdate,
			.gbufferPass = gbufferPass,
			.shadowPass = shadowMapPass,
			.skyLightingPass = proceduralSkyPasses.skyLighting,

			.material = m_materialManager.getMaterialIndex("lighting_deferred"),
		}

	);
	passes.push_back(lightingDeferred);

	struct SkyboxData {
		TaskIndex sceneUpdatePass;
		TaskIndex hdrOutputPass;
		TaskIndex skyviewLUTPass;

		MaterialIndex skyboxMaterial;
	};

	TaskIndex skyboxPass = m_graph.addTask(
		"skybox",
		{

			.setup = [](Task::SetupContext& context) -> Task::Dependencies {
				auto data = context.getData<SkyboxData>();
				context.getReference(data.sceneUpdatePass, SceneData::Slot::Camera);
				context.getReference(data.skyviewLUTPass, 0);

				context.getReference(data.hdrOutputPass, 0);
				context.getReference(data.hdrOutputPass, 1);

				return {
					.inputs = {
                        {
							context.getReference(data.sceneUpdatePass, SceneData::Slot::Camera),
							ResourceUsage::Type::UniformBuffer
						}, {
							context.getReference(data.skyviewLUTPass, 0),
							ResourceUsage::Type::SampledRead,
						},
					},
					.outputs = {
                        {
                            context.getReference(data.hdrOutputPass, 0),
    			              ResourceUsage::Type::ColorAttachmentWrite,
                        },
                        {
                            context.getReference(data.hdrOutputPass, 1),
    			              ResourceUsage::Type::DepthStencilRead
                        },
					 }
				};
			},

			.build =
				[](Task::BuildContext& context) {
					RenderPass::Begin(context, AttachmentOp::ReadWrite, AttachmentOp::Read);
					DrawPass::Quad(context, context.getData<SkyboxData>().skyboxMaterial);

					RenderPass::End(context.commandBuffer);
				},
		},
		SkyboxData {
			.sceneUpdatePass = sceneUpdate,
			.hdrOutputPass = lightingDeferred,
			.skyviewLUTPass = proceduralSkyPasses.skyviewLUT,
			.skyboxMaterial = m_materialManager.getMaterialIndex("skybox"),
		}
	);
	passes.push_back(skyboxPass);

	struct SimplePassData {
		TaskIndex previousPass;
		MaterialIndex material;
	};
	TaskIndex compositionPass = m_graph.addTask(
		"composition",
		{ .setup = [](Task::SetupContext& context) -> Task::Dependencies {
			 auto hdr_output = context.getReference(context.getData<SimplePassData>().previousPass, 0);
			 auto resolution = context.scene.camera.getResolution();

			 rendergraph::ResourceIndex tonemapped = context.createImage(
				 "tonemapped",
				 {
					 .width = static_cast<uint32_t>(resolution.x),
					 .height = static_cast<uint32_t>(resolution.y),
					 .depth = 1,
					 .miplevels = 1,
					 .format = vk::Format::eR8G8B8A8Unorm,
					 .usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
				 }

			 );
			 return {
				.inputs = {
					 { hdr_output, ResourceUsage::Type::ShaderRead },
				},
				.outputs = {
					 { tonemapped, ResourceUsage::Type::ShaderWrite },
				},
			 };
		 },
		  .build =
			  [](Task::BuildContext& context) {
				  auto& size = context.getInput<Image&>(0).size;

				  ComputePass(
					  context, context.getData<SimplePassData>().material, glm::uvec3(size.width, size.height, 1)
				  );
			  } },
		SimplePassData {
			.previousPass = lightingDeferred,
			.material = m_materialManager.getMaterialIndex("composition"),
		}

	);

	passes.push_back(compositionPass);

	TaskIndex fxaa = m_graph.addTask(
		"fxaa",
		{
			.setup = [](Task::SetupContext& context) -> Task::Dependencies {
				auto tonemapped = context.getReference(context.getData<SimplePassData>().previousPass, 0);

				auto resolution = context.scene.camera.getResolution();
				auto final = context.createImage(
					"final",
					{
						.width = static_cast<uint32_t>(resolution.x),
						.height = static_cast<uint32_t>(resolution.y),
						.depth = 1,
						.miplevels = 1,
						.format = vk::Format::eR8G8B8A8Unorm,
						.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,

					}

				);
				return {
					.inputs = {
					 { tonemapped, ResourceUsage::Type::ShaderRead },
					},
					.outputs = {
					 { final, ResourceUsage::Type::ColorAttachmentWrite },
					},
				};
			},
			.build =
				[](Task::BuildContext& context) {
					RenderPass::Begin(context, AttachmentOp::ClearWrite, AttachmentOp::Read);
					DrawPass::Quad(context, context.getData<SimplePassData>().material);
					RenderPass::End(context.commandBuffer);
				},
		},
		SimplePassData {
			.previousPass = compositionPass,
			.material = m_materialManager.getMaterialIndex("fxaa"),
		}
	);
	passes.push_back(fxaa);

	TaskIndex ui = m_graph.addTask(
		"UI",
		{
			.setup = [](Task::SetupContext& context) -> Task::Dependencies {
				return {
					.outputs = {
						{
							context.getReference(context.getData<TaskIndex>(), 0),
							ResourceUsage::Type::ColorAttachmentWrite,
						},
					},
				};
			},
			.build =
				[](Task::BuildContext& context) {
					RenderPass::Begin(context, AttachmentOp::ReadWrite, AttachmentOp::ReadWrite);
					UIPass(context);
					RenderPass::End(context.commandBuffer);
				},
		},
		fxaa
	);
	passes.push_back(ui);
	m_graph.update(passes, scene);

	return passes;
}
