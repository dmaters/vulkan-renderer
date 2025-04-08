#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "resources/ResourceManager.hpp"

struct ResourceUsage {
	enum class Type : uint8_t {
		WRITE = 0,
		READ_WRITE = 1,
		READ = 2,
	};
	Type type;
	vk::AccessFlags2 access;
	vk::PipelineStageFlags2 stage;

	bool operator==(const ResourceUsage& b) const {
		return (type == Type::READ && b.type == ResourceUsage::Type::READ) ||
		       (type == Type::WRITE && b.type == ResourceUsage::Type::WRITE);
	}
};
struct ResourceReference {
	std::string_view task;
	ResourceUsage usage;
	std::optional<vk::ImageLayout> requiredLayout;
};

struct ImageDependencyInfo {
	std::string_view name;
	ResourceUsage usage;
	std::optional<vk::ImageLayout> requiredLayout;
};

struct BufferDependencyInfo {
	std::string_view name;
	ResourceUsage usage;
};

struct Barriers {
	std::vector<vk::ImageMemoryBarrier2> imageBarriers;
	std::vector<vk::BufferMemoryBarrier2> bufferBarriers;
};

struct TaskData {
	std::string_view name;
	std::optional<std::array<Barriers, 3>> barrier;
	std::vector<ImageDependencyInfo> requiredImages;
	std::vector<BufferDependencyInfo> requiredBuffers;
};

struct GraphData {
	std::vector<TaskData> tasks;
	std::unordered_map<std::string_view, ImageDependencyInfo> requiredLayouts;
};

class Task;
class RenderGraphBuilder {
private:
	struct RegisteredTask;

	std::unordered_map<std::string_view, std::vector<ResourceReference>>
		m_imageReferences;
	std::unordered_map<std::string_view, std::vector<ResourceReference>>
		m_bufferReferences;
	std::unordered_map<std::string_view, RegisteredTask> m_tasks;

public:
	void addTask(std::string_view name, Task& task);
	GraphData build(
		const std::set<std::string_view>& internalResources,
		ResourceManager& resourceManager
	);
};

struct RenderGraphBuilder::RegisteredTask {
	std::string_view name;
	std::vector<ImageDependencyInfo> images;
	std::vector<BufferDependencyInfo> buffers;
};