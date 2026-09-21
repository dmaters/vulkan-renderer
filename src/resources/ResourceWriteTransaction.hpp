#pragma once

#include "resources/ResourceManager.hpp"

class ResourceWriteTransaction {
public:
	struct BufferReference {
		BufferHandle handle;
		uint32_t size;
		uint32_t offset;
	};
	struct ImageReference {
		ImageHandle handle;
		uint32_t mipLevel = 0;
		vk::ImageLayout initialLayout;
		vk::ImageLayout finalLayout;
	};

private:
	ResourceManager& m_resourceManager;
	vk::CommandBuffer m_commandBuffer;

public:
	ResourceWriteTransaction(vk::CommandPool& commandPool, ResourceManager& resourceManager);

	void copy(const BufferReference& source, const ImageReference& destination);
	void copy(const BufferReference& source, const BufferReference& destination);
	void imageClear(const ImageReference& image, const vk::ClearColorValue& clearValue);

	void submit(vk::Queue& queue, vk::Semaphore& signalSemaphore, uint64_t signalValue);
};
