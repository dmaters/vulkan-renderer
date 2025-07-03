#pragma once

#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

#include "resources/ResourceManager.hpp"
#include "tasks/Task.hpp"

struct ResourceDependency {
	ResourceUsage usage;
	std::optional<vk::ImageLayout> requiredLayout;
};

struct GraphData {
	struct Barriers {
		std::unordered_map<std::string_view, vk::ImageMemoryBarrier2>
			imageBarriers;
		std::unordered_map<std::string_view, vk::BufferMemoryBarrier2>
			bufferBarriers;
	};

	struct TaskData {
		Task task;
		std::string_view name;
		Barriers barriers;
		std::unordered_map<std::string_view, ResourceDependency> images;
		std::unordered_map<std::string_view, ResourceDependency> buffers;
	};

	struct ImageStatus {
		vk::ImageLayout initialLayout;
		vk::ImageLayout finalLayout;
	};

	std::vector<TaskData> tasks;
	std::unordered_set<std::string_view> transientImagesRequired;
	std::unordered_set<std::string_view> transientBuffersRequired;

	std::unordered_map<std::string_view, ImageStatus> imageStatuses;
};

class RenderGraphBuilder {
private:
	struct RegisteredTask;
	struct ResourceTaskReference;

	typedef uint32_t TaskIndex;

	std::unordered_map<std::string_view, std::vector<ResourceTaskReference>>
		m_imageReferences;
	std::unordered_map<std::string_view, std::vector<ResourceTaskReference>>
		m_bufferReferences;

	std::vector<RegisteredTask> m_tasks;

public:
	void addTask(Task task);

	std::vector<std::string_view> getReferencedResources() const;

	GraphData build();
};

struct RenderGraphBuilder::RegisteredTask {
	Task task;
	std::unordered_map<std::string_view, ResourceDependency> images;
	std::unordered_map<std::string_view, ResourceDependency> buffers;
};
struct RenderGraphBuilder::ResourceTaskReference {
	TaskIndex task;
	ResourceUsage usage;
	std::optional<vk::ImageLayout> requiredLayout;
};
