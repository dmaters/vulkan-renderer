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
		vk::DescriptorSetLayoutBinding { .binding = 0 }
	};
	vk::DescriptorSetLayout materialLayout =
		m_device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo {
			.bindingCount = (uint32_t)materialBindings.size(),
			.pBindings = materialBindings.data(),
		});

	PipelineIndex pipeline = m_shaderEngine->registerPipeline({
		.modules = {
					.vertex = "resources/shaders/standard_forward_vert.slang",
					.fragment = "resources/shaders/standard_forward_frag.slang",
					},
		.layouts = {
			m_globalSetLayout,
			materialLayout,
			m_emptySetLayout,
		}
    });

	MaterialMetadata metadata = {
		.pipeline = pipeline,
		.materialBindings = materialBindings,
		.materialLayout = materialLayout,
	};
	m_materialMetadata[index] = metadata;

	return index;
}

template <>
MaterialManager::MaterialData MaterialManager::createMaterialData<
	MaterialDefinitions::PBRMaterial>(MaterialIndex index) {
	BufferHandle mirror =
		m_resourceManager.createBuffer(ResourceManager::BufferDescription {
			.size = sizeof(MaterialDefinitions::PBRMaterial::BaseValues),
			.usage = vk::BufferUsageFlagBits::eTransferSrc,
			.location = AllocationLocation::Host,
		});

	BufferHandle buffer =
		m_resourceManager.createBuffer(ResourceManager::BufferDescription {
			.size = sizeof(MaterialDefinitions::PBRMaterial::BaseValues),
			.usage = vk::BufferUsageFlagBits::eUniformBuffer |
	                 vk::BufferUsageFlagBits::eTransferDst,
			.location = AllocationLocation::Device,
		});

	MaterialMetadata metadata = m_materialMetadata[index];

	return {
		.pipeline = metadata.pipeline,
		.materialBuffers = {
			MirroredBuffer(mirror, buffer),
            m_globalBuffers["light_buffer"],
		},
	};
};

template <>
MaterialDefinitions::PBRMaterial MaterialManager::getMaterialData(
	MaterialIndex index
) {
	assert(m_materialData.contains(index));
	std::vector<MirroredBuffer>& buffers =
		m_materialData[index].materialBuffers;

	Buffer& mirroredBuffer =
		m_resourceManager.getBuffer(buffers[0].localBuffer);
	assert(mirroredBuffer.size != sizeof(MaterialDefinitions::PBRMaterial));
	MaterialDefinitions::PBRMaterial result = {
		.base_values = (MaterialDefinitions::PBRMaterial::BaseValues*)
		                   mirroredBuffer.allocation.address,
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
