
#include "Renderer.hpp"

#include <SDL3/SDL_vulkan.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <glm/ext/matrix_clip_space.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Instance.hpp"
#include "Swapchain.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/trigonometric.hpp"
#include "material/MaterialDefinitions.hpp"
#include "material/MaterialManager.hpp"
#include "rendergraph/RenderGraph.hpp"
#include "rendergraph/RenderGraphBuilder.hpp"
#include "rendergraph/ResourceUsage.hpp"
#include "rendergraph/tasks/ComputePass.hpp"
#include "rendergraph/tasks/RenderPass.hpp"
#include "rendergraph/tasks/ShadowPass.hpp"
#include "rendergraph/tasks/Task.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Light.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneLoader.hpp"

Renderer::Renderer(SDL_Window* window) :
	m_instance(Instance::Create(window)),
	m_resourceManager(),
	m_materialManager(m_resourceManager),
	m_swapchain(),
	m_renderGraph(m_swapchain, m_resourceManager, m_materialManager) {
	if (window == nullptr) return;

	m_graphicsQueue = m_instance.device.getQueue(
		m_instance.queueFamiliesIndices.graphicsIndex, 0
	);

	m_presentQueue = m_instance.device.getQueue(
		m_instance.queueFamiliesIndices.presentIndex, 0
	);
}

