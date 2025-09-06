#pragma once
#include <functional>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "material/MaterialManager.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Primitive.hpp"
using ResourceIndex = uint32_t;

struct TaskContext {
	vk::CommandBuffer& commandBuffer;
	std::vector<ResourceIndex>& inputs;
	std::vector<ResourceIndex>& outputs;
	std::unordered_map<ResourceIndex, ImageHandle>& images;
	std::unordered_map<ResourceIndex, BufferHandle>& buffers;
	ResourceManager& resourceManager;
	MaterialManager& materialManager;
};

enum class TaskType {
	CPU,
	Graphic,
	Compute,
	Transfer,
};
using Task = std::function<void(TaskContext&)>;
