#include "MaterialManager.hpp"

#include <cassert>
#include <cstdint>
#include <memory>
#include <ranges>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Instance.hpp"
#include "Material.hpp"
#include "MaterialDefinitions.hpp"
#include "MaterialManager_impl.hpp"
#include "ShaderEngine.hpp"
#include "material/MaterialManager.hpp"
#include "rendergraph/RenderGraphBuilder.hpp"
#include "resources/Buffer.hpp"
#include "resources/ResourceManager.hpp"

MaterialManager::MaterialManager(ResourceManager& resourceManager) :
	m_resourceManager(resourceManager) {
	vk::Device& device = Instance::Get().device;

	std::array<vk::DescriptorPoolSize, 2> sizes = {
		vk::DescriptorPoolSize {
								.type = vk::DescriptorType::eUniformBuffer,
								.descriptorCount = 200,
								},
		vk::DescriptorPoolSize {
								.type = vk::DescriptorType::eCombinedImageSampler,
								.descriptorCount = 600,
								}
	};

	vk::DescriptorPoolCreateInfo info {
		.flags = { vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind },
		.maxSets = 128,
		.poolSizeCount = sizes.size(),
		.pPoolSizes = sizes.data()
	};

	m_pool = device.createDescriptorPool(info);

	vk::SamplerCreateInfo samplerInfo {
		.magFilter = vk::Filter::eLinear,
		.minFilter = vk::Filter::eLinear,
		.mipmapMode = vk::SamplerMipmapMode::eLinear,
		.addressModeU = vk::SamplerAddressMode::eRepeat,
		.addressModeV = vk::SamplerAddressMode::eRepeat,
		.addressModeW = vk::SamplerAddressMode::eRepeat,
		.maxLod = 100,
	};

	m_linearSampler = device.createSampler(samplerInfo);

	m_emptySetLayout = device.createDescriptorSetLayout({});
	m_emptySet = device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
		.descriptorPool = m_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &m_emptySetLayout,
	})[0];

	createGlobalBuffers();
	createGlobalDescriptorSet();

	m_shaderEngine = std::make_unique<ShaderEngine>();

	// m_names["pbr_forward"]
	// = registerMaterial<MaterialDefinitions::PBRMaterial>();
	m_names["pbr_deferred"] =
		registerMaterial<MaterialDefinitions::GBufferBase>();
	m_names["lighting_deferred"] =
		registerMaterial<MaterialDefinitions::DeferredLighting>();
	m_names["shadow_map"] = registerMaterial<MaterialDefinitions::ShadowMap>();

	createMaterialData();
}

void MaterialManager::createGlobalBuffers() {
	ResourceManager::AllocationIndex index = m_resourceManager.createResources(
		{
    },
		{
			{
				.size = sizeof(MaterialDefinitions::Camera),
				.usage = vk::BufferUsageFlagBits::eTransferDst |
	                     vk::BufferUsageFlagBits::eUniformBuffer,
			},
			{
				.size = sizeof(MaterialDefinitions::Lights),
				.usage = vk::BufferUsageFlagBits::eTransferDst |
	                     vk::BufferUsageFlagBits::eUniformBuffer,

			},
			{
				.size = sizeof(MaterialDefinitions::EnvironmentData),
				.usage = vk::BufferUsageFlagBits::eTransferDst |
	                     vk::BufferUsageFlagBits::eUniformBuffer,
			},
		}
	);

	auto& buffers = m_resourceManager.getBuffers(index);
	m_resourceManager.setName("camera_data", buffers.at(0));
	m_resourceManager.setName("light_buffer", buffers.at(1));
	m_resourceManager.setName("environment_data", buffers.at(2));
}

