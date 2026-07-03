#pragma once
#include "DataProvider.hpp"
#include "ResourceIndex.hpp"
#include "Task.hpp"
#include "material/MaterialManager.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"

struct Task::BuildContext {
	TaskIndex task;
	vk::CommandBuffer& commandBuffer;
	DataProvider& dataProvider;

	uint64_t currentFrame;
	std::unordered_map<rendergraph::ResourceIndex, ImageHandle>& images;
	std::unordered_map<rendergraph::ResourceIndex, BufferHandle>& buffers;

	std::span<ResourceDependency> inputs;
	std::span<ResourceDependency> outputs;

	ResourceManager& resourceManager;
	MaterialManager& materialManager;
	const Scene& scene;

	template <typename T>
	T getInput(std::size_t slot);

	template <typename T>
	T getOutput(std::size_t slot);

	Image& getImage(rendergraph::ResourceIndex index) {
		return resourceManager.getImage(images[index]);
	}
	Buffer& getBuffer(rendergraph::ResourceIndex index) {
		return resourceManager.getBuffer(buffers[index]);
	}

	struct Descriptors {
		std::vector<vk::DescriptorImageInfo> _imageInfo = {};
		std::vector<vk::DescriptorBufferInfo> _bufferInfo = {};
		std::vector<vk::WriteDescriptorSet> descriptors = {};
	};

	Descriptors getDescriptors() const;

	template <typename T>
	T& getData() {
		return dataProvider.getData<T>(task);
	}
	template <typename T>
	T& getData(TaskIndex task) {
		return dataProvider.getData<T>(task);
	}
};

template <typename T>
T Task::BuildContext::getInput(std::size_t slot) {
	if constexpr (std::same_as<T, Buffer&>) {
		return resourceManager.getBuffer(buffers[inputs[slot].resource]);
	} else if constexpr (std::same_as<T, Image&>) {
		return resourceManager.getImage(images[inputs[slot].resource]);
	} else {
		Buffer& buffer =
			resourceManager.getBuffer(buffers[inputs[slot].resource]);

		return static_cast<T>(buffer.data);
	}
}

template <typename T>
T Task::BuildContext::getOutput(std::size_t slot) {
	if constexpr (std::same_as<T, Buffer&>) {
		return resourceManager.getBuffer(buffers[outputs[slot].resource]);
	} else if constexpr (std::same_as<T, Image&>) {
		return resourceManager.getImage(images[outputs[slot].resource]);
	} else {
		Buffer& buffer =
			resourceManager.getBuffer(buffers[outputs[slot].resource]);

		return static_cast<T>(buffer.data);
	}
}
