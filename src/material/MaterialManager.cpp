#include "MaterialManager.hpp"

#include <cassert>
#include <cstdint>
#include <memory>
#include <unordered_map>
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
}

void MaterialManager::createGlobalBuffers() {
	BufferHandle lightBuffer = m_resourceManager.createBuffer({
		.size = sizeof(MaterialDefinitions::Lights),
		.usage = vk::BufferUsageFlagBits::eTransferDst |
	             vk::BufferUsageFlagBits::eUniformBuffer,
		.location = AllocationLocation::Device,
	});

	m_resourceManager.setName("light_buffer", lightBuffer);

	BufferHandle viewProj = m_resourceManager.createBuffer({
		.size = sizeof(MaterialDefinitions::ViewProjection),
		.usage = vk::BufferUsageFlagBits::eTransferDst |
	             vk::BufferUsageFlagBits::eUniformBuffer,
		.location = AllocationLocation::Device,
	});

	m_resourceManager.setName("view_projection", viewProj);
}

void MaterialManager::createGlobalDescriptorSet() {
	vk::Device& device = Instance::Get().device;

	std::array<vk::DescriptorSetLayoutBinding, 3> bindings {
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
										.descriptorType = vk::DescriptorType::eCombinedImageSampler,
										.descriptorCount = 512,
										.stageFlags = vk::ShaderStageFlagBits::eAllGraphics,
										.pImmutableSamplers = {},
										}
	};

	std::array<vk::DescriptorBindingFlags, 3> bindingFlags = {
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

	Buffer& cameraBuffer = m_resourceManager.getNamedBuffer("view_projection");

	vk::DescriptorBufferInfo cameraDescriptorInfo {
		.buffer = cameraBuffer.buffer,
		.offset = cameraBuffer.bufferAccess[0].offset,
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

	Buffer& lightBuffer = m_resourceManager.getNamedBuffer("light_buffer");

	vk::DescriptorBufferInfo lightBufferDescriptorInfo {
		.buffer = lightBuffer.buffer,
		.offset = lightBuffer.bufferAccess[0].offset,
		.range = lightBuffer.size,
	};
	vk::WriteDescriptorSet lightBufferWriteDescriptor {
		.dstSet = set,
		.dstBinding = 1,
		.descriptorCount = 1,
		.descriptorType = vk::DescriptorType::eUniformBuffer,
		.pImageInfo = {},
		.pBufferInfo = &lightBufferDescriptorInfo,
		.pTexelBufferView = {},
	};
	device.updateDescriptorSets(
		{ cameraWriteDescriptor, lightBufferWriteDescriptor }, {}
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
	std::vector<vk::DescriptorSetLayoutBinding>& bindings,
	vk::DescriptorSetLayout layout,
	std::vector<BufferHandle>& materialBuffersHandles
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
			.offset = buffer.bufferAccess[0].offset,
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
	if (!m_materialData.contains(index)) {
		m_materialData[index] = createMaterialData(index);
	}
	MaterialData& data = m_materialData.at(index);
	Material material {
		.pipeline = m_shaderEngine->getPipeline(data.pipeline),
		.materialSet = data.materialSet,
	};

	return material;
}

void MaterialManager::syncData(std::vector<MirroredBuffer> buffers) {
	std::vector<ResourceManager::BufferCopy> info;

	for (MirroredBuffer buffer : buffers) {
		uint32_t size = m_resourceManager.getBuffer(buffer.localBuffer).size;

		info.push_back({
			.origin = buffer.localBuffer,
			.destination = buffer.deviceBuffer,
			.copy = {
					 .srcOffset = 0,
					 .dstOffset = 0,
					 .size = size,
					 }
        });
	}

	m_resourceManager.copyBuffers(info);
}

uint32_t MaterialManager::registerTextureGroup(
	const std::vector<ImageHandle>& textures
) {
	uint32_t offset = m_staticTextureGroup.size();

	m_staticTextureGroup.insert(
		m_staticTextureGroup.end(), textures.begin(), textures.end()
	);

	std::vector<vk::DescriptorImageInfo> info;
	for (auto& handle : textures) {
		Image& image = m_resourceManager.getImage(handle);

		info.push_back({
			.sampler = m_linearSampler,
			.imageView = image.accesses[0].view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		});
	}

	vk::WriteDescriptorSet writeInfo = {
		.dstSet = m_globalSet,
		.dstBinding = 2,
		.dstArrayElement = offset,
		.descriptorCount = (uint32_t)textures.size(),
		.descriptorType = vk::DescriptorType::eCombinedImageSampler,
		.pImageInfo = info.data(),
	};

	Instance::Get().device.updateDescriptorSets({ writeInfo }, {});
	return offset;
}

MaterialManager::MaterialData MaterialManager::createMaterialData(
	MaterialIndex index
) {
	MaterialMetadata metadata = m_materialMetadata[index];

	std::vector<MirroredBuffer> materialBuffers;
	std::vector<BufferHandle> materialBuffersHandles;

	for (int i = 0; i < metadata.materialBuffers.size(); i++) {
		ResourceManager& resourceManager = m_resourceManager;

		MirroredBuffer buffer = std::visit(
			[&resourceManager](auto&& buffer) {
				using T = std::decay_t<decltype(buffer)>;
				if constexpr (std::is_same_v<T, uint32_t>) {
					BufferHandle mirror = resourceManager.createBuffer(
						ResourceManager::BufferDescription {
							.size = buffer,
							.usage = vk::BufferUsageFlagBits::eTransferSrc,
							.location = AllocationLocation::Host,
						}
					);

					BufferHandle base = resourceManager.createBuffer(
						ResourceManager::BufferDescription {
							.size = buffer,
							.usage = vk::BufferUsageFlagBits::eUniformBuffer |
				                     vk::BufferUsageFlagBits::eTransferDst,
							.location = AllocationLocation::Device,
						}
					);
					return MirroredBuffer { mirror, base };
				} else if constexpr (std::is_same_v<T, std::string_view>) {
					return MirroredBuffer {
						BufferHandle { 0 },
						resourceManager.getNamedBufferIndex(buffer)
					};
				}
			},
			metadata.materialBuffers[i]
		);
		materialBuffers.push_back(buffer);
		materialBuffersHandles.push_back(buffer.deviceBuffer);
	}

	vk::DescriptorSet materialSet = createSet(
		metadata.materialBindings,
		metadata.materialLayout,
		materialBuffersHandles
	);

	return {
		.pipeline = metadata.pipeline,
		.materialBuffers = materialBuffers,
		.materialSet = materialSet,
	};
};
