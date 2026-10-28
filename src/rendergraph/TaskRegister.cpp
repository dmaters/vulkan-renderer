#include "TaskRegister.hpp"

rendergraph::ResourceIndex TaskRegister::registerStaticImage(std::string name) {
	auto index = m_resources.names.size();

	m_resources.names[index] = std::move(name);
	m_resources.types[index] = rendergraph::ResourceType::Image;

	return index;
}

rendergraph::ResourceIndex TaskRegister::registerStaticBuffer(std::string name) {
	auto index = m_resources.names.size();

	m_resources.names[index] = std::move(name);
	m_resources.types[index] = rendergraph::ResourceType::Buffer;

	return index;
}
