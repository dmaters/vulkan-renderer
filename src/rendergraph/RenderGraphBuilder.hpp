#pragma once

#include <string_view>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

#include "GraphData.hpp"
#include "ResourceUsage.hpp"
#include "resources/ResourceManager.hpp"
#include "tasks/Task.hpp"

namespace rendergraph::internal {

class RenderGraphBuilder {
private:
	const GraphData& m_data;

	std::vector<FeatureIndex> m_taskFeatures;
	struct TaskResourceDependency;
	std::unordered_map<ResourceIndex, std::vector<TaskResourceDependency>>
		m_dependencies;

	struct ResourceAccess;
	ResourceAccess getResourceBarrier(
		uint32_t taskIndex,
		ResourceIndex resourceIndex,
		std::unordered_set<FeatureIndex>& enabledFeatures,
		bool readOnly
	) const;

	struct Barrier;
	Barrier getBarrier(
		uint32_t taskIndex, std::unordered_set<FeatureIndex>& enabledFeatures
	) const;

public:
	RenderGraphBuilder(const GraphData& data) : m_data(data) {}

	ResourceIndex createImage(
		std::string_view name,
		ResourceManager::ImageDescription desc,
		uint8_t swapchainRatio = 0
	);
	ResourceIndex createBuffer(
		std::string_view name, ResourceManager::BufferDescription desc
	);

	void addTask(
		TaskIndex task,
		std::vector<ResourceDependency>& inputResources,
		std::vector<ResourceDependency>& outputResources,
		FeatureIndex feature
	);

	rendergraph::internal::ExecutionInfo getTasks(
		ResourceIndex outputImage,
		std::unordered_set<FeatureIndex>& enabledFeatures
	) const;
};
struct RenderGraphBuilder::TaskResourceDependency {
	ResourceUsage::Type usage;
	uint32_t taskIndex;
	enum class UsageType {
		Input,
		Output,
	};
	UsageType usageType;
};

}  // namespace rendergraph::internal
