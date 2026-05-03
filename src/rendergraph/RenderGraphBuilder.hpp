#pragma once

#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "GraphData.hpp"
#include "ResourceUsage.hpp"
#include "resources/ResourceManager.hpp"
#include "tasks/Task.hpp"

namespace rendergraph::internal {

class RenderGraphBuilder {
private:
	const GraphData& m_data;

	struct TaskResourceDependency;
	std::unordered_map<ResourceIndex, std::vector<TaskResourceDependency>>
		m_dependencies;

	struct TaskReference;
	TaskReference getPreviousTask(
		uint32_t taskIndex, ResourceIndex resourceIndex
	) const;

	std::unordered_set<uint32_t> getReferencedTasks(uint32_t taskIndex) const;

public:
	RenderGraphBuilder(const GraphData& data) : m_data(data) {}

	void addTask(
		TaskIndex task,
		std::vector<ResourceDependency>& inputResources,
		std::vector<ResourceDependency>& outputResources
	);

	rendergraph::internal::ExecutionInfo getTasks(
		ResourceIndex outputImage
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
