#pragma once

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

	rendergraph::ResourceIndex createImage(
		std::string name,
		ResourceManager::ImageDescription description,
		ResourceManager::MemoryLocation location = ResourceManager::MemoryLocation::Device
	);
	rendergraph::ResourceIndex createBuffer(
		std::string name,
		ResourceManager::BufferDescription description,
		ResourceManager::MemoryLocation location = ResourceManager::MemoryLocation::Device
	);

	template <typename T>
	T& getData() {
		return dataProvider.getData<T>(task);
	}

	rendergraph::ResourceIndex getReference(TaskIndex task, std::size_t slot);
};
class Task::SetupContext::ResourceProvider {
public:
	class TaskManager {
	public:
		virtual rendergraph::ResourceIndex getResource(TaskIndex task, std::size_t slot) = 0;
	};

private:
	rendergraph::internal::ResourceIndexer m_indexer;
	std::unordered_map<rendergraph::ResourceIndex, std::string> m_names;

	std::unordered_map<rendergraph::ResourceIndex, rendergraph::internal::ExecutionInfo::Resources::ImageBuildData>
		m_images;

	std::unordered_map<rendergraph::ResourceIndex, rendergraph::internal::ExecutionInfo::Resources::BufferBuildData>
		m_buffers;

	TaskManager& m_taskManager;

public:
	ResourceProvider(rendergraph::internal::ResourceIndexer indexer, TaskManager& taskManager) :
		m_indexer(indexer), m_taskManager(taskManager) {}

	rendergraph::ResourceIndex createImage(
		std::string name,
		ResourceManager::ImageDescription description,
		ResourceManager::MemoryLocation location = ResourceManager::MemoryLocation::Device
	);
	rendergraph::ResourceIndex createBuffer(
		std::string name,
		ResourceManager::BufferDescription description,
		ResourceManager::MemoryLocation location = ResourceManager::MemoryLocation::Device
	);

	rendergraph::ResourceIndex getReference(TaskIndex task, std::size_t slot);

	rendergraph::internal::ExecutionInfo::Resources getCompiledResources() &&;
};
