#include "RenderGraphBuilder.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <unordered_map>
#include <utility>
#include <vector>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "ResourceUsage.hpp"
#include "resources/ResourceManager.hpp"
#include "tasks/Task.hpp"
ResourceIndex RenderGraphBuilder::createImage(
	std::string_view name,
	ResourceManager::ImageDescription desc,
	uint8_t swapchainRatio
) {
	ResourceIndex index = ++m_resourceCount;
	m_images[index] = desc;

	if (swapchainRatio != 0) m_swapchainImageRatio[index] = swapchainRatio;

	m_names[index] = name;

	return index;
}

ResourceIndex RenderGraphBuilder::createBuffer(
	std::string_view name, ResourceManager::BufferDescription desc
) {
	ResourceIndex index = ++m_resourceCount;
	m_buffers[index] = desc;
	m_names[index] = name;

	return index;
}

GraphData::Barriers RenderGraphBuilder::getBarriers(uint32_t taskIndex) const {
	std::unordered_map<ResourceIndex, vk::ImageMemoryBarrier2> imageBarriers;
	std::unordered_map<ResourceIndex, vk::BufferMemoryBarrier2> bufferBarriers;

	for (auto& [index, reference] : m_tasks.at(taskIndex).inputs) {
		TaskResourceDependency lastAccess;

		TaskResourceDependency currentAccess;

		for (int i = 0; i < m_dependencies.at(index).size(); i++) {
			if (m_dependencies.at(index)[i].taskIndex != taskIndex) continue;

			lastAccess = m_dependencies.at(index).at(
				(i - 1) % m_dependencies.at(index).size()
			);

			currentAccess = m_dependencies.at(index).at(i);
		}

		if (m_images.contains(index)) {
			if (ResourceUsage::GetLayout(reference) ==
			    ResourceUsage::GetLayout(lastAccess.usage))
				continue;
			auto source = ResourceUsage::GetAccess(lastAccess.usage);
			auto destination = ResourceUsage::GetAccess(reference);
			imageBarriers[index] = vk::ImageMemoryBarrier2 {
				.srcStageMask = source.stage,
				.srcAccessMask = source.access,
				.dstStageMask = destination.stage,
				.dstAccessMask = destination.access,
				.oldLayout = ResourceUsage::GetLayout(lastAccess.usage),
				.newLayout = ResourceUsage::GetLayout(reference),
			};
		} else {
			auto source = ResourceUsage::GetAccess(lastAccess.usage);
			auto destination = ResourceUsage::GetAccess(reference);
			bufferBarriers[index] = vk::BufferMemoryBarrier2 {
				.srcStageMask = source.stage,
				.srcAccessMask = source.access,
				.dstStageMask = destination.stage,
				.dstAccessMask = destination.access,
			};
		}
	}

	for (auto& [index, reference] : m_tasks.at(taskIndex).outputs) {
		TaskResourceDependency lastAccess;
		TaskResourceDependency currentAccess;

		for (int i = 0; i < m_dependencies.at(index).size(); i++) {
			if (m_dependencies.at(index)[i].taskIndex != taskIndex) continue;

			lastAccess = m_dependencies.at(index).at(
				(i - 1) % m_dependencies.at(index).size()
			);

			currentAccess = m_dependencies.at(index).at(i);
		}

		if (m_images.contains(index)) {
			if (ResourceUsage::GetLayout(reference) ==
			    ResourceUsage::GetLayout(lastAccess.usage))
				continue;
			auto source = ResourceUsage::GetAccess(lastAccess.usage);
			auto destination = ResourceUsage::GetAccess(reference);
			imageBarriers[index] = vk::ImageMemoryBarrier2 {
				.srcStageMask = source.stage,
				.srcAccessMask = source.access,
				.dstStageMask = destination.stage,
				.dstAccessMask = destination.access,
				.oldLayout = ResourceUsage::GetLayout(lastAccess.usage),
				.newLayout = ResourceUsage::GetLayout(reference),
			};
		} else {
			auto source = ResourceUsage::GetAccess(lastAccess.usage);
			auto destination = ResourceUsage::GetAccess(reference);
			bufferBarriers[index] = vk::BufferMemoryBarrier2 {
				.srcStageMask = source.stage,
				.srcAccessMask = source.access,
				.dstStageMask = destination.stage,
				.dstAccessMask = destination.access,
			};
		}
	}

	return {
		.imageBarriers = imageBarriers,
		.bufferBarriers = bufferBarriers,
	};
}
void RenderGraphBuilder::addTask(
	std::string_view name,
	TaskType type,
	std::vector<ResourceDependency> inputResources,
	std::vector<ResourceDependency> outputResources,
	Task task
) {
	uint32_t taskIndex = m_tasks.size();

	m_tasks.push_back({
		.task = task,
		.type = type,
		.name = name,
		.inputs = inputResources,
		.outputs = outputResources,
	});

	for (auto& [index, dependency] : inputResources) {
		if (!m_dependencies.contains(index))
			m_dependencies[index] = std::vector<TaskResourceDependency>();

		assert(
			m_dependencies.size() > 0
		);  // We don't read uninitalized resource;

		m_dependencies[index].push_back(TaskResourceDependency {
			.usage = dependency,
			.taskIndex = taskIndex,
			.usageType = TaskResourceDependency::UsageType::Input,
		});
	}

	for (auto& [index, dependency] : outputResources) {
		if (!m_dependencies.contains(index))
			m_dependencies[index] = std::vector<TaskResourceDependency>();

		assert(
			m_dependencies[index].size() == 0 ||
			(m_dependencies[index].size() > 0 &&
		     m_dependencies[index].back().usageType !=
		         TaskResourceDependency::UsageType::Input)
		);  // We don't read uninitalized resource;

		m_dependencies[index].push_back(TaskResourceDependency {
			.usage = dependency,
			.taskIndex = taskIndex,
			.usageType = TaskResourceDependency::UsageType::Output,
		});
	}
}

GraphData RenderGraphBuilder::build() {
	std::unordered_map<ResourceIndex, vk::ImageLayout> requiredInitialLayouts;

	GraphData data;

	for (int i = 0; i < m_tasks.size(); i++) {
		m_tasks[i].barriers = getBarriers(i);
	}

	for (auto& [index, dependencies] : m_dependencies) {
		auto findFunc = [](TaskResourceDependency& dependency) {
			return ResourceUsage::GetLayout(dependency.usage) !=
			       vk::ImageLayout::eUndefined;
		};

		auto lastLayout =
			std::find_if(dependencies.rbegin(), dependencies.rend(), findFunc);

		if (lastLayout == dependencies.rend()) continue;

		requiredInitialLayouts[index] =
			ResourceUsage::GetLayout(lastLayout->usage);
	}

	return {
		.tasks = m_tasks,
		.outputImage = m_outputImage,
		.requiredLayouts = requiredInitialLayouts,
		.swapchainImageRatio = m_swapchainImageRatio,
		.images = m_images,
		.buffers = m_buffers,
		.names = m_names,
	};
}
