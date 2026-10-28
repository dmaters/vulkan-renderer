#pragma once

#include "DataProvider.hpp"
#include "GraphData.hpp"
#include "RenderingConfiguration.hpp"
#include "Task.hpp"
#include "scene/Scene.hpp"
#include "utils/IndexMap.hpp"

struct Task::SetupContext {
	TaskIndex task;

	class ResourceProvider;

	const Scene& scene;

	ResourceProvider& resourceProvider;
	DataProvider& dataProvider;

	const RenderingConfiguration& renderingConfiguration;

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
	IndexMap<rendergraph::ResourceIndex, std::string> m_names;
	IndexMap<rendergraph::ResourceIndex, rendergraph::ResourceType> m_types;

	IndexMap<rendergraph::ResourceIndex, rendergraph::internal::ExecutionInfo::Resources::ImageBuildData> m_images;
	IndexMap<rendergraph::ResourceIndex, rendergraph::internal::ExecutionInfo::Resources::BufferBuildData> m_buffers;

	std::size_t m_resourceCount = 0;
	TaskManager& m_taskManager;

public:
	ResourceProvider(TaskManager& taskManager, const TaskRegister& taskRegister);

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
