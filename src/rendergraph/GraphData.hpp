#pragma once

#include <vector>

#include "ResourceUsage.hpp"
#include "Task.hpp"
#include "resources/ResourceManager.hpp"
#include "utils/IndexMap.hpp"

namespace rendergraph::internal {

struct ExecutionInfo {
	struct Resources {
		IndexMap<rendergraph::ResourceIndex, std::string> names;
		IndexMap<rendergraph::ResourceIndex, rendergraph::ResourceType> types;

		struct ImageBuildData {
			ResourceManager::ImageDescription description;
			ResourceManager::MemoryLocation location;
		};
		IndexMap<rendergraph::ResourceIndex, ImageBuildData> images;

		struct BufferBuildData {
			ResourceManager::BufferDescription description;
			ResourceManager::MemoryLocation location;
		};
		IndexMap<rendergraph::ResourceIndex, BufferBuildData> buffers;

		IndexMap<rendergraph::ResourceIndex, ImageHandle> externalImages;
		IndexMap<rendergraph::ResourceIndex, BufferHandle> externalBuffers;
	};
	Resources resources;

	struct References {
		IndexMap<TaskIndex, std::vector<Task::ResourceDependency>> inputs;
		IndexMap<TaskIndex, std::vector<Task::ResourceDependency>> outputs;
	};
	References references;
	ResourceIndex outputImage;

	std::vector<TaskIndex> tasks;

	struct Barrier {
		ResourceUsage::Type previousUsage;
		ResourceUsage::Type currentUsage;
		ResourceIndex index;
	};
	IndexMap<TaskIndex, std::vector<Barrier>> barriers;

	std::vector<Barrier> initializationBarriers;

	struct InstanceData;
};

}  // namespace rendergraph::internal
