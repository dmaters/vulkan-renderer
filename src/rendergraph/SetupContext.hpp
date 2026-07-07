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
		ResourceManager::MemoryLocation location =
			ResourceManager::MemoryLocation::Device
	);
	rendergraph::ResourceIndex createBuffer(
		std::string name,
		ResourceManager::BufferDescription description,
		ResourceManager::MemoryLocation location =
			ResourceManager::MemoryLocation::Device
	);

	template <typename T>
	T& getData() {
		return dataProvider.getData<T>(task);
	}

	rendergraph::ResourceIndex getReference(TaskIndex task, std::size_t slot);
};

class Task::SetupContext::ResourceProvider {
private:
	rendergraph::internal::ResourceIndexer m_indexer;
	std::unordered_map<rendergraph::ResourceIndex, std::string> m_names;

	std::unordered_map<
		rendergraph::ResourceIndex,
		rendergraph::internal::ExecutionInfo::Resources::ImageBuildData>
		m_images;

	std::unordered_map<
		rendergraph::ResourceIndex,
		rendergraph::internal::ExecutionInfo::Resources::BufferBuildData>
		m_buffers;

	const rendergraph::internal::ExecutionInfo::References& m_references;

public:
	ResourceProvider(
		rendergraph::internal::ResourceIndexer baseIndexer,
		const rendergraph::internal::ExecutionInfo::References& references
	) :
		m_indexer(baseIndexer), m_references(references) {}

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

	rendergraph::ResourceIndex getReference(TaskIndex task, std::size_t slot);

	rendergraph::internal::ExecutionInfo::Resources getCompiledResources() &&;
};
