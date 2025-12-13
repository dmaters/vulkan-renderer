#pragma once
#include <functional>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "../ResourceUsage.hpp"
#include "material/MaterialManager.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"

using ResourceIndex = uint32_t;
using FeatureIndex = uint16_t;
using TaskIndex = uint32_t;

struct TaskContext {
	vk::CommandBuffer& commandBuffer;
	std::vector<ResourceIndex>& inputs;
	std::vector<ResourceIndex>& outputs;
	std::unordered_map<ResourceIndex, ImageHandle>& images;
	std::unordered_map<ResourceIndex, BufferHandle>& buffers;
	ResourceManager& resourceManager;
	MaterialManager& materialManager;
	const Scene& scene;
};

enum class TaskType {
	CPU,
	Graphic,
	Compute,
	Transfer,
};
using Task = std::function<void(TaskContext&)>;
using ResourceDependency = std::pair<ResourceIndex, ResourceUsage::Type>;
