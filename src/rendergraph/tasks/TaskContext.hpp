#pragma once
#include "Task.hpp"
#include "material/MaterialManager.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"

struct TaskContext {
	vk::CommandBuffer& commandBuffer;
	std::span<const ResourceDependency>& inputs;
	std::span<const ResourceDependency>& outputs;
	std::unordered_map<ResourceIndex, ImageHandle>& images;
	std::unordered_map<ResourceIndex, BufferHandle>& buffers;
	ResourceManager& resourceManager;
	MaterialManager& materialManager;
	const Scene& scene;

	template <ContextResourceRef T>
	T getInput(uint32_t index) {
		if constexpr (std::same_as<T, Buffer&>) {
			return resourceManager.getBuffer(buffers[inputs[index].first]);
		} else if constexpr (std::same_as<T, Image&>) {
			return resourceManager.getImage(images[inputs[index].first]);
		}
	}

	template <ContextResourceRef T>
	T getOutput(uint32_t index) {
		if constexpr (std::same_as<T, Buffer&>) {
			return resourceManager.getBuffer(buffers[outputs[index].first]);
		} else if constexpr (std::same_as<T, Image&>) {
			return resourceManager.getImage(images[outputs[index].first]);
		}
	}

	struct Descriptors {
		std::vector<vk::DescriptorImageInfo> _imageInfo = {};
		std::vector<vk::DescriptorBufferInfo> _bufferInfo = {};
		std::vector<vk::WriteDescriptorSet> descriptors = {};
	};

	Descriptors getDescriptors(bool isRenderPass = false) const;
};
