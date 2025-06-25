#include "MaterialManager.hpp"

#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Instance.hpp"
#include "Material.hpp"
#include "MaterialDefinitions.hpp"
#include "ShaderEngine.hpp"
#include "material/MaterialManager.hpp"
#include "resources/Buffer.hpp"
#include "resources/ResourceManager.hpp"

MaterialManager::MaterialManager(
	Instance& instance, ResourceManager& resourceManager
) :
	m_device(instance.device),

	m_resourceManager(resourceManager) {
	std::array<vk::DescriptorPoolSize, 2> sizes = {
		vk::DescriptorPoolSize {
								.type = vk::DescriptorType::eUniformBuffer,
								.descriptorCount = 100,
								},
		vk::DescriptorPoolSize {
								.type = vk::DescriptorType::eCombinedImageSampler,
								.descriptorCount = 20,
								}
	};

	vk::DescriptorPoolCreateInfo info {
		.flags = {},
		// TODO : keep count of sets for materials
		.maxSets = 512,
		.poolSizeCount = 1,
		.pPoolSizes = sizes.data()
	};

	m_pool = m_device.createDescriptorPool(info);

	vk::SamplerCreateInfo samplerInfo {
		.magFilter = vk::Filter::eLinear,
		.minFilter = vk::Filter::eLinear,
		.mipmapMode = vk::SamplerMipmapMode::eLinear,
		.addressModeU = vk::SamplerAddressMode::eRepeat,
		.addressModeV = vk::SamplerAddressMode::eRepeat,
		.addressModeW = vk::SamplerAddressMode::eRepeat,
	};

	m_linearSampler = m_device.createSampler(samplerInfo);

	vk::DescriptorSetLayout emptyLayout =
		m_device.createDescriptorSetLayout({});
	m_emptySet = m_device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
		.descriptorPool = m_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &emptyLayout,
	})[0];

	createGlobalBuffers();
	createGlobalDescriptorSet();

	m_shaderEngine =
		std::make_unique<ShaderEngine>(m_device, m_globalSetLayout);

	registerMaterial<MaterialDefinitions::SimpleMaterial>();

	MaterialDescription basePBRMaterial = {
		.pipelineName = "pbr",
	};
	registerMaterial<MaterialDefinitions::PBRMaterial>();
}
void MaterialManager::createGlobalBuffers() {
	BufferHandle mLightBuffer = m_resourceManager.createBuffer({
		.size = sizeof(MaterialDefinitions::Lights),
		.usage = vk::BufferUsageFlagBits::eTransferSrc,
		.location = AllocationLocation::Host,
	});
	BufferHandle lightBuffer = m_resourceManager.createBuffer({
		.size = sizeof(MaterialDefinitions::Lights),
		.usage = vk::BufferUsageFlagBits::eTransferDst |
	             vk::BufferUsageFlagBits::eUniformBuffer,
		.location = AllocationLocation::Device,
	});
	m_globalBuffers["light_buffer"] = { mLightBuffer, lightBuffer };

	BufferHandle mViewProj = m_resourceManager.createBuffer({
		.size = sizeof(MaterialDefinitions::ViewProjection),
		.usage = vk::BufferUsageFlagBits::eTransferSrc,
		.location = AllocationLocation::Host,
	});
	BufferHandle viewProj = m_resourceManager.createBuffer({
		.size = sizeof(MaterialDefinitions::ViewProjection),
		.usage = vk::BufferUsageFlagBits::eTransferDst |
	             vk::BufferUsageFlagBits::eUniformBuffer,
		.location = AllocationLocation::Device,
	});

	m_globalBuffers["view_projection"] = { mViewProj, viewProj };
}

