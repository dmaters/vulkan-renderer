#pragma once

#include <cassert>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "MaterialDefinitions.hpp"
#include "MaterialManager.hpp"
#include "Pipeline.hpp"
#include "material/MaterialDefinitions.hpp"
#include "memory/Allocation.hpp"
#include "memory/MemoryAllocator.hpp"
#include "resources/ResourceManager.hpp"

#pragma region ForwardPBR

template <>
MaterialIndex
MaterialManager::registerMaterial<MaterialDefinitions::PBRMaterial>() {
	MaterialIndex index = m_materialCount;
	m_materialCount++;

	std::vector<vk::DescriptorSetLayoutBinding> materialBindings {
		{
         .binding = 0,
         .descriptorType = vk::DescriptorType::eUniformBuffer,
         .descriptorCount = 1,
		 },
	};
	vk::DescriptorSetLayout materialLayout =
		Instance::Get().device.createDescriptorSetLayout(
			vk::DescriptorSetLayoutCreateInfo {
				.bindingCount = (uint32_t)materialBindings.size(),
				.pBindings = materialBindings.data(),
			}
		);

	PipelineIndex pipeline = m_shaderEngine->registerPipeline({
		.modules = {
					.vertex = "../resources/shaders/standard_forward_vert.slang",
					.fragment = "../resources/shaders/standard_forward_frag.slang",
					},
		.layouts = {
			m_globalSetLayout,
			materialLayout,
		},
    });

	MaterialMetadata
		metadata = { .pipeline = pipeline,
		             .materialBindings = materialBindings,
		             .materialLayout = materialLayout,
					 .materialBuffers = {
						(uint32_t)sizeof(MaterialDefinitions::PBRUniforms),
					 },
		             .namedResourceDependencies = 
	{
		{
			.name = "main_color",
			.kind = ResourceDependency::Kind::RenderTarget,
			.usage = {
				.type = ResourceUsage::Type::WRITE,
				.access =
					vk::AccessFlagBits2::eColorAttachmentWrite,
				.stage = vk::PipelineStageFlagBits2::
					eColorAttachmentOutput,
			},
			.requiredLayout = vk::ImageLayout::eColorAttachmentOptimal,
		},  
		{
			.name = "depth",
			.kind = ResourceDependency::Kind::RenderTarget,
			.usage = {
				.type = ResourceUsage::Type::WRITE,
				.access =
					vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
				.stage = vk::PipelineStageFlagBits2::
					eEarlyFragmentTests,
				},
				.requiredLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
		 },
	},
		
	};

	m_materialMetadata[index] = metadata;

	return index;
}

#pragma region GBUffer

