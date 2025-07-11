#pragma once

#include <cassert>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "MaterialDefinitions.hpp"
#include "MaterialManager.hpp"
#include "Pipeline.hpp"
#include "memory/Allocation.hpp"
#include "memory/MemoryAllocator.hpp"
#include "resources/ResourceManager.hpp"

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
		{
         .binding = 1,
         .descriptorType = vk::DescriptorType::eUniformBuffer,
         .descriptorCount = 1,
		 }
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
		}
    });

	MaterialMetadata
		metadata = { .pipeline = pipeline,
		             .materialBindings = materialBindings,
		             .materialLayout = materialLayout,
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

template <>
MaterialManager::MaterialData MaterialManager::createMaterialData<
	MaterialDefinitions::PBRMaterial>(MaterialIndex index) {
	BufferHandle mirrorBase =
		m_resourceManager.createBuffer(ResourceManager::BufferDescription {
			.size = sizeof(MaterialDefinitions::PBRMaterial::BaseValues),
			.usage = vk::BufferUsageFlagBits::eTransferSrc,
			.location = AllocationLocation::Host,
		});

	BufferHandle bufferBase =
		m_resourceManager.createBuffer(ResourceManager::BufferDescription {
			.size = sizeof(MaterialDefinitions::PBRMaterial::BaseValues),
			.usage = vk::BufferUsageFlagBits::eUniformBuffer |
	                 vk::BufferUsageFlagBits::eTransferDst,
			.location = AllocationLocation::Device,
		});

	BufferHandle mirrorInstance =
		m_resourceManager.createBuffer(ResourceManager::BufferDescription {
			.size = sizeof(MaterialDefinitions::PBRMaterialUniforms),
			.usage = vk::BufferUsageFlagBits::eTransferSrc,
			.location = AllocationLocation::Host,
		});

	BufferHandle bufferInstance =
		m_resourceManager.createBuffer(ResourceManager::BufferDescription {
			.size = sizeof(MaterialDefinitions::PBRMaterialUniforms),
			.usage = vk::BufferUsageFlagBits::eUniformBuffer |
	                 vk::BufferUsageFlagBits::eTransferDst,
			.location = AllocationLocation::Device,
		});

	MaterialMetadata metadata = m_materialMetadata[index];

	std::vector<BufferHandle> materialBuffers = { bufferBase, bufferInstance };
	vk::DescriptorSet materialSet = createSet(
		metadata.materialBindings, metadata.materialLayout, materialBuffers
	);

	return {
		.pipeline = metadata.pipeline,
		.materialBuffers = {
			MirroredBuffer(mirrorBase, bufferBase),
			MirroredBuffer(mirrorInstance, bufferInstance),
            m_globalBuffers["light_buffer"],
		},
		.materialSet = materialSet
	};
};

template <>
MaterialDefinitions::PBRMaterial MaterialManager::getMaterialData(
	MaterialIndex index
) {
	if (!m_materialData.contains(index)) {
		m_materialData[index] =
			createMaterialData<MaterialDefinitions::PBRMaterial>(index);
	}
	std::vector<MirroredBuffer>& buffers =
		m_materialData[index].materialBuffers;

	Buffer& mirroredBufferBase =
		m_resourceManager.getBuffer(buffers[0].localBuffer);
	assert(mirroredBufferBase.size != sizeof(MaterialDefinitions::PBRMaterial));

	Buffer& mirroredBufferInstance =
		m_resourceManager.getBuffer(buffers[0].localBuffer);

	MaterialDefinitions::PBRMaterial result = {
		.base_values = (MaterialDefinitions::PBRMaterial::BaseValues*)
		                   mirroredBufferBase.allocation.address,
		.instances =
			(std::array<MaterialDefinitions::PBRMaterialUniforms, 512>*)
				mirroredBufferInstance.allocation.address,

		.light_data =
			(const MaterialDefinitions::Lights*)m_resourceManager
				.getBuffer(m_globalBuffers["light_buffer"].localBuffer)
				.allocation.address,
		.view_projection =
			(const MaterialDefinitions::ViewProjection*)m_resourceManager
				.getBuffer(m_globalBuffers["view_projection"].localBuffer)
				.allocation.address
	};

	return result;
};
