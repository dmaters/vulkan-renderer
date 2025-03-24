#include "MaterialManager.hpp"

#include <cstddef>
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
}

std::array<Buffer, 3> MaterialManager::setupMainDescriptorSet() {
	ResourceManager::BufferDescription uboDesc {
		.size = sizeof(GlobalResources::Camera),
		.usage = vk::BufferUsageFlagBits::eUniformBuffer |
		         vk::BufferUsageFlagBits::eTransferDst,
		.location = AllocationLocation::Device,
	};

	std::array<Buffer, 3> ubos {
		m_resourceManager.createBuffer(uboDesc),
		m_resourceManager.createBuffer(uboDesc),
		m_resourceManager.createBuffer(uboDesc),
	};

	std::array<vk::DescriptorSetLayoutBinding, 1> bindings {
		vk::DescriptorSetLayoutBinding {
										.binding = 0,
										.descriptorType = vk::DescriptorType::eUniformBuffer,
										.descriptorCount = 1,
										.stageFlags = vk::ShaderStageFlagBits::eAllGraphics,
										.pImmutableSamplers = {} },
	};

	vk::DescriptorSetLayoutCreateInfo info {
		.flags = {},
		.bindingCount = 1,
		.pBindings = bindings.data(),
	};

	vk::DescriptorSetLayout layout = m_device.createDescriptorSetLayout(info);

	std::array<vk::DescriptorSetLayout, 3> layouts = { layout, layout, layout };

	vk::DescriptorSetAllocateInfo allocateInfo { .descriptorPool = m_pool,
		                                         .descriptorSetCount = 3,
		                                         .pSetLayouts =
		                                             layouts.data() };

	auto sets = m_device.allocateDescriptorSets(allocateInfo);

	for (int i = 0; i < 3; i++) {
		vk::DescriptorSet set = sets[i];

		m_globalDescriptorSets[i] = { .set = set, .layout = layout };

		vk::DescriptorBufferInfo bufferInfo {
			.buffer = ubos[i].buffer,
			.offset = 0,
			.range = sizeof(GlobalResources::Camera)
		};

		vk::WriteDescriptorSet writeInfo {
			.dstSet = set,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.pImageInfo = {},
			.pBufferInfo = &bufferInfo,
			.pTexelBufferView = {}

		};
		m_device.updateDescriptorSets(writeInfo, {});
	}

	return ubos;
}

MaterialManager::MaterialInstance MaterialManager::instantiateMaterial(
	MaterialManager::MaterialDescription& description
) {
	std::vector<vk::DescriptorSetLayoutBinding> resourcesLayouts;
	std::vector<Image> textures;
	for (const auto& resource : description.resources) {
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
		.layouts = { m_globalDescriptorSets[0].layout, localLayout }
	};
	vk::DescriptorSetAllocateInfo descriptorInfo { .descriptorPool = m_pool,
		                                           .descriptorSetCount = 1,
		                                           .pSetLayouts =
		                                               &localLayout };

	vk::DescriptorSet set = m_device.allocateDescriptorSets(descriptorInfo)[0];

	std::vector<vk::WriteDescriptorSet> writeInfos;

	int binding = 0;
	for (const auto& texture : textures) {
		vk::DescriptorImageInfo imageInfo { .imageView = texture.view,
			                                .imageLayout = texture.layout };

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
		m_materials.push_back({
			.pipeline = pipeline,
			.globalSets = m_globalDescriptorSets,
		});
	}

	MaterialInstance instance {
		.baseMaterial = m_materials[0],
		.instanceSet = { .set = set, .layout = localLayout },
	};

	return instance;
}
