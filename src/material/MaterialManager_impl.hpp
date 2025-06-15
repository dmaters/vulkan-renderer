

#include <cassert>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

#include "MaterialDefinitions.hpp"
#include "MaterialManager.hpp"
#include "memory/Allocation.hpp"
#include "memory/MemoryAllocator.hpp"
#include "resources/ResourceManager.hpp"

template <>
void MaterialManager::createMaterialData<MaterialDefinitions::PBRMaterial>(
	MaterialIndex index
) {
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

	m_materialData[index][0] = MirroredBuffer(mirror, buffer);
	m_materialData[index][1] = m_globalBuffers["light_buffer"];
};

template <>
MaterialDefinitions::PBRMaterial MaterialManager::getMaterialData(
	MaterialIndex index
) {
	assert(m_materialData.contains(index));
	std::vector<MirroredBuffer>& buffers = m_materialData[index];

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
