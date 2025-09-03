#pragma once

#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

#include "resources/ResourceManager.hpp"
#include "tasks/Task.hpp"

struct ResourceDependency {
	ResourceUsage usage;
	vk::ImageLayout requiredLayout = vk::ImageLayout::eUndefined;
};

struct GraphData {
	struct Barriers {
		std::unordered_map<ResourceIndex, vk::ImageMemoryBarrier2>
			imageBarriers;
		std::unordered_map<ResourceIndex, vk::BufferMemoryBarrier2>
			bufferBarriers;
	};

	struct TaskData {
		Task task;
		TaskType type;
		std::string_view name;
		Barriers barriers;
		std::unordered_map<ResourceIndex, ResourceDependency> inputs;
		std::unordered_map<ResourceIndex, ResourceDependency> outputs;
	};
	std::vector<TaskData> tasks;

	ResourceIndex outputImage;

	std::unordered_map<ResourceIndex, vk::ImageLayout> requiredLayouts;
	std::unordered_map<ResourceIndex, uint8_t> swapchainImageRatio;
	std::unordered_map<ResourceIndex, ResourceManager::ImageDescription> images;
	std::unordered_map<ResourceIndex, ResourceManager::BufferDescription>
		buffers;
};

struct TaskResourceDependency {
	ResourceDependency dependencyInfo;
	uint32_t taskIndex;
	enum class UsageType {
		Input,
		Output,
	};
	UsageType usageType;
};

class RenderGraphBuilder {
private:
	uint32_t m_resourceCount = 0;

	std::unordered_map<ResourceIndex, ResourceManager::ImageDescription>
		m_images;
	std::unordered_map<ResourceIndex, uint8_t> m_swapchainImageRatio;

	std::unordered_map<ResourceIndex, ResourceManager::BufferDescription>
		m_buffers;

	std::vector<GraphData::TaskData> m_tasks;
	std::unordered_map<ResourceIndex, std::vector<TaskResourceDependency>>
		m_dependencies;

	ResourceIndex m_outputImage;
	GraphData::Barriers getBarriers(uint32_t task) const;

public:
	ResourceIndex createImage(
		std::string_view name,
		ResourceManager::ImageDescription desc,
		uint8_t swapchainRatio = 0
	);
	ResourceIndex createBuffer(
		std::string_view name, ResourceManager::BufferDescription desc
	);

	void addTask(
		std::string_view name,
		TaskType type,
		std::unordered_map<ResourceIndex, ResourceDependency> inputResources,
		std::unordered_map<ResourceIndex, ResourceDependency> outputResources,
		Task task
	);

	void setOutputImage(ResourceIndex index) { m_outputImage = index; }
	GraphData build();
};
