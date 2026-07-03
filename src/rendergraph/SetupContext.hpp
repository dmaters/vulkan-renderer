#pragma once
#include <algorithm>

#include "DataProvider.hpp"
#include "GraphData.hpp"
#include "Task.hpp"
#include "scene/Scene.hpp"

struct Task::SetupContext {
	TaskIndex task;

	class ResourceProvider;

	const Scene& scene;
	ResourceProvider& resourceProvider;
	DataProvider& dataProvider;

	struct ResourceReference {
		struct SlotReference {
			TaskIndex task;
			std::size_t slot;
		};

		std::variant<rendergraph::ResourceIndex, SlotReference> reference;

		ResourceUsage::Type usage;
	};
	std::vector<ResourceReference>& inputs;
	std::vector<ResourceReference>& outputs;

	rendergraph::ResourceIndex createImage(
		std::string name,
		ResourceManager::ImageDescription description,
		ResourceManager::MemoryLocation location =
			ResourceManager::MemoryLocation::Device
	);
	rendergraph::ResourceIndex createBuffer(
		std::string name,
		ResourceManager::BufferDescription description,
		ResourceManager::MemoryLocation location =
			ResourceManager::MemoryLocation::Device
	);

	void registerInput(
		rendergraph::ResourceIndex index, ResourceUsage::Type usage
	) {
		inputs.push_back({ .reference = index, .usage = usage });
	}
	void registerInput(
		TaskIndex task, std::size_t slot, ResourceUsage::Type usage
	) {
		inputs.push_back(
			{
				.reference =
					ResourceReference::SlotReference {
													  .task = task,
													  .slot = slot,
													  },
				.usage = usage,
        }
		);
	}

	void registerOutput(
		rendergraph::ResourceIndex index, ResourceUsage::Type usage
	) {
		outputs.push_back(
			{
				.reference = index,
				.usage = usage,
			}
		);
	}
	void registerOutput(
		TaskIndex task, std::size_t slot, ResourceUsage::Type usage
	) {
		outputs.push_back(
			{
				.reference =
					ResourceReference::SlotReference {
													  .task = task,
													  .slot = slot,
													  },
				.usage = usage,
        }
		);
	}

	template <typename T>
	T& getData() {
		return dataProvider.getData<T>(task);
	}
	template <typename T>
	T& getData(TaskIndex task) {
		return dataProvider.getData<T>(task);
	}
};

class Task::SetupContext::ResourceProvider {
public:
private:
	rendergraph::internal::ResourceIndexer m_indexer;

	std::unordered_map<
		rendergraph::ImageIndex,
		rendergraph::internal::ExecutionInfo::Resources::ImageBuildData>
		m_images;

	std::unordered_map<
		rendergraph::BufferIndex,
		rendergraph::internal::ExecutionInfo::Resources::BufferBuildData>
		m_buffers;

public:
	ResourceProvider(rendergraph::internal::ResourceIndexer baseIndexer);

	rendergraph::ResourceIndex createImage(
		std::string name,
		ResourceManager::ImageDescription description,
		ResourceManager::MemoryLocation location =
			ResourceManager::MemoryLocation::Device
	);
	rendergraph::ResourceIndex createBuffer(
		std::string name,
		ResourceManager::BufferDescription description,
		ResourceManager::MemoryLocation location =
			ResourceManager::MemoryLocation::Device
	);

	rendergraph::internal::ExecutionInfo::Resources getCompiledResources() &&;
};
