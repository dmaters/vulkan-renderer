#pragma once

#include <string_view>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

#include "ResourceUsage.hpp"
#include "resources/ResourceManager.hpp"
#include "tasks/Task.hpp"

using ResourceDependency = std::pair<ResourceIndex, ResourceUsage::Type>;

struct GraphData {
	struct TaskData {
		Task task;
		TaskType type;
		std::string_view name;
		std::vector<ResourceDependency> inputs;
		std::vector<ResourceDependency> outputs;
	};

	std::unordered_map<ResourceIndex, uint8_t> swapchainImageRatio;
	std::unordered_map<ResourceIndex, ResourceManager::ImageDescription> images;
	std::unordered_map<ResourceIndex, ResourceManager::BufferDescription>
		buffers;
	std::unordered_map<ResourceIndex, std::string_view> names;
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
	std::vector<FeatureIndex> m_taskFeatures;

	struct TaskResourceDependency;
	std::unordered_map<ResourceIndex, std::vector<TaskResourceDependency>>
		m_dependencies;

	std::unordered_map<ResourceIndex, std::string_view> m_names;

	struct ResourceAccess;
	ResourceAccess getResourceBarrier(
		uint32_t taskIndex,
		ResourceIndex resourceIndex,
		std::unordered_set<FeatureIndex>& enabledFeatures,
		bool readOnly
	) const;

	struct Barrier;
	Barrier getBarrier(
		uint32_t taskIndex, std::unordered_set<FeatureIndex>& enabledFeatures
	) const;

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
		std::vector<ResourceDependency> inputResources,
		std::vector<ResourceDependency> outputResources,
		Task task,
		FeatureIndex feature
	);

	GraphData getData() const;
	std::vector<GraphData::TaskData> getTasks(
		ResourceIndex outputImage,
		std::unordered_set<FeatureIndex>& enabledFeatures
	) const;
};

struct RenderGraphBuilder::TaskResourceDependency {
	ResourceUsage::Type usage;
	uint32_t taskIndex;
	enum class UsageType {
		Input,
		Output,
	};
	UsageType usageType;
};

struct RenderGraphBuilder::Barrier {
	std::optional<GraphData::TaskData> barrierTask;
	std::unordered_set<uint32_t> previousTasks;
};