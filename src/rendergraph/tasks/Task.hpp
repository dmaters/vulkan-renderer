#pragma once
#include <functional>
#include <variant>
#include <vulkan/vulkan.hpp>

#include "resources/Buffer.hpp"

struct TaskContext{
    vk::CommandBuffer& commandBuffer;
    std::vector<ImageHandle> images;
    std::vector<BufferHandle> buffers;
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
