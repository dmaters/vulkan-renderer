#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ResourceUsage.hpp"
#include "resources/ResourceManager.hpp"
#include "tasks/Task.hpp"
namespace rendergraph::internal {

using ResourceDependency = std::pair<ResourceIndex, ResourceUsage::Type>;

struct GraphData {
	struct TaskData {
		TaskType type;
		std::string name;

		struct ResourceDependencySpan {
			uint32_t offset;
			uint8_t count;
		};

		ResourceDependencySpan inputs;
		ResourceDependencySpan outputs;
	};

	std::unordered_map<ResourceIndex, uint8_t> swapchainImageRatio;
	std::unordered_map<ResourceIndex, ResourceManager::ImageDescription> images;
	std::unordered_map<ResourceIndex, ResourceManager::BufferDescription>
		buffers;
	std::unordered_map<ResourceIndex, uint32_t> localBufferSizes;
	std::unordered_map<ResourceIndex, ResourceUsage::Type> finalUsages;
	std::unordered_set<ResourceIndex> sharedBuffers;

	std::unordered_map<ResourceIndex, ImageHandle> externalImages;
	std::unordered_map<ResourceIndex, BufferHandle> externalBuffers;

	std::vector<std::string> resourceNames;
	std::vector<Task> tasks;
	std::vector<TaskData> taskData;
	std::vector<ResourceDependency> taskDependencies;
};

struct ExecutionInfo {
	std::vector<TaskIndex> tasks;
	std::vector<GraphData::TaskData> data;
	std::vector<Task> barriers;
	Task initializationTask;
	std::unordered_map<ResourceIndex, ResourceUsage::Type> finalUsages;
};

}  // namespace rendergraph::internal
