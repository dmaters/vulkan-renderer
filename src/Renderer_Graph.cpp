#include <vector>

#include "Renderer.hpp"
#include "material/MaterialDefinitions.hpp"
#include "rendergraph/ResourceUsage.hpp"
#include "rendergraph/tasks/BufferUpload.hpp"
#include "rendergraph/tasks/ComputePass.hpp"
#include "rendergraph/tasks/DrawPass.hpp"
#include "rendergraph/tasks/DrawPassIndirect.hpp"
#include "rendergraph/tasks/FrustumCulling.hpp"
#include "rendergraph/tasks/RenderPassBegin.hpp"
#include "rendergraph/tasks/RenderPassEnd.hpp"
#include "rendergraph/tasks/SceneUpdatePass.hpp"
#include "rendergraph/tasks/ShadowPass.hpp"
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

	ResourceIndex pbrMaterialData =
		m_graph.registerBuffer("pbr_data_buffer", buffers[4]);
	ResourceIndex pbrMaterialInstances =
		m_graph.registerBuffer("pbr_instances_buffer", buffers[5]);

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
	m_graph.addTask(
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

	m_graph.addTask(
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

	m_graph.addTask(
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
	m_graph.addTask(
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

	ResourceIndex skyLightingSH = m_graph.createDeviceBuffer(
		"skyLightingSH",
		{
			.size = sizeof(glm::vec4) * 9,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer |
	                 vk::BufferUsageFlagBits::eUniformBuffer,
		}
	);
	m_graph.addTask(
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

	ResourceIndex indirectShadowBuffer = m_graph.createDeviceBuffer(
		"indirect_shadow_buffer",
		{
			.size = static_cast<uint32_t>(
				m_currentScene.primitives.size() *
				sizeof(vk::DrawIndexedIndirectCommand) * 3
			),
			.usage = vk::BufferUsageFlagBits::eTransferDst |
	                 vk::BufferUsageFlagBits::eIndirectBuffer,
		}
	);
	ResourceIndex indirectShadowBufferHost = m_graph.createHostBuffer(
		"indirect_shadow_buffer_host",
		m_currentScene.primitives.size() *
			sizeof(vk::DrawIndexedIndirectCommand) * 3 * 3
	);

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
	m_graph.addTask(
		"shadowmap",
		TaskType::Graphic,
		{
			{ lightBuffer,          ResourceUsage::Type::UniformBuffer      },
			{ cameraBuffer,         ResourceUsage::Type::UniformBuffer      },
			{ pbrMaterialData,      ResourceUsage::Type::StorageBufferRead  },
			{ pbrMaterialInstances, ResourceUsage::Type::StorageBufferRead  },
			{ indirectShadowBuffer, ResourceUsage::Type::IndirectBufferRead },
    },
		{
			{ shadowAtlas, ResourceUsage::Type::DepthStencilWrite },
			{ indirectShadowBufferHost, ResourceUsage::Type::Undefined },
		},
		[](TaskContext& context) {
			ShadowPass(context, 0);
			ShadowPass(context, 1);
			ShadowPass(context, 2);
		}
	);
	m_graph.addTask(
		"copy_shadow_indirect",
		TaskType::Transfer,
		{
			{
             indirectShadowBufferHost, ResourceUsage::Type::TransferSrc,
			 },
    },
		{
			{ indirectShadowBuffer, ResourceUsage::Type::TransferDst },
		},
		BufferUpload
	);

	ResourceIndex indirectGPassBuffer = m_graph.createDeviceBuffer(
		"indirect_gpass_buffer",
		{
			.size = static_cast<uint32_t>(
				m_currentScene.primitives.size() *
				sizeof(vk::DrawIndexedIndirectCommand)
			),
			.usage = vk::BufferUsageFlagBits::eTransferDst |
	                 vk::BufferUsageFlagBits::eIndirectBuffer,
		}
	);
	ResourceIndex indirectGPassMaterialBuffer = m_graph.createDeviceBuffer(
		"indirect_gpass_material_buffer",
		{
			.size = static_cast<uint32_t>(
				m_currentScene.primitives.size() * sizeof(uint32_t)
			),
			.usage = vk::BufferUsageFlagBits::eTransferDst |
	                 vk::BufferUsageFlagBits::eStorageBuffer,
		}
	);
	ResourceIndex indirectGPassBufferHost = m_graph.createHostBuffer(
		"indirect_gpass_buffer_host",
		m_currentScene.primitives.size() *
			sizeof(vk::DrawIndexedIndirectCommand) * 3
	);

	ResourceIndex indirectGPassMaterialBufferHost = m_graph.createHostBuffer(
		"indirect_gpass_buffer_host",
		m_currentScene.primitives.size() * sizeof(uint32_t) * 3
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

	m_graph.addTask(
		"gbuffer",
		TaskType::Graphic,
		{
			{ cameraBuffer,                ResourceUsage::Type::UniformBuffer      },
			{ pbrMaterialData,             ResourceUsage::Type::StorageBufferRead  },
			{ pbrMaterialInstances,        ResourceUsage::Type::StorageBufferRead  },
			{ indirectGPassMaterialBuffer,
             ResourceUsage::Type::StorageBufferRead                                },
			{ indirectGPassBuffer,         ResourceUsage::Type::IndirectBufferRead },
    },
		{
			{ albedo, ResourceUsage::Type::ColorAttachmentWrite },
			{ normal, ResourceUsage::Type::ColorAttachmentWrite },
			{ worldPos, ResourceUsage::Type::ColorAttachmentWrite },
			{ roughnessMetallic, ResourceUsage::Type::ColorAttachmentWrite },
			{ depth, ResourceUsage::Type::DepthStencilWrite },
			{ indirectGPassMaterialBufferHost, ResourceUsage::Type::Undefined },
			{ indirectGPassBufferHost, ResourceUsage::Type::Undefined },
		},
		[material = m_materialManager.getMaterialIndex("gbuffer"),
	     alphaTested = m_materialManager.getMaterialIndex(
			 "gbuffer_alphatested"
		 )](TaskContext& context) {
			auto visiblePrimitives = FrustumCulling(
				context.scene,
				context.scene.buckets.at(material),
				context.scene.camera.getFrustumPlanes()
			);
			auto visiblePrimitivesAlphaTested = FrustumCulling(
				context.scene,
				context.scene.buckets.at(alphaTested),
				context.scene.camera.getFrustumPlanes()
			);

			UI::Data.sceneData.gbufferCount =
				visiblePrimitives.size() + visiblePrimitivesAlphaTested.size();

			RenderPassBegin(
				context, AttachmentOp::ClearWrite, AttachmentOp::ClearWrite
			);

			DrawPassIndirect(context, material, visiblePrimitives);
			DrawPass(context, alphaTested, visiblePrimitivesAlphaTested);

			RenderPassEnd(context);
		}
	);

	m_graph.addTask(
		"copy_indirect_gbuffer",
		TaskType::Transfer,
		{
			{ indirectGPassBufferHost, ResourceUsage::Type::TransferSrc },
    },
		{
			{ indirectGPassBuffer, ResourceUsage::Type::TransferDst },
		},
		BufferUpload
	);
	m_graph.addTask(
		"copy_material_gbuffer",
		TaskType::Transfer,
		{
			{
             indirectGPassMaterialBufferHost, ResourceUsage::Type::TransferSrc,
			 },
    },
		{
			{ indirectGPassMaterialBuffer, ResourceUsage::Type::TransferDst },
		},
		BufferUpload
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
	m_graph.addTask(
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
			DrawPass(context, material, context.scene.buckets.at(material));
			RenderPassEnd(context);
		}
	);

	m_graph.addTask(
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
			DrawPass(context, material, context.scene.buckets.at(material));
			RenderPassEnd(context);
		}
	);

	ResourceIndex result = m_graph.createImage(
		"result",
		{
			.width = 1280,
			.height = 720,
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eR8G8B8A8Snorm,
			.usage = vk::ImageUsageFlagBits::eStorage |
	                 vk::ImageUsageFlagBits::eTransferSrc,

		},
		1
	);
	m_graph.addTask(
		"composition",
		TaskType::Compute,
		{
			{ hdr_output, ResourceUsage::Type::ShaderRead },
    },
		{
			{ result, ResourceUsage::Type::ShaderWrite },
		},
		[composition = m_materialManager.getMaterialIndex("composition")](
			TaskContext& context
		) {
			ImageHandle input = context.images[context.inputs[0].first];
			auto dispatch = context.resourceManager.getImage(input).size;

			ComputePass(
				context,
				composition,
				glm::uvec3(dispatch.width, dispatch.height, 1)
			);
		}
	);
	m_graph.addTask(
		"UI",
		TaskType::Graphic,
		{
    },
		{
			{ result, ResourceUsage::Type::ColorAttachmentWrite },
		},
		UIPass
	);
	m_graph.setOutputImage(result);
}