void MaterialManager::createGlobalDescriptorSet() {
	vk::Device& device = Instance::Get().device;

	std::array<vk::DescriptorSetLayoutBinding, 4> bindings {
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
										},
		vk::DescriptorSetLayoutBinding {
										.binding = 2,
										.descriptorType = vk::DescriptorType::eUniformBuffer,
										.descriptorCount = 1,
										.stageFlags = vk::ShaderStageFlagBits::eAllGraphics,
										.pImmutableSamplers = {},
										},
		vk::DescriptorSetLayoutBinding {
										.binding = 3,
										.descriptorType = vk::DescriptorType::eCombinedImageSampler,
										.descriptorCount = 512,
										.stageFlags = vk::ShaderStageFlagBits::eAllGraphics,
										.pImmutableSamplers = {},
										}
	};

	std::array<vk::DescriptorBindingFlags, 4> bindingFlags = {
		vk::DescriptorBindingFlags {},
		vk::DescriptorBindingFlags {},
		vk::DescriptorBindingFlags {},
		vk::DescriptorBindingFlagBits::ePartiallyBound |
			vk::DescriptorBindingFlagBits::eUpdateAfterBind
	};

	vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsInfo {
		.bindingCount = bindingFlags.size(),
		.pBindingFlags = bindingFlags.data()
	};

	vk::DescriptorSetLayoutCreateInfo layoutInfo {
		.pNext = &flagsInfo,
		.flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
		.bindingCount = bindings.size(),
		.pBindings = bindings.data(),
	};

	vk::DescriptorSetLayout layout =
		device.createDescriptorSetLayout(layoutInfo);

	vk::DescriptorSetAllocateInfo allocateInfo {
		.descriptorPool = m_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &layout,
	};

	auto set = device.allocateDescriptorSets(allocateInfo)[0];

	Buffer& cameraBuffer = m_resourceManager.getNamedBuffer("camera_data");

	vk::DescriptorBufferInfo cameraDescriptorInfo {
		.buffer = cameraBuffer.buffer,
		.range = cameraBuffer.size,
	};

	vk::WriteDescriptorSet cameraWriteDescriptor {
		.dstSet = set,
		.dstBinding = 0,
		.descriptorCount = 1,
		.descriptorType = vk::DescriptorType::eUniformBuffer,
		.pImageInfo = {},
		.pBufferInfo = &cameraDescriptorInfo,
		.pTexelBufferView = {},
	};

	Buffer& environmentdataBuffer =
		m_resourceManager.getNamedBuffer("environment_data");

	vk::DescriptorBufferInfo environmentDataInfo {
		.buffer = environmentdataBuffer.buffer,
		.range = environmentdataBuffer.size,
	};

	vk::WriteDescriptorSet environemntWriteDescriptor {
		.dstSet = set,
		.dstBinding = 1,
		.descriptorCount = 1,
		.descriptorType = vk::DescriptorType::eUniformBuffer,
		.pImageInfo = {},
		.pBufferInfo = &environmentDataInfo,
		.pTexelBufferView = {},
	};

	Buffer& lightBuffer = m_resourceManager.getNamedBuffer("light_buffer");

	vk::DescriptorBufferInfo lightBufferDescriptorInfo {
		.buffer = lightBuffer.buffer,
		.range = lightBuffer.size,
	};
	vk::WriteDescriptorSet lightBufferWriteDescriptor {
		.dstSet = set,
		.dstBinding = 2,
		.descriptorCount = 1,
		.descriptorType = vk::DescriptorType::eUniformBuffer,
		.pImageInfo = {},
		.pBufferInfo = &lightBufferDescriptorInfo,
		.pTexelBufferView = {},
	};
	device.updateDescriptorSets(
		{ cameraWriteDescriptor,
	      environemntWriteDescriptor,
	      lightBufferWriteDescriptor },
		{}
	);
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

