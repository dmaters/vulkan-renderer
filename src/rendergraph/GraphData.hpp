#pragma once

#include <vector>

#include "ResourceUsage.hpp"
#include "Task.hpp"
#include "resources/ResourceManager.hpp"

namespace rendergraph::internal {

struct GraphData {
	std::vector<std::pair<ResourceIndex, ImageHandle>> externalImages;
	std::vector<std::pair<ResourceIndex, BufferHandle>> externalBuffers;

	std::unordered_map<ResourceIndex, std::string> resourceNames;

	std::unordered_map<TaskIndex, Task> tasks;
	std::unordered_map<TaskIndex, std::vector<std::byte>> taskData;

	struct TaskMetaData {
		Task::Type type;
		std::string name;
	};

	std::unordered_map<TaskIndex, TaskMetaData> taskMetadata;

	ResourceIndexer indexer;
};

struct ExecutionInfo {
	struct Resources {
		std::unordered_map<rendergraph::ResourceIndex, std::string> names;

		struct ImageBuildData {
			ResourceManager::ImageDescription description;
			ResourceManager::MemoryLocation location;
		};
		std::unordered_map<rendergraph::ImageIndex, ImageBuildData> images;

		struct BufferBuildData {
			ResourceManager::BufferDescription description;
			ResourceManager::MemoryLocation location;
		};
		std::unordered_map<rendergraph::ImageIndex, BufferBuildData> buffers;
	};
	Resources resources;

	struct References {
		std::unordered_map<TaskIndex, std::vector<Task::ResourceDependency>>
			inputs;
		std::unordered_map<TaskIndex, std::vector<Task::ResourceDependency>>
			outputs;
	};
	References references;

	std::vector<TaskIndex> tasks;

	struct Barrier {
		ResourceUsage::Type previousUsage;
		ResourceUsage::Type currentUsage;
		ResourceIndex index;
	};
	std::unordered_map<TaskIndex, std::vector<Barrier>> barriers;

	std::vector<Barrier> initializationBarriers;

	struct InstanceData;
};

}  // namespace rendergraph::internal
