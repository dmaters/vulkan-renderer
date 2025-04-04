#include "MaterialManager.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Instance.hpp"
#include "Pipeline.hpp"
#include "material/MaterialManager.hpp"
#include "material/Pipeline.hpp"
#include "memory/MemoryAllocator.hpp"
#include "resources/Buffer.hpp"
#include "resources/ResourceManager.hpp"

MaterialManager::MaterialManager(
	Instance& instance, ResourceManager& resourceManager
) :
	m_device(instance.device), m_resourceManager(resourceManager) {
	std::array<vk::DescriptorPoolSize, 2> sizes = {
		vk::DescriptorPoolSize {
								.type = vk::DescriptorType::eUniformBuffer,
								.descriptorCount = 1,
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

	std::array<vk::DescriptorSetLayoutBinding, 1> bindings {
		vk::DescriptorSetLayoutBinding {
										.binding = 0,
										.descriptorType = vk::DescriptorType::eUniformBuffer,
										.descriptorCount = 1,
										.stageFlags = vk::ShaderStageFlagBits::eAllGraphics,
										.pImmutableSamplers = {} },
	};

	vk::DescriptorSetLayoutCreateInfo layoutInfo {
		.flags = {},
		.bindingCount = 1,
		.pBindings = bindings.data(),
	};

	vk::DescriptorSetLayout layout =
		m_device.createDescriptorSetLayout(layoutInfo);

	vk::DescriptorSetAllocateInfo allocateInfo {
		.descriptorPool = m_pool,
		.descriptorSetCount = 3,
		.pSetLayouts =
			std::array<vk::DescriptorSetLayout, 3> { layout, layout, layout }
				.data(),
	};

	auto sets = m_device.allocateDescriptorSets(allocateInfo);

	for (int i = 0; i < 3; i++) {
		Buffer& buffer = m_resourceManager.getNamedBuffer("gset_buffer");
		vk::DescriptorBufferInfo bufferInfo {
			.buffer = buffer.buffer,
			.offset = buffer.bufferAccess[i].offset,
			.range = sizeof(GlobalResources::Camera)
		};

		vk::WriteDescriptorSet writeInfo {
			.dstSet = sets[i],
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.pImageInfo = {},
			.pBufferInfo = &bufferInfo,
			.pTexelBufferView = {},

		};
		m_device.updateDescriptorSets(writeInfo, {});
		m_globalSets[i] = { .set = sets[i], .layout = layout };
	}

	vk::SamplerCreateInfo samplerInfo {
		.magFilter = vk::Filter::eLinear,
		.minFilter = vk::Filter::eLinear,
		.mipmapMode = vk::SamplerMipmapMode::eLinear,
		.addressModeU = vk::SamplerAddressMode::eRepeat,
		.addressModeV = vk::SamplerAddressMode::eRepeat,
		.addressModeW = vk::SamplerAddressMode::eRepeat,
	};

	m_linearSampler = m_device.createSampler(samplerInfo);
}
void MaterialManager::updateDescriptorSets(uint8_t currentFrame) {
	for (auto& material : m_materials) {
		material->globalSet = m_globalSets[currentFrame];
	}
}

uint32_t MaterialManager::createMaterial(MaterialDescription& description) {
	auto key = std::pair(description.vertex, description.fragment);

	// if (m_materialCache.contains(key)) return m_materialCache[key];

	if (m_materials.size() > 0) return 0;

	std::vector<vk::DescriptorSetLayoutBinding> resourcesLayouts;
	std::vector<ImageHandle> textures;
	for (const auto& resource : description.instanceResources) {
		resourcesLayouts.push_back({ .binding = resource.binding,
		                             .descriptorType = resource.type,
		                             .descriptorCount = resource.count,
		                             .stageFlags = resource.stage });
		if (resource.type == vk::DescriptorType::eCombinedImageSampler) {
			textures.push_back(m_resourceManager.loadImage(resource.path));
		}
	}
	vk::DescriptorSetLayoutCreateInfo layoutInfo {

		.bindingCount = (uint32_t)resourcesLayouts.size(),
		.pBindings = resourcesLayouts.data()
	};
	vk::DescriptorSetLayout localLayout =
		m_device.createDescriptorSetLayout(layoutInfo);

	PipelineBuilder::PipelineBuildInfo pipelineInfo {
		.device = m_device,
		.vertex = description.vertex,
		.fragment = description.fragment,
		.layouts = { m_globalSets[0].layout, localLayout }
	};
	Pipeline pipeline = PipelineBuilder::DefaultPipeline(pipelineInfo);
	m_materials.push_back(std::make_shared<Material>(Material {
		.pipeline = pipeline,
		.globalSet = m_globalSets[0],
		.materialSet = {},
		.instanceLayout = localLayout,
	}));

	uint32_t materialIndex = m_materials.size() - 1;

	// m_materialCache[key] = materialIndex;

	return materialIndex;
}
MaterialInstance MaterialManager::instantiateMaterial(
	MaterialDescription& description
) {
	Material& material = *m_materials[createMaterial(description)];
	vk::DescriptorSetAllocateInfo descriptorInfo {
		.descriptorPool = m_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &material.instanceLayout
	};

	vk::DescriptorSet set = m_device.allocateDescriptorSets(descriptorInfo)[0];

	std::vector<vk::WriteDescriptorSet> writeInfos;

	int binding = 0;
	std::vector<ImageHandle> textures;
	for (const auto& resource : description.instanceResources) {
		if (resource.type == vk::DescriptorType::eCombinedImageSampler) {
			textures.push_back(m_resourceManager.loadImage(resource.path));
		}
	}
	for (const auto& texture : textures) {
		Image& image = m_resourceManager.getImage(texture);
		vk::DescriptorImageInfo imageInfo {
			.sampler = m_linearSampler,
			.imageView = image.view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
		};

		writeInfos.push_back(vk::WriteDescriptorSet {
			.dstSet = set,
			.dstBinding = (uint32_t)(binding),
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.pImageInfo = &imageInfo,
		});

		binding++;
	}

	m_device.updateDescriptorSets(
		writeInfos.size(), writeInfos.data(), 0, nullptr
	);

	material.instanceSets.push_back(set);
	return { (uint32_t)material.instanceSets.size() - 1 };
}
