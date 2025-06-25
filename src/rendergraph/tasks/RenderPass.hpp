#pragma once

#include <string_view>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "Task.hpp"
#include "material/MaterialManager.hpp"
#include "rendergraph/RenderGraphBuilder.hpp"

class RenderPass : Task {
public:
private:
	MaterialIndex m_material;
	std::vector<ResourceDependency> m_dependencies;

public:
	RenderPass(
		MaterialIndex material, std::vector<ResourceDependency> dependencies
	);

	void setup(
		std::vector<ImageDependencyInfo>& requiredImages,
		std::vector<BufferDependencyInfo>& requiredBuffers
	) override;

	void execute(vk::CommandBuffer& buffer, const Resources& resources)
		override;
};
