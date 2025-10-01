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
#include "resources/ResourceManager.hpp"

#pragma region ForwardPBR

template <>
MaterialIndex
MaterialManager::registerMaterial<MaterialDefinitions::PBRMaterial>() {
	MaterialIndex index = ++m_materialCount;

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
					.vertex = "resources/shaders/standard_forward_vert.slang",
					.fragment = "resources/shaders/standard_forward_frag.slang",
					},
		.layouts = {
			m_globalSetLayout,
			materialLayout,
		},
    });

	MaterialMetadata metadata = {
		.pipeline = pipeline,
		.materialBindings = materialBindings,
		.materialLayout = materialLayout,
		.materialBufferSize =
			(uint32_t)sizeof(MaterialDefinitions::PBRUniforms),
	};

	m_materialMetadata[index] = metadata;

	return index;
}

#pragma region GBUffer

template <>
MaterialIndex
MaterialManager::registerMaterial<MaterialDefinitions::GBufferBase>() {
	MaterialIndex index = ++m_materialCount;

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
					.vertex = "resources/shaders/base_transform_vert.slang",
					.fragment = "resources/shaders/gbuffer_frag.slang",
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

	MaterialMetadata metadata = {
		.pipeline = pipeline,
		.materialBindings = materialBindings,
		.materialLayout = materialLayout,
		.materialBufferSize =
			(uint32_t)sizeof(MaterialDefinitions::PBRUniforms),
	};

	m_materialMetadata[index] = metadata;

	return index;
}

#pragma region DeferredLighting

template <>
MaterialIndex
MaterialManager::registerMaterial<MaterialDefinitions::DeferredLighting>() {
	MaterialIndex index = ++m_materialCount;

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
		{
         .binding = 4,
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
					.vertex = "resources/shaders/quad_vert.slang",
					.fragment = "resources/shaders/lighting_deferred.slang",
					},
		.layouts = {
			m_globalSetLayout,
			materialLayout,
			transientSetLayout,
		},
		.configuration = {
			.attachmentFormats = {
			    vk::Format::eR16G16B16A16Sfloat
			},
		    .cullMode = vk::CullModeFlagBits::eNone,
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

	MaterialMetadata metadata = {
		.pipeline = pipeline,
		.materialBindings = materialBindings,
		.materialLayout = materialLayout,
		.depthOp = MaterialManager::MaterialMetadata::AttachmentOp::Read,
	};
	m_materialMetadata[index] = metadata;

	return index;
}

#pragma region Shadow Map

template <>
MaterialIndex MaterialManager::registerMaterial<MaterialDefinitions::ShadowMap>(
) {
	MaterialIndex index = ++m_materialCount;

	PipelineIndex pipeline = m_shaderEngine->registerPipeline({
		.modules = {
					.vertex = "resources/shaders/shadow_vert.slang",
					.fragment = "resources/shaders/dummy_frag.slang",
					},
		.layouts = {
			m_globalSetLayout,
			m_emptySetLayout,
		},

		.configuration ={
			.depthWrite = true,
			.depthOp = vk::CompareOp::eLessOrEqual,
		}
    });

	MaterialMetadata metadata = {
		.pipeline = pipeline,
		.materialBindings = {},
		.materialLayout = m_emptySetLayout,
	};

	m_materialMetadata[index] = metadata;

	return index;
}

#pragma region Composition Pass

template <>
MaterialIndex
MaterialManager::registerMaterial<MaterialDefinitions::CompositionPass>() {
	MaterialIndex index = ++m_materialCount;
	std::vector<vk::DescriptorSetLayoutBinding> transientBindings {
		{
         .binding = 0,
         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute,
		 },
		{
         .binding = 1,
         .descriptorType = vk::DescriptorType::eStorageImage,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute,
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
					.compute = "resources/shaders/composition.slang",
					},
		.layouts = {
			m_globalSetLayout,
			m_emptySetLayout,
			transientSetLayout
		},

		.configuration ={
			.depthWrite = true,
			.depthOp = vk::CompareOp::eLessOrEqual,
		}
    });

	MaterialMetadata metadata = {
		.pipeline = pipeline,
		.materialBindings = {},
		.materialLayout = m_emptySetLayout,
	};

	m_materialMetadata[index] = metadata;

	return index;
}

