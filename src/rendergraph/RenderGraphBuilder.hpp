#pragma once

#include <vector>
#include <vulkan/vulkan.hpp>

#include "GraphData.hpp"
#include "tasks/Task.hpp"

namespace rendergraph::internal {

class RenderGraphBuilder {
private:
	const GraphData& m_data;

public:
	RenderGraphBuilder(const GraphData& data) : m_data(data) {}

	rendergraph::internal::ExecutionInfo getTasks(
		const std::vector<TaskIndex> taskList, ResourceIndex outputImage
	) const;
};
}  // namespace rendergraph::internal
