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
#include "resources/Buffer.hpp"
#include "resources/MemoryAllocator.hpp"
#include "resources/ResourceManager.hpp"

MaterialManager::MaterialManager(Instance& instance) :
	m_device(instance.device),
	m_globalResources(std::make_unique<GlobalResources>()),
	m_resourceManager(instance)

{
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
	setupGlobalResources();
}

void MaterialManager::setupGlobalResources() {
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

	std::array<vk::DescriptorSetLayout, 1> layouts = { layout };

	vk::DescriptorSetAllocateInfo allocateInfo { .descriptorPool = m_pool,
		                                         .descriptorSetCount = 1,
		                                         .pSetLayouts =
		                                             layouts.data() };

	vk::DescriptorSet set = m_device.allocateDescriptorSets(allocateInfo)[0];

	ResourceManager::BufferHandle buffer =
		m_resourceManager.createBuffer(sizeof(GlobalResources::CameraBuffer));

	m_globalDescriptor = DescriptorSet { .set = set, .layout = layout };

	vk::DescriptorBufferInfo bufferInfo {
		.buffer = buffer.get().buffer,
		.offset = 0,
		.range = sizeof(GlobalResources::CameraBuffer)
	};

	vk::WriteDescriptorSet writeInfo { .dstSet = set,
		                               .dstBinding = 0,
		                               .descriptorCount = 1,
		                               .descriptorType =
		                                   vk::DescriptorType::eUniformBuffer,
		                               .pImageInfo = {},
		                               .pBufferInfo = &bufferInfo,
		                               .pTexelBufferView = {}

	};

	m_globalResourceBuffers.push_back(buffer);
	m_device.updateDescriptorSets(writeInfo, {});
}

MaterialHandle MaterialManager::instantiateMaterial(
	MaterialManager::MaterialDescription& description
) {
	std::vector<vk::DescriptorSetLayoutBinding> resourcesLayouts;
	std::vector<ResourceManager::ImageHandle> textures;
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
		.layouts = { m_globalDescriptor.layout, localLayout }
	};
	vk::DescriptorSetAllocateInfo descriptorInfo { .descriptorPool = m_pool,
		                                           .descriptorSetCount = 1,
		                                           .pSetLayouts =
		                                               &localLayout };

	vk::DescriptorSet set = m_device.allocateDescriptorSets(descriptorInfo)[0];

	std::vector<vk::WriteDescriptorSet> writeInfos;

	int binding = 0;
	for (const auto& texture : textures) {
		const Image& image = m_resourceManager.getImage(texture);

		vk::DescriptorImageInfo imageInfo { .imageView = image.view,
			                                .imageLayout = image.layout };

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

	MaterialInstance instance {
		.pipeline = PipelineBuilder::DefaultPipeline(pipelineInfo),
		.instanceSet = { .set = set, .layout = localLayout },
		.textures = textures
	};
	MaterialHandle handle = MaterialHandle();
	m_materials.insert({ handle, instance });
	return handle;
}

const MaterialManager::DescriptorSet& MaterialManager::getGlobalDescriptorSet(
) {
	if (m_dirtyGlobalDescriptors) {
		m_dirtyGlobalDescriptors = false;

		Buffer buffer = m_globalResourceBuffers[0].get();
		unsigned char* dataPointer = (unsigned char*)&m_globalResources->camera;
		std::vector<unsigned char> data(
			dataPointer, dataPointer + sizeof(GlobalResources::CameraBuffer)
		);
		buffer.allocation.updateData(data);
	}

	return m_globalDescriptor;
};