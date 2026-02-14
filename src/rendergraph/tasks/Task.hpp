#pragma once
#include <cstdint>
#include <functional>
#include <vulkan/vulkan.hpp>

#include "../ResourceUsage.hpp"
using ResourceIndex = uint32_t;
using FeatureIndex = uint16_t;
using TaskIndex = uint32_t;

enum class TaskType {
	CPU,
	Graphic,
	Compute,
	Transfer,
};

struct TaskContext;
using Task = std::function<void(TaskContext&)>;
using ResourceDependency = std::pair<ResourceIndex, ResourceUsage::Type>;