template <>
MaterialIndex
MaterialManager::registerMaterial<MaterialDefinitions::GBufferBase>() {
	MaterialIndex index = m_materialCount;
	m_materialCount++;

	std::vector<vk::DescriptorSetLayoutBinding> materialBindings {
		{
         .binding = 0,
         .descriptorType = vk::DescriptorType::eUniformBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eFragment,
		 },
	};
	vk::DescriptorSetLayout materialLayout =
		Instance::Get().device.createDescriptorSetLayout(
			vk::DescriptorSetLayoutCreateInfo {
				.bindingCount = (uint32_t)materialBindings.size(),
				.pBindings = materialBindings.data(),
			}
		);

	PipelineIndex pipeline = m_shaderEngine->registerPipeline({
		.modules = {
					.vertex = "../resources/shaders/base_transform_vert.slang",
					.fragment = "../resources/shaders/gbuffer_frag.slang",
					},
		.layouts = {
			m_globalSetLayout,
			materialLayout,
		},
		.configuration = {
			.attachmentFormats = {
				vk::Format::eR16G16B16A16Sfloat,
				vk::Format::eR16G16B16A16Sfloat,
				vk::Format::eR16G16B16A16Sfloat,
				vk::Format::eR16G16B16A16Sfloat,

			},
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
		}
		
    });

	MaterialMetadata
		metadata = { .pipeline = pipeline,
		             .materialBindings = materialBindings,
		             .materialLayout = materialLayout,
					 .materialBuffers ={
						(uint32_t)sizeof(MaterialDefinitions::PBRUniforms),
					 },
		             .namedResourceDependencies = 
	{
		{
			.name = "gbuffer_albedo",
			.kind = ResourceDependency::Kind::RenderTarget,
			.usage = {
				.type = ResourceUsage::Type::WRITE,
				.access =
					vk::AccessFlagBits2::eColorAttachmentWrite,
				.stage = vk::PipelineStageFlagBits2::
					eColorAttachmentOutput,
			},
			.requiredLayout = vk::ImageLayout::eColorAttachmentOptimal,
		},  {
			.name = "gbuffer_worldpos",
			.kind = ResourceDependency::Kind::RenderTarget,
			.usage = {
				.type = ResourceUsage::Type::WRITE,
				.access =
					vk::AccessFlagBits2::eColorAttachmentWrite,
				.stage = vk::PipelineStageFlagBits2::
					eColorAttachmentOutput,
			},
			.requiredLayout = vk::ImageLayout::eColorAttachmentOptimal,
		},  {
			.name = "gbuffer_normal",
			.kind = ResourceDependency::Kind::RenderTarget,
			.usage = {
				.type = ResourceUsage::Type::WRITE,
				.access =
					vk::AccessFlagBits2::eColorAttachmentWrite,
				.stage = vk::PipelineStageFlagBits2::
					eColorAttachmentOutput,
			},
			.requiredLayout = vk::ImageLayout::eColorAttachmentOptimal,
		},  {
			.name = "gbuffer_roughnessMetallic",
			.kind = ResourceDependency::Kind::RenderTarget,
			.usage = {
				.type = ResourceUsage::Type::WRITE,
				.access =
					vk::AccessFlagBits2::eColorAttachmentWrite,
				.stage = vk::PipelineStageFlagBits2::
					eColorAttachmentOutput,
			},
			.requiredLayout = vk::ImageLayout::eColorAttachmentOptimal,
		},  
		{
			.name = "depth",
			.kind = ResourceDependency::Kind::RenderTarget,
			.usage = {
				.type = ResourceUsage::Type::WRITE,
				.access =
					vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
				.stage = vk::PipelineStageFlagBits2::
					eEarlyFragmentTests,
				},
				.requiredLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
		 },
	},
		
	};

	m_materialMetadata[index] = metadata;

	return index;
}

#pragma region DeferredLighting