vk::DescriptorSet MaterialManager::createSet(
	const std::vector<vk::DescriptorSetLayoutBinding>& bindings,
	const vk::DescriptorSetLayout layout,
	const std::vector<BufferHandle>& materialBuffersHandles
) {
	vk::Device& device = Instance::Get().device;

	vk::DescriptorSetAllocateInfo allocateInfo {
		.descriptorPool = m_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &layout,
	};

	auto set = device.allocateDescriptorSets(allocateInfo)[0];

	std::vector<vk::DescriptorBufferInfo> buffersDescriptors;

	for (BufferHandle handle : materialBuffersHandles) {
		Buffer& buffer = m_resourceManager.getBuffer(handle);
		buffersDescriptors.push_back({
			.buffer = buffer.buffer,
			.range = buffer.size,
		});
	}

	std::vector<vk::WriteDescriptorSet> writeInfos;
	for (int i = 0; i < bindings.size(); i++) {
		vk::DescriptorSetLayoutBinding binding = bindings[i];

		writeInfos.push_back({
			.dstSet = set,
			.dstBinding = (uint32_t)i,
			.dstArrayElement = 0,
			.descriptorCount = binding.descriptorCount,
			.descriptorType = binding.descriptorType,
			.pImageInfo = nullptr,
			.pBufferInfo = &buffersDescriptors.at(i),
		});
	}

	device.updateDescriptorSets(writeInfos, {});
	return set;
}

Material MaterialManager::getMaterial(MaterialIndex index) {
	MaterialData& data = m_materialData.at(index);
	Material material {
		.pipeline = m_shaderEngine->getPipeline(data.pipeline),
		.materialSet = data.materialSet,

	};

	return material;
}

uint32_t MaterialManager::registerTextureGroup(
	ResourceManager::AllocationIndex index
) {
	auto& textures = m_resourceManager.getImages(index);

	std::vector<vk::DescriptorImageInfo> info;
	for (auto& handle : textures) {
		Image& image = m_resourceManager.getImage(handle);

		info.push_back({
			.sampler = m_linearSampler,
			.imageView = image.view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		});
	}

	vk::WriteDescriptorSet writeInfo = {
		.dstSet = m_globalSet,
		.dstBinding = 3,
		.dstArrayElement = 0,
		.descriptorCount = (uint32_t)textures.size(),
		.descriptorType = vk::DescriptorType::eCombinedImageSampler,
		.pImageInfo = info.data(),
	};

	Instance::Get().device.updateDescriptorSets({ writeInfo }, {});
	return 0;
}

void MaterialManager::createMaterialData() {
	std::vector<ResourceManager::BufferDescription> ownedBufferDescriptions;
	std::vector<ResourceManager::BufferDescription> sharedBufferDescriptions;
	std::unordered_map<std::string_view, uint32_t> sharedBuffersIndices;

	for (const auto& [index, metadata] : m_materialMetadata) {
		if (metadata.materialBufferSize > 0)
			ownedBufferDescriptions.push_back(
				ResourceManager::BufferDescription {
					.size = metadata.materialBufferSize,
					.usage = vk::BufferUsageFlagBits::eUniformBuffer |
			                 vk::BufferUsageFlagBits::eTransferDst,
				}
			);
	}
	std::vector<ResourceManager::BufferDescription> bufferDescriptions;

	if (sharedBufferDescriptions.size() > 0)
		bufferDescriptions.insert(
			bufferDescriptions.end(),
			sharedBufferDescriptions.begin(),
			sharedBufferDescriptions.end()
		);
	if (ownedBufferDescriptions.size() > 0)
		bufferDescriptions.insert(
			bufferDescriptions.end(),
			ownedBufferDescriptions.begin(),
			ownedBufferDescriptions.end()
		);

	m_materialDataAllocation =
		m_resourceManager.createResources({}, bufferDescriptions);

	std::vector<BufferHandle> handles =
		m_resourceManager.getBuffers(m_materialDataAllocation);

	uint32_t offset = sharedBufferDescriptions.size();
	for (const auto& [index, metadata] : m_materialMetadata) {
		std::vector<BufferHandle> materialHandles;

		if (metadata.materialBufferSize > 0) {
			materialHandles.push_back(handles.at(offset));
			offset++;
		}

		vk::DescriptorSet materialSet = createSet(
			metadata.materialBindings, metadata.materialLayout, materialHandles
		);

		m_materialData[index] = {
			.pipeline = metadata.pipeline,
			.materialBuffers = materialHandles,
			.materialSet = materialSet,
		};
	}
};
