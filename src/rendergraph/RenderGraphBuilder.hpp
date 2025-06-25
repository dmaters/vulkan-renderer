#pragma once

#include <array>
#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "resources/Image.hpp"
#include "resources/ResourceManager.hpp"

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
	std::unordered_map<std::string_view, vk::ImageMemoryBarrier2> imageBarriers;
	std::unordered_map<std::string_view, vk::BufferMemoryBarrier2>
		bufferBarriers;
};
class Task;
struct TaskData {
	std::unique_ptr<Task> task;
	std::string_view name;
	Barriers barriers;
	std::unordered_map<std::string_view, ImageDependencyInfo> images;
	std::unordered_map<std::string_view, BufferDependencyInfo> buffers;
};

struct GraphData {
	std::vector<TaskData> tasks;

	std::unordered_map<std::string_view, ImageDependencyInfo> requiredLayouts;
};

class RenderGraphBuilder {
private:
	struct RegisteredTask;
	struct ResourceReference;

	typedef uint32_t TaskIndex;

	std::unordered_map<std::string_view, std::vector<ResourceReference>>
		m_imageReferences;
	std::unordered_map<std::string_view, std::vector<ResourceReference>>
		m_bufferReferences;
	std::vector<RegisteredTask> m_tasks;

public:
	void addTask(std::unique_ptr<Task> task);

	std::vector<std::string_view> getReferencedResources() const;

	GraphData build();
};

struct RenderGraphBuilder::RegisteredTask {
	std::unique_ptr<Task> task;
	std::vector<ImageDependencyInfo> images;
	std::vector<BufferDependencyInfo> buffers;
};
struct RenderGraphBuilder::ResourceReference {
	TaskIndex task;
	ResourceUsage usage;
	std::optional<vk::ImageLayout> requiredLayout;
};
