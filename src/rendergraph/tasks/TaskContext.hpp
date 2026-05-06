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

	uint64_t currentFrame;

	ResourceManager& resourceManager;
	MaterialManager& materialManager;
	const Scene& scene;

	template <typename T>
	T getInput(uint32_t index);

	template <typename T>
	std::span<T> getInputSpan(uint32_t index);

	template <typename T>
	T getOutput(uint32_t index);

	template <typename T>
	std::span<T> getOutputSpan(uint32_t index);

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
		return resourceManager.getBuffer(buffers[inputs[index].resource]);
	} else if constexpr (std::same_as<T, Image&>) {
		return resourceManager.getImage(images[inputs[index].resource]);
	} else {
		Buffer& buffer =
			resourceManager.getBuffer(buffers[inputs[index].resource]);

		return static_cast<T>(buffer.data);
	}
}

template <typename T>
std::span<T> TaskContext::getInputSpan(uint32_t index) {
	Buffer& buffer = resourceManager.getBuffer(buffers[inputs[index].resource]);
	if (buffer.data != nullptr)
		return std::span<T>(
			reinterpret_cast<T*>(
				static_cast<std::byte*>(buffer.data) +
				(buffer.allocation.size / 3) * (currentFrame % 3)
			),
			buffer.allocation.size / 3 / sizeof(T)
		);
	else
		return std::span<T>(
			reinterpret_cast<T*>(buffer.data),
			buffer.allocation.size / sizeof(T)
		);
	;
}

template <typename T>
T TaskContext::getOutput(ResourceIndex index) {
	if constexpr (std::same_as<T, Buffer&>) {
		return resourceManager.getBuffer(buffers[outputs[index].resource]);
	} else if constexpr (std::same_as<T, Image&>) {
		return resourceManager.getImage(images[outputs[index].resource]);
	} else {
		Buffer& buffer =
			resourceManager.getBuffer(buffers[outputs[index].resource]);

		return static_cast<T>(buffer.data);
	}
}

template <typename T>
std::span<T> TaskContext::getOutputSpan(uint32_t index) {
	Buffer& buffer =
		resourceManager.getBuffer(buffers[outputs[index].resource]);

	if (buffer.data != nullptr)
		return std::span<T>(
			reinterpret_cast<T*>(
				static_cast<std::byte*>(buffer.data) +
				(buffer.allocation.size / 3) * (currentFrame % 3)
			),
			buffer.allocation.size / 3 / sizeof(T)
		);
	else
		return std::span<T>(
			reinterpret_cast<T*>(buffer.data),
			buffer.allocation.size / sizeof(T)
		);
	;
}
