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
	void* baseHostAddress;
	std::unordered_map<ResourceIndex, uint32_t>& localBuffers;

	uint8_t currentFrameIndex;

	ResourceManager& resourceManager;
	MaterialManager& materialManager;
	const Scene& scene;

	template <typename T>
	T getInput(ResourceIndex index) {
		if constexpr (std::same_as<T, Buffer&>) {
			return resourceManager.getBuffer(buffers[inputs[index].first]);
		} else if constexpr (std::same_as<T, Image&>) {
			return resourceManager.getImage(images[inputs[index].first]);
		} else {
			assert(localBuffers.contains(index));
			return static_cast<T>(
				static_cast<std::byte*>(baseHostAddress) + localBuffers[index]
			);
		}
	}

	template <typename T>
	T getOutput(ResourceIndex index) {
		if constexpr (std::same_as<T, Buffer&>) {
			return resourceManager.getBuffer(buffers[outputs[index].first]);
		} else if constexpr (std::same_as<T, Image&>) {
			return resourceManager.getImage(images[outputs[index].first]);
		} else {
			assert(localBuffers.contains(index));
			return static_cast<T>(
				static_cast<std::byte*>(baseHostAddress) + localBuffers[index]
			);
		}
	}

	struct Descriptors {
		std::vector<vk::DescriptorImageInfo> _imageInfo = {};
		std::vector<vk::DescriptorBufferInfo> _bufferInfo = {};
		std::vector<vk::WriteDescriptorSet> descriptors = {};
	};

	Descriptors getDescriptors(bool isRenderPass = false) const;
};