void Renderer::createRenderGraph() {
	RenderGraphBuilder builder;

	ResourceIndex transmittanceLUT = builder.createImage(
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

	builder.addTask(
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

	ResourceIndex multiscatteringLUT = builder.createImage(
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

	builder.addTask(
		"multiscatteringLUT",
		TaskType::Compute,
		{
			{ transmittanceLUT, ResourceUsage::Type::SampledRead }
    },
		{
			{ multiscatteringLUT, ResourceUsage::Type::ShaderWrite },
		},
		[material = m_materialManager.getMaterialIndex("multiscatteringLUT")](
			TaskContext& context
		) { ComputePass(context, material, { 64, 64, 1 }); }
	);

	ResourceIndex skyviewLUT = builder.createImage(
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
	builder.addTask(
		"skyviewLUT",
		TaskType::Compute,
		{
			{ transmittanceLUT,   ResourceUsage::Type::SampledRead },
			{ multiscatteringLUT, ResourceUsage::Type::SampledRead },
    },
		{
			{ skyviewLUT, ResourceUsage::Type::ShaderWrite },
		},
		[material = m_materialManager.getMaterialIndex("skyviewLUT")](
			TaskContext& context
		) { ComputePass(context, material, { 200, 100, 1 }); }
	);

	ResourceIndex skyLightingSH = builder.createBuffer(
		"skyLightingSH",
		{
			.size = sizeof(glm::vec4) * 9,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer |
	                 vk::BufferUsageFlagBits::eUniformBuffer,
		}
	);
	builder.addTask(
		"skyLighting",
		TaskType::Compute,
		{
			{ skyviewLUT, ResourceUsage::Type::SampledRead },
    },
		{
			{ skyLightingSH, ResourceUsage::Type::StorageBufferWrite },
		},
		[material = m_materialManager.getMaterialIndex("skyLighting")](
			TaskContext& context
		) { ComputePass(context, material, { 1, 1, 1 }); }
	);

	ResourceIndex shadowAtlas = builder.createImage(
		"shadow_atlas",
		{
			.width = 3072,
			.height = 1024,
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eD16Unorm,
			.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment |
	                 vk::ImageUsageFlagBits::eSampled,

		}
	);
	builder.addTask(
		"shadowmap_near",
		TaskType::Graphic,
		{
    },
		{
			{ shadowAtlas, ResourceUsage::Type::DepthStencilWrite },
		},
		[&primitives = m_currentScene.primitives](TaskContext& context) {
			ShadowPass(context, 0, primitives);
			ShadowPass(context, 1, primitives);
			ShadowPass(context, 2, primitives);
		}
	);

	ResourceIndex albedo = builder.createImage(
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
	ResourceIndex normal = builder.createImage(
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
	ResourceIndex worldPos = builder.createImage(
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
	ResourceIndex roughnessMetallic = builder.createImage(
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
	ResourceIndex depth = builder.createImage(
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
	builder.addTask(
		"gbuffer",
		TaskType::Graphic,
		{
    },
		{
			{ albedo, ResourceUsage::Type::ColorAttachmentWrite },
			{ normal, ResourceUsage::Type::ColorAttachmentWrite },
			{ worldPos, ResourceUsage::Type::ColorAttachmentWrite },
			{ roughnessMetallic, ResourceUsage::Type::ColorAttachmentWrite },
			{ depth, ResourceUsage::Type::DepthStencilWrite },
		},
		[material = m_materialManager.getMaterialIndex("pbr_deferred"),
	     &primitives = m_currentScene.primitives](TaskContext& context) {
			RenderPass(context, material, primitives);
		}
	);

	ResourceIndex hdr_output = builder.createImage(
		"hdr_output",
		{
			.width = 1280,
			.height = 720,
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eR16G16B16A16Sfloat,
			.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                 vk::ImageUsageFlagBits::eSampled,

		},
		1
	);
	builder.addTask(
		"pbr_lighting",
		TaskType::Graphic,
		{
			{ albedo,            ResourceUsage::Type::SampledRead   },
			{ normal,            ResourceUsage::Type::SampledRead   },
			{ worldPos,          ResourceUsage::Type::SampledRead   },
			{ roughnessMetallic, ResourceUsage::Type::SampledRead   },
			{ shadowAtlas,       ResourceUsage::Type::SampledRead   },
			{ skyLightingSH,     ResourceUsage::Type::UniformBuffer }
    },
		{
			{ hdr_output, ResourceUsage::Type::ColorAttachmentWrite },
			{ depth, ResourceUsage::Type::DepthStencilRead },
		},
		[material = m_materialManager.getMaterialIndex("lighting_deferred"),
	     &primitives = m_currentScene.primitives](TaskContext& context) {
			RenderPass(context, material, primitives);
		}
	);
	builder.addTask(
		"skybox",
		TaskType::Graphic,
		{
			{ skyviewLUT, ResourceUsage::Type::SampledRead }
    },
		{
			{ hdr_output, ResourceUsage::Type::ColorAttachmentWrite },
			{ depth, ResourceUsage::Type::DepthStencilRead },
		},
		[material = m_materialManager.getMaterialIndex("skybox"),
	     &primitives = m_currentScene.primitives](TaskContext& context) {
			RenderPass(context, material, primitives);
		}
	);

	ResourceIndex result = builder.createImage(
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
	builder.addTask(
		"composition",
		TaskType::Compute,
		{
			{ hdr_output, ResourceUsage::Type::ShaderRead },
    },
		{
			{ result, ResourceUsage::Type::ShaderWrite },
		},
		[material = m_materialManager.getMaterialIndex("composition"
	     )](TaskContext& context) {
			ImageHandle input = context.images[context.inputs[0]];
			auto dispatch = context.resourceManager.getImage(input).size;

			ComputePass(
				context,
				material,
				glm::uvec3(dispatch.width, dispatch.height, 1)
			);
		}
	);
	builder.setOutputImage(result);
	GraphData data = builder.build();
	m_renderGraph.build(data);
}

void Renderer::render() {
	m_materialManager.update(m_currentFrame);
	vk::Extent2D resolution = m_swapchain.getResolution();

	if (resolution == vk::Extent2D(0)) {
		m_swapchain.rebuild();
		return;
	}
	m_currentScene.camera.setResolution({
		resolution.width,
		resolution.height,
	});

	m_renderGraph.submit(m_currentScene);

	m_currentFrame = (m_currentFrame + 1) % 3;
};

void Renderer::load(const std::filesystem::path& path) {
	SceneLoader loader(m_resourceManager, m_materialManager);
	m_currentScene = loader.load(path);

	glm::mat3 orientation = glm::mat3(1);
	orientation[0] = glm::vec3(1, 0, 0);
	orientation[1] = glm::vec3(0, 0, 1);
	orientation[2] = glm::vec3(0, 1, 0);
	orientation = glm::rotate_slow(
		glm::mat4(orientation), (float)glm::radians(-80.0), glm::vec3(1, 0, 0)
	);
	m_currentScene.lights.push_back({
		.position = glm::vec3(0, 0, 600),
		.orientation = orientation,
		.intensity = 25.0f,
	});

	const auto& lights = m_currentScene.lights;

	m_resourceManager.queueBufferUpdate<MaterialDefinitions::Lights>(
		m_resourceManager.getNamedBufferIndex("light_buffer"),
		[lights](MaterialDefinitions::Lights& lightUBO) {
			lightUBO.light = lights[0].getShaderObject();
		}
	);

	m_resourceManager.queueBufferUpdate<MaterialDefinitions::EnvironmentData>(
		m_resourceManager.getNamedBufferIndex("environment_data"),
		[size =
	         m_currentScene.size](MaterialDefinitions::EnvironmentData& envData
	    ) {
			envData.sceneSize = size;
			envData.environmentColor = glm::vec3(0.04, 0.02, 0.1);
			// envData.environmentColor = glm::vec3(1);
		}
	);

	MaterialIndex lighting =
		m_materialManager.getMaterialIndex("lighting_deferred");
	m_currentScene.primitives.push_back({
		.baseVertex = 0,
		.baseIndex = 0,
		.indexCount = 3,
		.instanceCount = 1,
		.materials = { {
			lighting,
		} },
	});

	MaterialIndex skybox = m_materialManager.getMaterialIndex("skybox");
	m_currentScene.primitives.push_back({
		.baseVertex = 0,
		.baseIndex = 0,
		.indexCount = 3,
		.instanceCount = 1,
		.materials = { {
			skybox,
		} },
	});
	createRenderGraph();
}