void MaterialManager::createGlobalDescriptorSet() {
	std::array<vk::DescriptorSetLayoutBinding, 2> bindings {
		vk::DescriptorSetLayoutBinding {
										.binding = 0,
										.descriptorType = vk::DescriptorType::eUniformBuffer,
										.descriptorCount = 1,
										.stageFlags = vk::ShaderStageFlagBits::eAllGraphics,
										.pImmutableSamplers = {},
										},
		vk::DescriptorSetLayoutBinding {
										.binding = 1,
										.descriptorType = vk::DescriptorType::eUniformBuffer,
										.descriptorCount = 1,
										.stageFlags = vk::ShaderStageFlagBits::eAllGraphics,
										.pImmutableSamplers = {},
										}
	};

	vk::DescriptorSetLayoutCreateInfo layoutInfo {
		.flags = {},
		.bindingCount = bindings.size(),
		.pBindings = bindings.data(),
	};

	vk::DescriptorSetLayout layout =
		m_device.createDescriptorSetLayout(layoutInfo);

	vk::DescriptorSetAllocateInfo allocateInfo {
		.descriptorPool = m_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &layout,
	};

	auto set = m_device.allocateDescriptorSets(allocateInfo)[0];

	Buffer& cameraBuffer = m_resourceManager.getBuffer(
		m_globalBuffers["view_projection"].deviceBuffer
	);
	Buffer& lightBuffer =
		m_resourceManager.getBuffer(m_globalBuffers["light_buffer"].deviceBuffer
	    );

	vk::DescriptorBufferInfo dynamicDescriptorInfo {
		.buffer = cameraBuffer.buffer,
		.offset = cameraBuffer.bufferAccess[0].offset,
		.range = cameraBuffer.size,
	};

	vk::DescriptorBufferInfo lightDescriptorInfo {
		.buffer = lightBuffer.buffer,
		.offset = lightBuffer.bufferAccess[0].offset,
		.range = lightBuffer.size
	};

	vk::WriteDescriptorSet gBufferInfo {
		.dstSet = set,
		.dstBinding = 0,
		.descriptorCount = 1,
		.descriptorType = vk::DescriptorType::eUniformBuffer,
		.pImageInfo = {},
		.pBufferInfo = &dynamicDescriptorInfo,
		.pTexelBufferView = {},

	};

	vk::WriteDescriptorSet lightBufferInfo {
		.dstSet = set,
		.dstBinding = 1,
		.descriptorCount = 1,
		.descriptorType = vk::DescriptorType::eUniformBuffer,
		.pImageInfo = {},
		.pBufferInfo = &lightDescriptorInfo,
		.pTexelBufferView = {},

	};
	m_device.updateDescriptorSets({ gBufferInfo, lightBufferInfo }, {});
	m_globalSet = set;
	m_globalSetLayout = layout;
}
void MaterialManager::update(uint8_t currentFrame) {
	m_shaderEngine->flushRetiredPipelines();
}

void writeToDescriptorSet(
	vk::Device& device,
	ResourceManager& resourceManager,
	vk::DescriptorSet set,
	std::vector<vk::DescriptorSetLayoutBinding> layoutBindings,
	std::unordered_map<uint32_t, BufferHandle>& buffers,
	std::unordered_map<uint32_t, ImageHandle>& images,
	vk::Sampler linearSampler
) {
	std::vector<vk::WriteDescriptorSet> writeInfos;

	for (const auto& resource : layoutBindings) {
		if (buffers.contains(resource.binding)) {
			Buffer& buffer =
				resourceManager.getBuffer(buffers[resource.binding]);

			vk::DescriptorBufferInfo bufferInfo {
				.buffer = buffer.buffer,
				.offset = 0,
				.range = buffer.size,
			};

			writeInfos.push_back(vk::WriteDescriptorSet {
				.dstSet = set,
				.dstBinding = (uint32_t)(resource.binding),
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.pBufferInfo = &bufferInfo,
			});
			continue;
		}
		if (images.contains(resource.binding)) {
			Image& image = resourceManager.getImage(images[resource.binding]);
			vk::DescriptorImageInfo imageInfo {
				.sampler = linearSampler,
				.imageView = image.view,
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			};

			writeInfos.push_back({
				.dstSet = set,
				.dstBinding = (uint32_t)(resource.binding),
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.pImageInfo = &imageInfo,

			});
			continue;
		}
		assert(false);
	}

	device.updateDescriptorSets(
		writeInfos.size(), writeInfos.data(), 0, nullptr
	);
}

Material MaterialManager::getMaterial(MaterialIndex index) {
	assert(m_materialData.contains(index));
	MaterialData& data = m_materialData[index];
	Material material {
		.pipeline = m_shaderEngine->getPipeline(data.pipeline),
		.materialSet = data.materialSet,
	};

	return material;
}

void MaterialManager::syncData(std::vector<MirroredBuffer> buffers) {
	std::vector<ResourceManager::BufferCopy> info;

	for (auto buffer : buffers) {
		uint32_t size = m_resourceManager.getBuffer(buffer.localBuffer).size;

		info.push_back({
			.origin = buffer.deviceBuffer,
			.destination = buffer.localBuffer,
			.copy = {
					 .srcOffset = 0,
					 .dstOffset = 0,
					 .size = size,
					 }
        });
	}

	m_resourceManager.copyBuffers(info);
}
