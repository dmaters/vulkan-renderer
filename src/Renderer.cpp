
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
#include "material/MaterialDefinitions.hpp"
#include "material/MaterialManager.hpp"
#include "rendergraph/RenderGraph.hpp"
#include "rendergraph/RenderGraphBuilder.hpp"
#include "rendergraph/tasks/RenderPass.hpp"
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

	ResourceIndex shadowAtlas = builder.createImage(
		"shadow_atlas",
		{
			.width = 1024,
			.height = 1024,
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eD16Unorm,
			.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment |
	                 vk::ImageUsageFlagBits::eSampled,

		}
	);
	builder.addTask(
		"shadowmap",
		TaskType::Graphic,
		{ },
		{ { shadowAtlas,
	        	{
					.usage = { 
						.access = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
	                    .stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests,
					},
					.requiredLayout = vk::ImageLayout::eDepthAttachmentOptimal,
				},},},
	[material = m_materialManager.getMaterialIndex("shadow_map"),  &primitives = m_currentScene.primitives]
			(TaskContext& context){RenderPass(context,material, primitives);}	
	);

	ResourceIndex albedo = builder.createImage(
		"gbuffer_albedo",
		{
			.width = 800,
			.height = 600,
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
			.width = 800,
			.height = 600,
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
			.width = 800,
			.height = 600,
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
			.width = 800,
			.height = 600,
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
			.width = 800,
			.height = 600,
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
			{
				albedo,
				{
					.usage = { 
						.access = vk::AccessFlagBits2::eColorAttachmentWrite,
	                    .stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
					},
					.requiredLayout = vk::ImageLayout::eColorAttachmentOptimal,
				},
			},
			{
				normal,
				 {
					.usage = { .access =
	                               vk::AccessFlagBits2::eColorAttachmentWrite,
	                           .stage = vk::PipelineStageFlagBits2::
	                               eColorAttachmentOutput,
							},
					.requiredLayout = vk::ImageLayout::eColorAttachmentOptimal,
				},
			},
			{
				worldPos,
				{
					.usage = { 
						.access = vk::AccessFlagBits2::eColorAttachmentWrite,
	                    .stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
					},
					.requiredLayout = vk::ImageLayout::eColorAttachmentOptimal,
				},
			},
			{
				roughnessMetallic,
				{
					.usage = { 
						.access = vk::AccessFlagBits2::eColorAttachmentWrite,
	                    .stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
					},
					.requiredLayout = vk::ImageLayout::eColorAttachmentOptimal,
				},
			},
			{
				depth,
				{
					.usage = { 
						.access = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
	                    .stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests,
					},
					.requiredLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
				},
			},
		},
		[material = m_materialManager.getMaterialIndex("pbr_deferred"), &primitives = m_currentScene.primitives](TaskContext& context) {RenderPass(context,material, primitives);}
	);

	ResourceIndex hdr_output = builder.createImage(
		"hdr_output",
		{
			.width = 800,
			.height = 600,
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eR16G16B16A16Sfloat,
			.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                 vk::ImageUsageFlagBits::eTransferSrc,

		},
		1
	);
	builder.addTask(
		"pbr_lighting",
		TaskType::Graphic,
		{{
				albedo,
				{
					.usage = { 
						.access = vk::AccessFlagBits2::eShaderSampledRead,
	                    .stage = vk::PipelineStageFlagBits2::eFragmentShader,
					},
					.requiredLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				},
			},
			{
				normal,
				 {
					.usage = { 
						.access = vk::AccessFlagBits2::eShaderSampledRead,
	                    .stage = vk::PipelineStageFlagBits2::eFragmentShader,
					},
					.requiredLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				},
			},
			{
				worldPos,
				{
					.usage = { 
						.access = vk::AccessFlagBits2::eShaderSampledRead,
	                    .stage = vk::PipelineStageFlagBits2::eFragmentShader,
					},
					.requiredLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				},
			},
			{
				roughnessMetallic,
				{
					.usage = { 
						.access = vk::AccessFlagBits2::eShaderSampledRead,
	                    .stage = vk::PipelineStageFlagBits2::eFragmentShader,
					},
					.requiredLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				},
			},
			{
				shadowAtlas,{
					.usage = { 
						.access = vk::AccessFlagBits2::eShaderSampledRead,
	                    .stage = vk::PipelineStageFlagBits2::eFragmentShader,
					},
					.requiredLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				},
			}
    	},
		
			{	
			{
				hdr_output,
				{
					.usage = { 
						.access = vk::AccessFlagBits2::eColorAttachmentWrite,
	                    .stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
					},
					.requiredLayout = vk::ImageLayout::eColorAttachmentOptimal,
				},
			},
			{
				depth,
				{
					.usage = { 
						.access = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
	                    .stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests,
					},
					.requiredLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
				},
			},
		},
		[material = m_materialManager.getMaterialIndex("lighting_deferred"), &primitives = m_currentScene.primitives](TaskContext& context) {
			RenderPass(context, material, primitives);
		}
	);
	builder.setOutputImage(hdr_output);
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
	m_currentScene.lights.push_back({
		.position = glm::vec3(0, 0, 150),
		.orientation = orientation,
		.intensity = 20,
	});

	const auto& lights = m_currentScene.lights;

	m_resourceManager.queueBufferUpdate<MaterialDefinitions::Lights>(
		m_resourceManager.getNamedBufferIndex("light_buffer"),
		[lights](MaterialDefinitions::Lights& lightUBO) {
			lightUBO.count = lights.size();
			lightUBO.directLightIndex = 0;
			for (int i = 0; i < lights.size(); i++) {
				lightUBO.lights[i] = lights[i].getShaderObject();
			}
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
	createRenderGraph();
}
