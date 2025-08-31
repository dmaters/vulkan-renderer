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
		std::unordered_map<ResourceIndex, ResourceDependency> requiredResources;
	};

	struct ImageStatus {
		vk::ImageLayout initialLayout;
		vk::ImageLayout finalLayout;
	};

	std::vector<TaskData> tasks;

	std::unordered_map<RenderGraphBuilder::ResourceIndex, ImageStatus> imageStatuses;
	std::unordered_map<RenderGraphBuilder::ResourceIndex, ResourceManager::ImageDescription> images;
	std::unordered_map<RenderGraphBuilder::ResourceIndex, ResourceManager::BufferDescription> buffers;
};

class RenderGraphBuilder {
public:
    typedef uint32_t ResourceIndex;

    struct ResourceDependency {
		ResourceUsage usage;
		std::optional<vk::ImageLayout> requiredLayout;
    };
private:
	struct RegisteredTask;
	struct ResourceTaskReference;

	typedef uint32_t TaskIndex;



	uint32_t m_resource = 0;
	std::unordered_map<ResourceIndex, ResourceManager::ImageDescription> m_images;
	std::unordered_map<ResourceIndex, ResourceManager::BufferDescription> m_buffers;



	std::unordered_map<ResourceIndex, std::vector<ResourceTaskReference>>
		m_imageReferences;
	std::unordered_map<ResourceIndex, std::vector<ResourceTaskReference>>
		m_bufferReferences;

	std::vector<RegisteredTask> m_tasks;

	GraphData::TaskData visitTask(RegisteredTask& task) const;
public:
	ResourceIndex createImage(std::string_view name, ResourceManager::ImageDescription desc);
	ResourceIndex createBuffer(std::string_view name, ResourceManager::BufferDescription desc);

	void addTask(std::string_view name, TaskType type, std::unordered_map<ResourceIndex, ResourceDependency> inputResources,std::vector<ResourceDependency> outputResources, Task task);
	GraphData build();
};

struct RenderGraphBuilder::RegisteredTask {
	Task task;
	std::string_view name;
	std::unordered_map<ResourceIndex, ResourceDependency> input;
	std::unordered_map<ResourceIndex, ResourceDependency> output;
};
struct RenderGraphBuilder::ResourceTaskReference {
	TaskIndex task;
	ResourceUsage usage;
	std::optional<vk::ImageLayout> requiredLayout;
};
