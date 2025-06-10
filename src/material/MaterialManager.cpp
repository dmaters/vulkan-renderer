#include "MaterialManager.hpp"

#include <cstddef>
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
#include "Pipeline.hpp"
#include "ShaderEngine.hpp"
#include "material/MaterialManager.hpp"
#include "material/Pipeline.hpp"
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

	MaterialDescription baseErrorMaterial = {
		.pipelineName = "fallback_error",
	};
	createMaterial<MaterialDefinitions::SimpleMaterial>(baseErrorMaterial);

	MaterialDescription basePBRMaterial = {
		.pipelineName = "pbr",
	};
	createMaterial<MaterialDefinitions::PBRMaterial>(basePBRMaterial);
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

	Buffer& dynamicDataBuffer =
		m_resourceManager.getBuffer(m_globalBuffers["view_projection"].second);
	Buffer& lightBuffer =
		m_resourceManager.getBuffer(m_globalBuffers["light_buffer"].second);

	vk::DescriptorBufferInfo dynamicDescriptorInfo {
		.buffer = dynamicDataBuffer.buffer,
		.offset = dynamicDataBuffer.bufferAccess[0].offset,
		.range = sizeof(GlobalResources::Camera)
	};

	vk::DescriptorBufferInfo lightDescriptorInfo {
		.buffer = lightBuffer.buffer,
		.offset = lightBuffer.bufferAccess[0].offset,
		.range = sizeof(MaterialDefinitions::Lights)
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
	for (PipelineIndex index : m_shaderEngine->getUpdatedPipelines()) {
		std::optional<Pipeline> pipeline = m_shaderEngine->getPipeline(index);
		MaterialIndex materialIndex = m_pipelines[index];
		if (pipeline.has_value()) {
			if (m_brokenMaterials.contains(materialIndex))
				m_brokenMaterials.erase(materialIndex);
			m_materials[materialIndex].pipeline = pipeline.value();
		} else {
			m_brokenMaterials.insert(materialIndex);
		}
	}

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

template <typename T>
void MaterialManager::createMaterial(MaterialDescription& description) {
	MaterialIndex index = m_materialCount;
	m_materialCount++;

	PipelineIndex pipelineIndex =
		m_shaderEngine->getIndex(description.pipelineName);
	m_pipelines[pipelineIndex] = index;

	std::optional<Pipeline> pipeline = m_shaderEngine->getPipeline(index);

	if (!pipeline.has_value()) {
		m_brokenMaterials.insert(index);
		return;
	}

	Material material {
		.pipeline = pipeline.value(),
		.globalSet = m_globalSet,
	};

	const PipelineMetadata& metadata =
		m_shaderEngine->getPipelineMetadata(pipelineIndex);
	if (metadata.materialResources.size() > 0) {
		vk::DescriptorSetAllocateInfo descriptorInfo {
			.descriptorPool = m_pool,
			.descriptorSetCount = 1,
			.pSetLayouts = &metadata.layouts.materialSetLayout
		};

		vk::DescriptorSet materialSet =
			m_device.allocateDescriptorSets(descriptorInfo)[0];

		writeToDescriptorSet(
			m_device,
			m_resourceManager,
			materialSet,
			metadata.materialResources,
			description.buffers,
			description.textures,
			m_linearSampler
		);

		material.materialSet = materialSet;

	} else
		material.materialSet = m_emptySet;

	m_materials[index] = material;
}

MaterialInstance MaterialManager::instantiateMaterial(
	MaterialDescription& description
) {
	PipelineIndex pipelineIndex =
		m_shaderEngine->getIndex(description.pipelineName);

	const PipelineMetadata& metadata =
		m_shaderEngine->getPipelineMetadata(pipelineIndex);
	MaterialIndex materialIndex = m_pipelines[pipelineIndex];
	Material& material = m_materials[materialIndex];

	if (metadata.instanceResources.size() > 0) {
		vk::DescriptorSetAllocateInfo descriptorInfo {
			.descriptorPool = m_pool,
			.descriptorSetCount = 1,
			.pSetLayouts = &metadata.layouts.instanceSetLayout
		};

		vk::DescriptorSet instanceSet =
			m_device.allocateDescriptorSets(descriptorInfo)[0];

		writeToDescriptorSet(
			m_device,
			m_resourceManager,
			instanceSet,
			metadata.instanceResources,
			description.buffers,
			description.textures,
			m_linearSampler
		);

		material.instanceSets.push_back(instanceSet);
	}

	return {
		.index = materialIndex,
		.instance = (uint32_t)material.instanceSets.size() - 1,
	};
}

Material& MaterialManager::getMaterial(MaterialIndex index) {
	if (m_brokenMaterials.contains(index))
		return m_materials[0];
	else
		return m_materials[index];
}

void MaterialManager::syncData(std::vector<MirroredBuffer> buffers) {
	std::vector<ResourceManager::BufferCopy> info;

	for (auto buffer : buffers) {
		uint32_t size = m_resourceManager.getBuffer(buffer.first).size;

		info.push_back({
			.origin = buffer.first,
			.destination = buffer.second,
			.copy = {
					 .srcOffset = 0,
					 .dstOffset = 0,
					 .size = size,
					 }
        });
	}

	m_resourceManager.copyBuffers(info);
}