#pragma region Transmittance LUT Pass

template <>
MaterialIndex
MaterialManager::registerMaterial<MaterialDefinitions::TransmittanceLUT>() {
	MaterialIndex index = ++m_materialCount;
	std::vector<vk::DescriptorSetLayoutBinding> transientBindings {

		{
         .binding = 0,
         .descriptorType = vk::DescriptorType::eStorageImage,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute,
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
					.compute = "resources/shaders/transmittanceLUT.slang",
					},
		.layouts = {
			m_globalSetLayout,
			m_emptySetLayout,
			transientSetLayout
		},
    });

	MaterialMetadata metadata = {
		.pipeline = pipeline,
		.materialBindings = {},
		.materialLayout = m_emptySetLayout,
	};

	m_materialMetadata[index] = metadata;

	return index;
}

#pragma region Multiscattering LUT Pass

template <>
MaterialIndex
MaterialManager::registerMaterial<MaterialDefinitions::MultiscatteringLUT>() {
	MaterialIndex index = ++m_materialCount;
	std::vector<vk::DescriptorSetLayoutBinding> transientBindings {
		{
         .binding = 0,
         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute,
		 },
		{
         .binding = 1,
         .descriptorType = vk::DescriptorType::eStorageImage,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute,
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
					.compute = "resources/shaders/multiscatteringLUT.slang",
					},
		.layouts = {
			m_globalSetLayout,
			m_emptySetLayout,
			transientSetLayout
		},
		
    });

	MaterialMetadata metadata = {
		.pipeline = pipeline,
		.materialBindings = {},
		.materialLayout = m_emptySetLayout,
	};

	m_materialMetadata[index] = metadata;

	return index;
}

#pragma region SkyView LUT Pass

template <>
MaterialIndex
MaterialManager::registerMaterial<MaterialDefinitions::SkyViewLUT>() {
	MaterialIndex index = ++m_materialCount;
	std::vector<vk::DescriptorSetLayoutBinding> transientBindings {
		{
         .binding = 0,
         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute,
		 },
		{
         .binding = 1,
         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute,
		 },
		{
         .binding = 2,
         .descriptorType = vk::DescriptorType::eStorageImage,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute,
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
					.compute = "resources/shaders/skyviewLUT.slang",
					},
		.layouts = {
			m_globalSetLayout,
			m_emptySetLayout,
			transientSetLayout
		},
		
    });

	MaterialMetadata metadata = {
		.pipeline = pipeline,
		.materialBindings = {},
		.materialLayout = m_emptySetLayout,
	};

	m_materialMetadata[index] = metadata;

	return index;
}
#pragma region Skybox

template <>
MaterialIndex MaterialManager::registerMaterial<MaterialDefinitions::Skybox>() {
	MaterialIndex index = ++m_materialCount;
	std::vector<vk::DescriptorSetLayoutBinding> transientBindings {
		{
         .binding = 0,
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
					.vertex = "resources/shaders/quad_vert.slang",
					.fragment = "resources/shaders/skybox.slang",
					},
		.layouts = {
			m_globalSetLayout,
			m_emptySetLayout,
			transientSetLayout
		},

		.configuration = {
			.attachmentFormats = {
			    vk::Format::eR16G16B16A16Sfloat
			},
			.stencilEnabled = true,
			.stencilOp = 
				{
				.failOp = vk::StencilOp::eKeep,
				.passOp = vk::StencilOp::eKeep,
				.compareOp = vk::CompareOp::eEqual,
				.compareMask = 0xFF,
				.reference = 0,
				}
		}
	});

	MaterialMetadata metadata = {
		.pipeline = pipeline,
		.materialBindings = {},
		.materialLayout = m_emptySetLayout,
		.colorOp = MaterialManager::MaterialMetadata::AttachmentOp::ReadWrite,
		.depthOp = MaterialManager::MaterialMetadata::AttachmentOp::Read,
	};

	m_materialMetadata[index] = metadata;

	return index;
}