template <>
MaterialIndex
MaterialManager::registerMaterial<MaterialDefinitions::DeferredLighting>() {
	MaterialIndex index = m_materialCount;
	m_materialCount++;

	std::vector<vk::DescriptorSetLayoutBinding> materialBindings {};
	vk::DescriptorSetLayout materialLayout =
		Instance::Get().device.createDescriptorSetLayout(
			vk::DescriptorSetLayoutCreateInfo {
				.bindingCount = (uint32_t)materialBindings.size(),
				.pBindings = materialBindings.data(),
			}
		);

	std::vector<vk::DescriptorSetLayoutBinding> transientBindings {
		{
         .binding = 0,
         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eFragment,
		 },
		{
         .binding = 1,
         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eFragment,
		 },
		{
         .binding = 2,
         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eFragment,
		 },
		{
         .binding = 3,
         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eFragment,
		 },
	};
	vk::DescriptorSetLayout transientSetLayout =
		Instance::Get().device.createDescriptorSetLayout(
			vk::DescriptorSetLayoutCreateInfo {
				.flags =
					vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
				.bindingCount = (uint32_t)transientBindings.size(),
				.pBindings = transientBindings.data(),

			}
		);

	PipelineIndex pipeline = m_shaderEngine->registerPipeline({
		.modules = {
					.vertex = "../resources/shaders/quad_vert.slang",
					.fragment = "../resources/shaders/lighting_deferred.slang",
					},
		.layouts = {
			m_globalSetLayout,
			materialLayout,
			transientSetLayout,
		},
		.configuration = {
			.stencilEnabled = true,
			.stencilOp = {
				.failOp = vk::StencilOp::eKeep,
				.passOp = vk::StencilOp::eKeep,
				.compareOp = vk::CompareOp::eEqual,
				.compareMask = 0xFF,
				.writeMask = 0,
				.reference = 1
			}
		}
    });

	MaterialMetadata
		metadata = { .pipeline = pipeline,
		             .materialBindings = materialBindings,
		             .materialLayout = materialLayout,
					 .materialBuffers ={
					 },
		             .namedResourceDependencies = 
	{{
			.name = "main_color",
			.kind = ResourceDependency::Kind::RenderTarget,
			.usage = {
				.type = ResourceUsage::Type::WRITE,
				.access =
					vk::AccessFlagBits2::eColorAttachmentWrite,
				.stage = vk::PipelineStageFlagBits2::
					eColorAttachmentOutput,
			},
			.requiredLayout = vk::ImageLayout::eColorAttachmentOptimal,
		},
		{
			.name = "depth",
			.kind = ResourceDependency::Kind::RenderTarget,
			.usage = {
				.type = ResourceUsage::Type::READ,
				.access =
					vk::AccessFlagBits2::eDepthStencilAttachmentRead,
				.stage = vk::PipelineStageFlagBits2::
					eEarlyFragmentTests,
			},
			.requiredLayout = vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal,
		},
		{
			.name = "gbuffer_albedo",
			.kind = ResourceDependency::Kind::Sampler,
			.usage = {
				.type = ResourceUsage::Type::READ,
				.access =
					vk::AccessFlagBits2::eShaderSampledRead,
				.stage = vk::PipelineStageFlagBits2::
					eFragmentShader,
			},
			.requiredLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		},  {
			.name = "gbuffer_worldpos",
			.kind = ResourceDependency::Kind::Sampler,
			.usage = {
				.type = ResourceUsage::Type::READ,
				.access =
					vk::AccessFlagBits2::eShaderSampledRead,
				.stage = vk::PipelineStageFlagBits2::
					eFragmentShader,
			},
			.requiredLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		},  {
			.name = "gbuffer_normal",
			.kind = ResourceDependency::Kind::Sampler,
			.usage = {
				.type = ResourceUsage::Type::READ,
				.access =
					vk::AccessFlagBits2::eShaderSampledRead,
				.stage = vk::PipelineStageFlagBits2::
					eFragmentShader,
			},
			.requiredLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		},  {
			.name = "gbuffer_roughnessMetallic",
			.kind = ResourceDependency::Kind::Sampler,
			.usage = {
				.type = ResourceUsage::Type::READ,
				.access =
					vk::AccessFlagBits2::eShaderSampledRead,
				.stage = vk::PipelineStageFlagBits2::
					eFragmentShader,
			},
			.requiredLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		},  
	},
	};
	m_materialMetadata[index] = metadata;

	return index;
}

#pragma region Shadow Map

template <>
MaterialIndex MaterialManager::registerMaterial<MaterialDefinitions::ShadowMap>(
) {
	MaterialIndex index = m_materialCount;
	m_materialCount++;
	/*
	    std::vector<vk::DescriptorSetLayoutBinding> materialBindings {
	        // {
	        //  .binding = 0,
	        //  .descriptorType = vk::DescriptorType::eUniformBuffer,
	        //  .descriptorCount = 1,
	        //  },
	    };
	    vk::DescriptorSetLayout materialLayout =
	        Instance::Get().device.createDescriptorSetLayout(
	            vk::DescriptorSetLayoutCreateInfo {
	                .bindingCount = (uint32_t)materialBindings.size(),
	                .pBindings = materialBindings.data(),
	            }
	        );
	*/
	PipelineIndex pipeline = m_shaderEngine->registerPipeline({
		.modules = {
					.vertex = "../resources/shaders/base_transform_vert.slang",
					.fragment = "../resources/shaders/dummy_frag.slang",
					},
		.layouts = {
			m_globalSetLayout,
			m_emptySetLayout,
		},
    });

	MaterialMetadata
		metadata = { .pipeline = pipeline,
		             .materialBindings = {},
		             .materialLayout = m_emptySetLayout,
					 .materialBuffers = {
						
						//(uint32_t)sizeof(MaterialDefinitions::PBRUniforms),
					 },
		             .namedResourceDependencies = 
	{
		{
			.name = "shadow_atlas",
			.kind = ResourceDependency::Kind::RenderTarget,
			.usage = {
				.type = ResourceUsage::Type::WRITE,
				.access =
					vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
				.stage = vk::PipelineStageFlagBits2::
					eEarlyFragmentTests,
				},
				.requiredLayout = vk::ImageLayout::eDepthAttachmentOptimal,
		 },
	},
		
	};

	m_materialMetadata[index] = metadata;

	return index;
}
