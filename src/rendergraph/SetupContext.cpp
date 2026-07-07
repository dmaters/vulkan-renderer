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

void Task::SetupContext::registerInput(
	rendergraph::ResourceIndex index, ResourceUsage::Type usage
) {
	inputs.push_back({ .reference = index, .usage = usage });
}

void Task::SetupContext::registerInput(
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

void Task::SetupContext::registerOutput(
	rendergraph::ResourceIndex index, ResourceUsage::Type usage
) {
	outputs.push_back(
		{
			.reference = index,
			.usage = usage,
		}
	);
}
void Task::SetupContext::registerOutput(
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

rendergraph::internal::ExecutionInfo::Resources
Task::SetupContext::ResourceProvider::getCompiledResources() && {
	return {
		.names = std::move(m_names),
		.images = std::move(m_images),
		.buffers = std::move(m_buffers),
	};
};
