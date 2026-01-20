#include <vector>

#include "Renderer.hpp"
#include "material/MaterialDefinitions.hpp"
#include "rendergraph/ResourceUsage.hpp"
#include "rendergraph/tasks/ComputePass.hpp"
#include "rendergraph/tasks/RenderPass.hpp"
#include "rendergraph/tasks/SceneUpdatePass.hpp"
#include "rendergraph/tasks/ShadowPass.hpp"
#include "rendergraph/tasks/Task.hpp"
#include "rendergraph/tasks/TaskContext.hpp"
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

	ResourceIndex cameraBuffer = m_graph.createBuffer(
		"camera_buffer",
		ResourceManager::BufferDescription {
			.size = sizeof(MaterialDefinitions::Camera),
			.usage = vk::BufferUsageFlagBits::eTransferDst |
	                 vk::BufferUsageFlagBits::eUniformBuffer,
		}
	);
	ResourceIndex lightBuffer = m_graph.createBuffer(
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

	ResourceIndex computeScratchBuffer = m_graph.createBuffer(
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

	m_graph.addComputePass(
		"transmittanceLUT",
		{
    },
		{
			{ transmittanceLUT, ResourceUsage::Type::ShaderWrite },
		},
		"resources/shaders/transmittanceLUT.slang",
		[](TaskContext& context) {
			ComputePass(
				context,
				context.materialManager.getMaterialIndex("transmittanceLUT"),
				{ 256, 64, 1 }
			);
		}
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

	m_graph.addComputePass(
		"multiscatteringLUT",
		{
			{ transmittanceLUT, ResourceUsage::Type::SampledRead },
    },
		{
			{ multiscatteringLUT, ResourceUsage::Type::ShaderWrite },
			{ computeScratchBuffer, ResourceUsage::Type::StorageBufferWrite },
		},
		"resources/shaders/multiscatteringLUT.slang",
		[](TaskContext& context) {
			ComputePass(
				context,
				context.materialManager.getMaterialIndex("multiscatteringLUT"),
				{ 64, 64, 1 }
			);
		}
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
	m_graph.addComputePass(
		"skyviewLUT",
		{
			{ lightBuffer,        ResourceUsage::Type::UniformBuffer },
			{ transmittanceLUT,   ResourceUsage::Type::SampledRead   },
			{ multiscatteringLUT, ResourceUsage::Type::SampledRead   },
    },
		{
			{ skyviewLUT, ResourceUsage::Type::ShaderWrite },
		},
		"resources/shaders/skyviewLUT.slang",
		[](TaskContext& context) {
			ComputePass(
				context,
				context.materialManager.getMaterialIndex("skyviewLUT"),
				{ 200, 100, 1 }
			);
		}
	);

	ResourceIndex skyLightingSH = m_graph.createBuffer(
		"skyLightingSH",
		{
			.size = sizeof(glm::vec4) * 9,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer |
	                 vk::BufferUsageFlagBits::eUniformBuffer,
		}
	);
	m_graph.addComputePass(
		"skyLighting",
		{
			{ skyviewLUT, ResourceUsage::Type::SampledRead },
    },
		{
			{ skyLightingSH, ResourceUsage::Type::StorageBufferWrite },
		},
		"resources/shaders/sky_lighting.slang",
		[](TaskContext& context) {
			ComputePass(
				context,
				context.materialManager.getMaterialIndex("skyLighting"),
				{ 1, 1, 1 }
			);
		}
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
	m_graph.addGraphicPass(
		"shadowmap",
		{
			{ lightBuffer,  ResourceUsage::Type::UniformBuffer },
			{ cameraBuffer, ResourceUsage::Type::UniformBuffer },
    },
		{
			{ shadowAtlas, ResourceUsage::Type::DepthStencilWrite },
		},
		{
			.vertex = "resources/shaders/shadow_vert.slang",
			.fragment = "resources/shaders/dummy_frag.slang",
		},
		{
			.depthFormat = vk::Format::eD16Unorm,
			.depthWrite = true,
			.depthOp = vk::CompareOp::eLessOrEqual,
		},
		[](TaskContext& context) {
			ShadowPass(context, 0);
			ShadowPass(context, 1);
			ShadowPass(context, 2);
		}
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

	m_graph.addGraphicPass(
		"gbuffer",
		{
		    { cameraBuffer,ResourceUsage::Type::UniformBuffer },
			{ pbrMaterialData , ResourceUsage::Type::StorageBufferRead	},
			{ pbrMaterialInstances , ResourceUsage::Type::StorageBufferRead	},

		},
		{
			{ albedo, ResourceUsage::Type::ColorAttachmentWrite },
			{ normal, ResourceUsage::Type::ColorAttachmentWrite },
			{ worldPos, ResourceUsage::Type::ColorAttachmentWrite },
			{ roughnessMetallic, ResourceUsage::Type::ColorAttachmentWrite },
			{ depth, ResourceUsage::Type::DepthStencilWrite },
		},
		{
			.vertex = "resources/shaders/base_transform_vert.slang",
			.fragment = "resources/shaders/gbuffer_frag.slang",
		},
		{
			.colorAttachmentFormats = {
				vk::Format::eR16G16B16A16Sfloat,
				vk::Format::eR16G16B16A16Sfloat,
				vk::Format::eR16G16B16A16Sfloat,
				vk::Format::eR16G16B16A16Sfloat,

			},
			.depthFormat = vk::Format::eD24UnormS8Uint,
			.depthWrite = true,
			.depthOp = vk::CompareOp::eLessOrEqual,
			.stencilEnabled = true,
			.stencilOp = {
				.failOp = vk::StencilOp::eKeep,
				.passOp = vk::StencilOp::eReplace,
				.compareOp = vk::CompareOp::eAlways,
				.compareMask = 0xFF,
				.writeMask = 0xFF,
				.reference = 1,
			}
		},
		[](TaskContext& context) {
		MaterialIndex materialIndex = context.materialManager.getMaterialIndex("gbuffer");
            auto visiblePrimitives = FrustumCulling(
                context.scene,
                context.scene.buckets.at(materialIndex),
                context.scene.camera.getFrustumPlanes()
            );
            RenderPass(
                context,
                visiblePrimitives,
                materialIndex,
                AttachmentOp::ClearWrite,
				AttachmentOp::ClearWrite
            );
		}
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
	m_graph.addGraphicPass(
		"lighting_deferred",
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
		{
			.vertex = "resources/shaders/quad_vert.slang",
			.fragment = "resources/shaders/lighting_deferred.slang",
		},
		{
			.colorAttachmentFormats = { vk::Format::eR16G16B16A16Sfloat },
			.depthFormat = vk::Format::eD24UnormS8Uint,
			.cullMode = vk::CullModeFlagBits::eNone,
			.stencilEnabled = true,
			.stencilOp = { .failOp = vk::StencilOp::eKeep,
	                       .passOp = vk::StencilOp::eKeep,
	                       .compareOp = vk::CompareOp::eEqual,
	                       .compareMask = 0xFF,
	                       .writeMask = 0,
	                       .reference = 1 },
		},
		[](TaskContext& context) {
			MaterialIndex materialIndex =
				context.materialManager.getMaterialIndex("lighting_deferred");

			RenderPass(
				context,
				context.scene.buckets.at(materialIndex),
				materialIndex,
				AttachmentOp::ClearWrite,
				AttachmentOp::Read
			);
		}
	);

	m_graph.addGraphicPass(
		"skybox",
		{
		    { cameraBuffer, ResourceUsage::Type::UniformBuffer },
			{ skyviewLUT, ResourceUsage::Type::SampledRead },
        },
		{
			{ hdr_output, ResourceUsage::Type::ColorAttachmentWrite },
			{ depth, ResourceUsage::Type::DepthStencilRead },
		},
		{
			.vertex = "resources/shaders/quad_vert.slang",
			.fragment = "resources/shaders/skybox.slang",
		},
		{
			.colorAttachmentFormats = {
			    vk::Format::eR16G16B16A16Sfloat
			},
			.depthFormat = vk::Format::eD24UnormS8Uint,
			.depthWrite = false,
			.stencilEnabled = true,
			.stencilOp =
				{
				.failOp = vk::StencilOp::eKeep,
				.passOp = vk::StencilOp::eKeep,
				.compareOp = vk::CompareOp::eEqual,
				.compareMask = 0xFF,
				.reference = 0,
				}
		},
		[](
			TaskContext& context
		) {

		    MaterialIndex materialIndex = context.materialManager.getMaterialIndex("skybox");
		    RenderPass(
		        context,
	            context.scene.buckets.at(materialIndex),
				materialIndex,
				AttachmentOp::ReadWrite,
				AttachmentOp::Read
		    );
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
	m_graph.addComputePass(
		"composition",
		{
			{ hdr_output, ResourceUsage::Type::ShaderRead },
    },
		{
			{ result, ResourceUsage::Type::ShaderWrite },
		},
		"resources/shaders/composition.slang",
		[](TaskContext& context) {
			ImageHandle input = context.images[context.inputs[0].first];
			auto dispatch = context.resourceManager.getImage(input).size;

			ComputePass(
				context,
				context.materialManager.getMaterialIndex("composition"),
				glm::uvec3(dispatch.width, dispatch.height, 1)
			);
		}
	);

	m_graph.setOutputImage(result);
}
