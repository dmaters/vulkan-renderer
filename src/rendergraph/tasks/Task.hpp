#pragma once
#include <cstdint>
#include <functional>
#include <vulkan/vulkan.hpp>

#include "../ResourceUsage.hpp"
#include "resources/Buffer.hpp"
#include "resources/Image.hpp"

using ResourceIndex = uint32_t;
using FeatureIndex = uint16_t;
using TaskIndex = uint32_t;

template <typename T>
concept ContextResourceRef =
	std::same_as<T, Buffer&> || std::same_as<T, Image&>;

enum class TaskType {
	CPU,
	Graphic,
	Compute,
	Transfer,
};

struct TaskContext;
using Task = std::function<void(TaskContext&)>;
using ResourceDependency = std::pair<ResourceIndex, ResourceUsage::Type>;
