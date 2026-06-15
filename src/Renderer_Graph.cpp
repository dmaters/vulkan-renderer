#include <vector>

#include "Renderer.hpp"
#include "material/MaterialDefinitions.hpp"
#include "rendergraph/ResourceUsage.hpp"
#include "rendergraph/tasks/ComputePass.hpp"
#include "rendergraph/tasks/DrawPassIndirect.hpp"
#include "rendergraph/tasks/DrawPassQuad.hpp"
#include "rendergraph/tasks/FrustumCulling.hpp"
#include "rendergraph/tasks/RenderPass.hpp"
#include "rendergraph/tasks/SceneUpdatePass.hpp"
#include "rendergraph/tasks/Task.hpp"
#include "rendergraph/tasks/TaskContext.hpp"
#include "rendergraph/tasks/UIPass.hpp"
#include "resources/ResourceManager.hpp"

void Renderer::createRenderGraph(Scene& scene) {
	const std::vector<BufferHandle>& buffers =
		m_resourceManager.getBuffers(scene.allocation);

	m_graph.registerBuffer("vertex_positions_buffer", buffers[0]);
	m_graph.registerBuffer("vertex_attributes_buffer", buffers[1]);
	m_graph.registerBuffer("index_buffer", buffers[2]);
	m_graph.registerBuffer("instance_buffer", buffers[3]);

	std::vector<TaskIndex> passes;

	ResourceIndex pbrMaterialData =
		m_graph.registerBuffer("pbr_data_buffer", buffers[5]);
	ResourceIndex pbrMaterialInstances =
		m_graph.registerBuffer("pbr_instances_buffer", buffers[4]);

	ResourceIndex cameraBuffer = m_graph.createDeviceBuffer(
		"camera_buffer",
		ResourceManager::BufferDescription {
			.size = sizeof(MaterialDefinitions::Camera),
			.usage = vk::BufferUsageFlagBits::eTransferDst |
	                 vk::BufferUsageFlagBits::eUniformBuffer,
		}
	);
	ResourceIndex lightBuffer = m_graph.createDeviceBuffer(
		"light_buffer",
		ResourceManager::BufferDescription {
			.size = sizeof(MaterialDefinitions::Light),
			.usage = vk::BufferUsageFlagBits::eTransferDst |
	                 vk::BufferUsageFlagBits::eUniformBuffer,
		}
	);
	TaskIndex sceneUpdate = m_graph.addTask(
		"scene_update",
		TaskType::Transfer,
		{
    },
		{
			{ cameraBuffer, ResourceUsage::Type::TransferDst },
			{ lightBuffer, ResourceUsage::Type::TransferDst },
		},
		SceneUpdatePass
	);
	passes.push_back(sceneUpdate);

	ResourceIndex computeScratchBuffer = m_graph.createDeviceBuffer(
		"compute_scratch_buffer",
		ResourceManager::BufferDescription {
			.size = 1 << 24,  // 16MB
			.usage = vk::BufferUsageFlagBits::eStorageBuffer,
		}
	);

	ResourceIndex transmittanceLUT = m_graph.createImage(
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

	TaskIndex transmittanceLUTPass = m_graph.addTask(
		"transmittanceLUT",
		TaskType::Compute,
		{
    },
		{
			{ transmittanceLUT, ResourceUsage::Type::ShaderWrite },
		},
		[material = m_materialManager.getMaterialIndex("transmittanceLUT")](
			TaskContext& context
		) { ComputePass(context, material, { 256, 64, 1 }); }
	);
	passes.push_back(transmittanceLUTPass);

	ResourceIndex multiscatteringLUT = m_graph.createImage(
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

	TaskIndex multiscatteringLUTPass = m_graph.addTask(
		"multiscatteringLUT",
		TaskType::Compute,
		{
			{ transmittanceLUT, ResourceUsage::Type::SampledRead },
    },
		{
			{ multiscatteringLUT, ResourceUsage::Type::ShaderWrite },
			{ computeScratchBuffer, ResourceUsage::Type::StorageBufferWrite },
		},
		[material = m_materialManager.getMaterialIndex("multiscatteringLUT")](
			TaskContext& context
		) { ComputePass(context, material, { 64, 64, 1 }); }
	);

	passes.push_back(multiscatteringLUTPass);

	ResourceIndex skyviewLUT = m_graph.createImage(
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
	TaskIndex skyviewLUTPass = m_graph.addTask(
		"skyviewLUT",
		TaskType::Compute,
		{
			{ lightBuffer,        ResourceUsage::Type::UniformBuffer },
			{ transmittanceLUT,   ResourceUsage::Type::SampledRead   },
			{ multiscatteringLUT, ResourceUsage::Type::SampledRead   },
    },
		{
			{ skyviewLUT, ResourceUsage::Type::ShaderWrite },
		},
		[material = m_materialManager.getMaterialIndex("skyviewLUT")](
			TaskContext& context
		) { ComputePass(context, material, { 200, 100, 1 }); }
	);
	passes.push_back(skyviewLUTPass);

	ResourceIndex skyLightingSH = m_graph.createDeviceBuffer(
		"skyLightingSH",
		{
			.size = sizeof(glm::vec4) * 9,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer |
	                 vk::BufferUsageFlagBits::eUniformBuffer,
		}
	);
	TaskIndex skyLighting = m_graph.addTask(
		"skyLighting",
		TaskType::Compute,
		{
			{ skyviewLUT, ResourceUsage::Type::SampledRead },
    },
		{
			{ skyLightingSH, ResourceUsage::Type::StorageBufferWrite },
		},
		[material = m_materialManager.getMaterialIndex("sky_lighting")](
			TaskContext& context
		) { ComputePass(context, material, { 1, 1, 1 }); }
	);
	passes.push_back(skyLighting);

	ResourceIndex shadowAtlas = m_graph.createImage(
		"shadow_atlas",
		{
			.width = 6144,
			.height = 2048,
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eD16Unorm,
			.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment |
	                 vk::ImageUsageFlagBits::eSampled,

		}
	);

	for (int i = 0; i < 3; i++) {
		ResourceIndex indirectShadowBuffer = m_graph.createDeviceBuffer(
			"indirect_shadow_buffer_cascade_" + std::to_string(i),
			{
				.size = static_cast<uint32_t>(
					m_currentScene
						.buckets[m_materialManager
		                             .getMaterialIndex("shadowmap")]
						.size() *
					sizeof(vk::DrawIndexedIndirectCommand) * 3
				),
				.usage = vk::BufferUsageFlagBits::eIndirectBuffer,
			},
			true
		);
		ResourceIndex primitiveShadowBuffer = m_graph.createDeviceBuffer(
			"primitive_shadow_buffer_cascade_" + std::to_string(i),
			{
				.size = static_cast<uint32_t>(
					m_currentScene
						.buckets[m_materialManager
		                             .getMaterialIndex("shadowmap")]
						.size() *
					sizeof(uint32_t) * 3
				),
				.usage = vk::BufferUsageFlagBits::eStorageBuffer,
			},
			true
		);

		TaskIndex shadowmapPass = m_graph.addTask(
			"shadowmap_cascade_" + std::to_string(i),
			TaskType::Graphic,
			{
				{ lightBuffer,           ResourceUsage::Type::UniformBuffer },
				{ cameraBuffer,          ResourceUsage::Type::UniformBuffer },
				{ indirectShadowBuffer,
                 ResourceUsage::Type::IndirectBufferRead                    },
				{ primitiveShadowBuffer,
                 ResourceUsage::Type::StorageBufferRead                     },
        },
			{
				{ shadowAtlas, ResourceUsage::Type::DepthStencilWrite },
			},
			[material = m_materialManager.getMaterialIndex("shadowmap"),
		     i](TaskContext& context) {
				static const uint16_t CASCADE_SIZE = 2048;
				RenderPassBegin(
					context,
					AttachmentOp::Read,
					AttachmentOp::ClearWrite,
					vk::Rect2D {
						{ CASCADE_SIZE * i, 0            },
						{ CASCADE_SIZE,     CASCADE_SIZE },
                }
				);

				DrawPassIndirect(
					context,
					material,
					context.scene.buckets.at(material),
					2,
					3,
					i
				);
				RenderPassEnd(context);
			}
		);
		passes.push_back(shadowmapPass);

		uint32_t atShadowedPrimitiveCount =
			m_currentScene
				.buckets[m_materialManager
		                     .getMaterialIndex("shadowmap_alphatested")]
				.size();

		if (atShadowedPrimitiveCount == 0) continue;

		ResourceIndex indirectShadowBufferAlpha = m_graph.createDeviceBuffer(
			"indirect_shadow_buffer_alphatested_cascade_" + std::to_string(i),
			{
				.size = static_cast<uint32_t>(
					atShadowedPrimitiveCount *
					sizeof(vk::DrawIndexedIndirectCommand) * 3
				),
				.usage = vk::BufferUsageFlagBits::eIndirectBuffer,
			},
			true
		);
		ResourceIndex primitiveShadowBufferAlpha = m_graph.createDeviceBuffer(
			"primitive_shadow_buffer_alphatested_cascade_" + std::to_string(i),
			{
				.size = static_cast<uint32_t>(
					atShadowedPrimitiveCount * sizeof(uint32_t) * 3
				),
				.usage = vk::BufferUsageFlagBits::eStorageBuffer,
			},
			true
		);

		TaskIndex shadowmapATpass = m_graph.addTask(
			"shadowmap_cascade_" + std::to_string(i) + "_alphatested",
			TaskType::Graphic,
			{
				{ lightBuffer,                ResourceUsage::Type::UniformBuffer     },
				{ cameraBuffer,               ResourceUsage::Type::UniformBuffer     },
				{ pbrMaterialData,            ResourceUsage::Type::StorageBufferRead },
				{ pbrMaterialInstances,
                 ResourceUsage::Type::StorageBufferRead                              },
				{ indirectShadowBufferAlpha,
                 ResourceUsage::Type::IndirectBufferRead                             },
				{ primitiveShadowBufferAlpha,
                 ResourceUsage::Type::StorageBufferRead                              },
        },
			{
				{ shadowAtlas, ResourceUsage::Type::DepthStencilWrite },
			},
			[material =
		         m_materialManager.getMaterialIndex("shadowmap_alphatested"),
		     i](TaskContext& context) {
				static const uint16_t CASCADE_SIZE = 2048;
				RenderPassBegin(
					context,
					AttachmentOp::Read,
					AttachmentOp::ReadWrite,
					vk::Rect2D {
						{ CASCADE_SIZE * i, 0            },
						{ CASCADE_SIZE,     CASCADE_SIZE },
                }
				);

				DrawPassIndirect(
					context,
					material,
					context.scene.buckets.at(material),
					4,
					5,
					i
				);
				RenderPassEnd(context);
			}
		);
		passes.push_back(shadowmapATpass);
	}

	ResourceIndex indirectGPassBuffer = m_graph.createDeviceBuffer(
		"indirect_gpass_buffer",
		{
			.size = static_cast<uint32_t>(
				m_currentScene.primitives.size() *
				sizeof(vk::DrawIndexedIndirectCommand) * 3
			),
			.usage = vk::BufferUsageFlagBits::eIndirectBuffer,
		},
		true
	);
	ResourceIndex primitivesGPassBuffer = m_graph.createDeviceBuffer(
		"primitive_gpass_buffer",
		{
			.size = static_cast<uint32_t>(
				m_currentScene.primitives.size() * sizeof(uint32_t) * 3
			),
			.usage = vk::BufferUsageFlagBits::eStorageBuffer,
		},
		true
	);

	ResourceIndex albedo = m_graph.createImage(
		"gbuffer_albedo",
		{
			.width = 1280,
			.height = 720,
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eR16G16B16A16Sfloat,
			.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                 vk::ImageUsageFlagBits::eInputAttachment |
	                 vk::ImageUsageFlagBits::eSampled,

		},
		1
	);
	ResourceIndex normal = m_graph.createImage(
		"gbuffer_normal",
		{
			.width = 1280,
			.height = 720,
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eR16G16B16A16Sfloat,
			.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                 vk::ImageUsageFlagBits::eInputAttachment |
	                 vk::ImageUsageFlagBits::eSampled,

		},
		1
	);
	ResourceIndex worldPos = m_graph.createImage(
		"gbuffer_worldpos",
		{
			.width = 1280,
			.height = 720,
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eR16G16B16A16Sfloat,
			.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                 vk::ImageUsageFlagBits::eInputAttachment |
	                 vk::ImageUsageFlagBits::eSampled,

		},
		1
	);
	ResourceIndex roughnessMetallic = m_graph.createImage(
		"gbuffer_roughnessMetallic",
		{
			.width = 1280,
			.height = 720,
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eR16G16B16A16Sfloat,
			.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                 vk::ImageUsageFlagBits::eInputAttachment |
	                 vk::ImageUsageFlagBits::eSampled,

		},
		1
	);
	ResourceIndex depth = m_graph.createImage(
		"depth",
		{
			.width = 1280,
			.height = 720,
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eD24UnormS8Uint,
			.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,

		},
		1
	);

	TaskIndex gbuffer = m_graph.addTask(
		"gbuffer",
		TaskType::Graphic,
		{
			{ cameraBuffer,          ResourceUsage::Type::UniformBuffer      },
			{ pbrMaterialData,       ResourceUsage::Type::StorageBufferRead  },
			{ pbrMaterialInstances,  ResourceUsage::Type::StorageBufferRead  },
			{ indirectGPassBuffer,   ResourceUsage::Type::IndirectBufferRead },
			{ primitivesGPassBuffer, ResourceUsage::Type::StorageBufferRead  },
    },
		{
			{ albedo, ResourceUsage::Type::ColorAttachmentWrite },
			{ normal, ResourceUsage::Type::ColorAttachmentWrite },
			{ worldPos, ResourceUsage::Type::ColorAttachmentWrite },
			{ roughnessMetallic, ResourceUsage::Type::ColorAttachmentWrite },
			{ depth, ResourceUsage::Type::DepthStencilWrite },
		},
		[material = m_materialManager.getMaterialIndex("gbuffer")](
			TaskContext& context
		) {
			auto visiblePrimitives = FrustumCulling(
				context.scene,
				context.scene.buckets.at(material),
				context.scene.camera.getFrustumPlanes()
			);

			UI::Data.sceneData.gbufferCount = visiblePrimitives.size();

			RenderPassBegin(
				context, AttachmentOp::ClearWrite, AttachmentOp::ClearWrite
			);

			DrawPassIndirect(context, material, visiblePrimitives, 3, 4);

			RenderPassEnd(context);
		}
	);
	passes.push_back(gbuffer);

	uint32_t atPrimitiveCount =
		m_currentScene.buckets
			.at(m_materialManager.getMaterialIndex("gbuffer_alphatested"))
			.size();
	if (atPrimitiveCount > 0) {
		ResourceIndex indirectGPassAlphatestedBuffer =
			m_graph.createDeviceBuffer(
				"indirect_gpass_alphatested_buffer",
				{
					.size = static_cast<uint32_t>(
						m_currentScene.primitives.size() *
						sizeof(vk::DrawIndexedIndirectCommand) * 3
					),
					.usage = vk::BufferUsageFlagBits::eIndirectBuffer,
				},
				true
			);
		ResourceIndex primitivesGPassAlphatestedBuffer =
			m_graph.createDeviceBuffer(
				"primitives_gpass_alphatested_buffer",
				{
					.size = static_cast<uint32_t>(
						m_currentScene.primitives.size() * sizeof(uint32_t) * 3
					),
					.usage = vk::BufferUsageFlagBits::eStorageBuffer,
				},
				true
			);
		TaskIndex gbufferAT = m_graph.addTask(
			"gbuffer_alphatested",
			TaskType::Graphic,
			{
				{ cameraBuffer,                     ResourceUsage::Type::UniformBuffer     },
				{ pbrMaterialData,                  ResourceUsage::Type::StorageBufferRead },
				{ pbrMaterialInstances,
                 ResourceUsage::Type::StorageBufferRead                                    },
				{ indirectGPassAlphatestedBuffer,
                 ResourceUsage::Type::IndirectBufferRead                                   },
				{ primitivesGPassAlphatestedBuffer,
                 ResourceUsage::Type::StorageBufferRead                                    },
        },
			{
				{ albedo, ResourceUsage::Type::ColorAttachmentWrite },
				{ normal, ResourceUsage::Type::ColorAttachmentWrite },
				{ worldPos, ResourceUsage::Type::ColorAttachmentWrite },
				{ roughnessMetallic,
		          ResourceUsage::Type::ColorAttachmentWrite },
				{ depth, ResourceUsage::Type::DepthStencilWrite },
			},
			[material = m_materialManager.getMaterialIndex(
				 "gbuffer_alphatested"
			 )](TaskContext& context) {
				auto visiblePrimitives = FrustumCulling(
					context.scene,
					context.scene.buckets.at(material),
					context.scene.camera.getFrustumPlanes()
				);

				UI::Data.sceneData.gbufferCount += visiblePrimitives.size();

				RenderPassBegin(
					context, AttachmentOp::ReadWrite, AttachmentOp::ReadWrite
				);

				DrawPassIndirect(context, material, visiblePrimitives, 3, 4);

				RenderPassEnd(context);
			}
		);

		passes.push_back(gbufferAT);
	}

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
			TaskContext& context
		) {
			RenderPassBegin(
				context, AttachmentOp::ClearWrite, AttachmentOp::Read
			);
			DrawPassQuad(context, material);
			RenderPassEnd(context);
		}
	);
	passes.push_back(lightingDeferred);

	TaskIndex skyboxPass = m_graph.addTask(
		"skybox",
		TaskType::Graphic,
		{
			{ cameraBuffer, ResourceUsage::Type::UniformBuffer },
			{ skyviewLUT,   ResourceUsage::Type::SampledRead   },
    },
		{
			{ hdr_output, ResourceUsage::Type::ColorAttachmentWrite },
			{ depth, ResourceUsage::Type::DepthStencilRead },
		},

		[material = m_materialManager.getMaterialIndex("skybox")](
			TaskContext& context
		) {
			RenderPassBegin(
				context, AttachmentOp::ReadWrite, AttachmentOp::Read
			);
			DrawPassQuad(context, material);

			RenderPassEnd(context);
		}
	);
	passes.push_back(skyboxPass);

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
			TaskContext& context
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
		[material =
	         m_materialManager.getMaterialIndex("fxaa")](TaskContext& context) {
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
		[](TaskContext& context) {
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
