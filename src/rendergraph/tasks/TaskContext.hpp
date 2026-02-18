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

	uint64_t currentFrame;

	ResourceManager& resourceManager;
	MaterialManager& materialManager;
	const Scene& scene;

	template <typename T>
	T getInput(uint32_t index);

	template <typename T>
	std::span<T> getInputSpan(uint32_t index, bool perFrame = false);

	template <typename T>
	T getOutput(uint32_t index);

	template <typename T>
	std::span<T> getOutputSpan(uint32_t index, bool perFrame = false);

	struct Descriptors {
		std::vector<vk::DescriptorImageInfo> _imageInfo = {};
		std::vector<vk::DescriptorBufferInfo> _bufferInfo = {};
		std::vector<vk::WriteDescriptorSet> descriptors = {};
	};

	Descriptors getDescriptors() const;
};

template <typename T>
T TaskContext::getInput(uint32_t index) {
	if constexpr (std::same_as<T, Buffer&>) {
		return resourceManager.getBuffer(buffers[inputs[index].first]);
	} else if constexpr (std::same_as<T, Image&>) {
		return resourceManager.getImage(images[inputs[index].first]);
	} else {
		Buffer& buffer =
			resourceManager.getBuffer(buffers[inputs[index].first]);
		return static_cast<T>(
			static_cast<std::byte*>(baseHostAddress) + buffer.allocation.offset
		);
	}
}

template <typename T>
std::span<T> TaskContext::getInputSpan(uint32_t index, bool perFrame) {
	Buffer& buffer = resourceManager.getBuffer(buffers[inputs[index].first]);
	if (perFrame)
		return std::span<T>(
			static_cast<std::byte*>(baseHostAddress) +
				buffer.allocation.offset +
				(buffer.allocation.size / 3) * (currentFrame % 3),
			buffer.allocation.size / 3 / sizeof(T)
		);
	else
		return std::span<T>(
			static_cast<T>(baseHostAddress), buffer.allocation.size
		);
	;
}

template <typename T>
T TaskContext::getOutput(ResourceIndex index) {
	if constexpr (std::same_as<T, Buffer&>) {
		return resourceManager.getBuffer(buffers[outputs[index].first]);
	} else if constexpr (std::same_as<T, Image&>) {
		return resourceManager.getImage(images[outputs[index].first]);
	} else {
		Buffer& buffer =
			resourceManager.getBuffer(buffers[outputs[index].first]);

		return static_cast<T>(
			static_cast<std::byte*>(baseHostAddress) + buffer.allocation.offset
		);
	}
}

template <typename T>
std::span<T> TaskContext::getOutputSpan(uint32_t index, bool perFrame) {
	Buffer& buffer = resourceManager.getBuffer(buffers[outputs[index].first]);

	if (perFrame)
		return std::span<T>(
			reinterpret_cast<T*>(
				static_cast<std::byte*>(baseHostAddress) +
				buffer.allocation.offset +
				(buffer.allocation.size / 3) * (currentFrame % 3)
			),
			buffer.allocation.size / 3 / sizeof(T)
		);
	else
		return std::span<T>(
			reinterpret_cast<T*>(
				static_cast<std::byte*>(baseHostAddress) +
				buffer.allocation.offset
			),
			buffer.allocation.size / sizeof(T)
		);
	;
}
