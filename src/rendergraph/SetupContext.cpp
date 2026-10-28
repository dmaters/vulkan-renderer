#include "SetupContext.hpp"

rendergraph::ResourceIndex Task::SetupContext::createImage(
	std::string name, ResourceManager::ImageDescription description, ResourceManager::MemoryLocation location
) {
	return resourceProvider.createImage(name, description, location);
}
rendergraph::ResourceIndex Task::SetupContext::createBuffer(
	std::string name, ResourceManager::BufferDescription description, ResourceManager::MemoryLocation location
) {
	assert(description.size > 0);
	return resourceProvider.createBuffer(name, description, location);
}

rendergraph::ResourceIndex Task::SetupContext::getReference(TaskIndex task, std::size_t slot) {
	return resourceProvider.getReference(task, slot);
};

Task::SetupContext::ResourceProvider::ResourceProvider(TaskManager& taskManager, const TaskRegister& taskRegister) :
	m_taskManager(taskManager) {
	const auto& resources = taskRegister.getStaticResources();
	for (const auto& [resource, name] : resources.names.items()) {
		m_names[resource] = name;
		m_types[resource] = *resources.types[resource];
	}
	m_resourceCount = m_names.size();
}

rendergraph::ResourceIndex Task::SetupContext::ResourceProvider::createImage(
	std::string name, ResourceManager::ImageDescription description, ResourceManager::MemoryLocation location
) {
	rendergraph::ResourceIndex index = m_resourceCount++;

	m_images[index] = {
		.description = description,
		.location = location,
	};
	m_names[index] = name;
	m_types[index] = rendergraph::ResourceType::Image;

	return index;
}

rendergraph::ResourceIndex Task::SetupContext::ResourceProvider::createBuffer(
	std::string name, ResourceManager::BufferDescription description, ResourceManager::MemoryLocation location
) {
	rendergraph::ResourceIndex index = m_resourceCount++;

	m_buffers[index] = {
		.description = description,
		.location = location,
	};
	m_names[index] = name;
	m_types[index] = rendergraph::ResourceType::Buffer;

	return index;
}

rendergraph::ResourceIndex Task::SetupContext::ResourceProvider::getReference(TaskIndex task, std::size_t slot) {
	return m_taskManager.getResource(task, slot);
}

rendergraph::internal::ExecutionInfo::Resources Task::SetupContext::ResourceProvider::getCompiledResources() && {
	return {
		.names = std::move(m_names),
		.types = std::move(m_types),
		.images = std::move(m_images),
		.buffers = std::move(m_buffers),
	};
};
