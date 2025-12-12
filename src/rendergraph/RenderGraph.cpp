#include "RenderGraph.hpp"

RenderGraph::RenderGraph(
	Swapchain& swapchain,
	ResourceManager& resourceManager,
	MaterialManager& materialManager
) :
	m_swapchain(swapchain),
	m_resourceManager(resourceManager),
	m_materialManager(materialManager) {

	addFeatureFlag("baseline");

}

FeatureIndex RenderGraph::addFeatureFlag(
	std::string_view name, bool defaultValue
) {
	assert(!m_features.contains(name));

	FeatureIndex index = m_features.size();
	m_features[name] = index;
	if (defaultValue) m_enabledFeatures.insert(index);
	return index;
}

void RenderGraph::setFeatureFlag(std::string_view name, bool value) {
	assert(m_features.contains(name));
	FeatureIndex index = m_features[name];
	setFeatureFlag(index, value);
}
void RenderGraph::setFeatureFlag(FeatureIndex index, bool value) {
	m_graphUpdated = true;

	if (!value && m_enabledFeatures.contains(index))
		m_enabledFeatures.erase(index);

	if (value) {
		m_enabledFeatures.insert(index);
	}
}
void RenderGraph::submit(const Scene& scene) {
	assert(m_outputImage != UINT32_MAX);

	if (!m_runner.has_value()) {
		m_runner.emplace(
			m_swapchain,
			m_resourceManager,
			m_materialManager,
			m_builder.getData()
		);
	}
	if (m_graphUpdated) {
		m_graphUpdated = false;
		m_tasks = m_builder.getTasks(m_outputImage, m_enabledFeatures);
	}

	m_runner->submit(scene, m_outputImage, m_tasks);
}
