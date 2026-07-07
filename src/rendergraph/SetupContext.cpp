#include "SetupContext.hpp"

rendergraph::ResourceIndex Task::SetupContext::createImage(
	std::string name,
	ResourceManager::ImageDescription description,
	ResourceManager::MemoryLocation location
) {
	return resourceProvider.createImage(name, description, location);
}
rendergraph::ResourceIndex Task::SetupContext::createBuffer(
	std::string name,
	ResourceManager::BufferDescription description,
	ResourceManager::MemoryLocation location
) {
	return resourceProvider.createBuffer(name, description, location);
}

rendergraph::ResourceIndex Task::SetupContext::getReference(
	TaskIndex task, std::size_t slot
) {
	return resourceProvider.getReference(task, slot);
};

rendergraph::ResourceIndex Task::SetupContext::ResourceProvider::createImage(
	std::string name,
	ResourceManager::ImageDescription description,
	ResourceManager::MemoryLocation location
) {
	rendergraph::ResourceIndex index = m_indexer.registerResource(
		rendergraph::internal::ResourceIndexer::ResourceType::Image
	);

	m_images[index] = {
		.description = description,
		.location = location,
	};
	m_names[index] = name;

	return index;
}

rendergraph::ResourceIndex Task::SetupContext::ResourceProvider::createBuffer(
	std::string name,
	ResourceManager::BufferDescription description,
	ResourceManager::MemoryLocation location
) {
	rendergraph::ResourceIndex index = m_indexer.registerResource(
		rendergraph::internal::ResourceIndexer::ResourceType::Buffer
	);

	m_buffers[index] = {
		.description = description,
		.location = location,
	};
	m_names[index] = name;

	return index;
}

rendergraph::ResourceIndex Task::SetupContext::ResourceProvider::getReference(
	TaskIndex task, std::size_t slot
) {
	return m_references.outputs.at(task)[slot].resource;
}

rendergraph::internal::ExecutionInfo::Resources
Task::SetupContext::ResourceProvider::getCompiledResources() && {
	return {
		.names = std::move(m_names),
		.images = std::move(m_images),
		.buffers = std::move(m_buffers),
	};
};
