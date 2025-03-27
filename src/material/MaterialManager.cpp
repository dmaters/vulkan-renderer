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
								.type = vk::DescriptorType::eSampledImage,
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
}
void MaterialManager::updateDescriptorSets(uint8_t currentFrame) {
	for (auto& material : m_materials) {
		material->globalSet = m_globalSets[currentFrame];
	}
}

MaterialInstance MaterialManager::instantiateMaterial(
	MaterialDescription& description
) {
	std::vector<vk::DescriptorSetLayoutBinding> resourcesLayouts;
	std::vector<ImageHandle> textures;
	for (const auto& resource : description.instanceResources) {
		resourcesLayouts.push_back({ .binding = resource.binding,
		                             .descriptorType = resource.type,
		                             .descriptorCount = resource.count,
		                             .stageFlags = resource.stage });
		if (resource.type == vk::DescriptorType::eSampledImage) {
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
	vk::DescriptorSetAllocateInfo descriptorInfo { .descriptorPool = m_pool,
		                                           .descriptorSetCount = 1,
		                                           .pSetLayouts =
		                                               &localLayout };

	vk::DescriptorSet set = m_device.allocateDescriptorSets(descriptorInfo)[0];

	std::vector<vk::WriteDescriptorSet> writeInfos;

	int binding = 0;
	for (const auto& texture : textures) {
		Image& image = m_resourceManager.getImage(texture);
		vk::DescriptorImageInfo imageInfo {
			.imageView = image.view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
		};

		writeInfos.push_back(vk::WriteDescriptorSet {
			.dstSet = set,
			.dstBinding = (uint32_t)(binding),
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eSampledImage,
			.pImageInfo = &imageInfo,
		});

		binding++;
	}

	m_device.updateDescriptorSets(
		writeInfos.size(), writeInfos.data(), 0, nullptr
	);

	Pipeline pipeline = PipelineBuilder::DefaultPipeline(pipelineInfo);
	if (m_materials.size() == 0) {
		m_materials.push_back(std::make_shared<Material>(Material {
			.pipeline = pipeline,
			.globalSet = m_globalSets[0],
			.materialSet = {},

		}));
	}

	MaterialInstance instance {
		.baseMaterial = m_materials[0],
		.instanceSet = { .set = set, .layout = localLayout },
	};

	return instance;
}
