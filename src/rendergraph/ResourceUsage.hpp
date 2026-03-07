#pragma once

#include <optional>
#include <vulkan/vulkan.hpp>

namespace ResourceUsage {
enum class Type {
	Undefined,
	SampledRead,
	ShaderWrite,
	ShaderRead,
	ColorAttachmentWrite,
	DepthStencilRead,
	DepthStencilWrite,
	VertexBuffer,
	IndexBuffer,
	UniformBuffer,
	StorageBufferWrite,
	StorageBufferRead,
	IndirectBufferRead,
	TransferSrc,
	TransferDst,
};

std::optional<vk::DescriptorType> GetDescriptorType(
	ResourceUsage::Type usage, bool isBuffer
);

struct Access {
	vk::PipelineStageFlags2 stage;
	vk::AccessFlags2 access;
	vk::ImageLayout layout;
};
Access GetAccess(Type type);

bool IsReadAccess(Type type);
};  // namespace ResourceUsage
