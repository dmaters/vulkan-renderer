#pragma once

#include <vulkan/vulkan.hpp>

#include "Task.hpp"
#include "material/MaterialManager.hpp"
#include "scene/Scene.hpp"

enum class AttachmentOp {
	ClearWrite,
	ReadWrite,
	Read,
};

void RenderPass(
	TaskContext& context,
	const std::vector<uint32_t>& primitives,
	MaterialIndex materialIndex,
	AttachmentOp color,
	AttachmentOp depth,
	bool indirect = false
);

std::vector<uint32_t> FrustumCulling(
	const Scene& scene,
	const std::vector<uint32_t>& bucket,
	const std::array<glm::vec4, 6>& planes
);
