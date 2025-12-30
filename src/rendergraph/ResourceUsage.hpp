#pragma once

#include <vulkan/vulkan.hpp>

namespace ResourceUsage {
enum class Type {
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
	TransferSrc,
	TransferDst,
};
struct Access {
	vk::PipelineStageFlags2 stage;
	vk::AccessFlags2 access;
	vk::ImageLayout layout;
};
static Access GetAccess(Type type) {
	switch (type) {
		case Type::SampledRead:
			return {
				.stage = vk::PipelineStageFlagBits2::eFragmentShader,
				.access = vk::AccessFlagBits2::eShaderSampledRead,
				.layout = vk::ImageLayout::eShaderReadOnlyOptimal,
			};
		case Type::ShaderRead:
			return {
				.stage = vk::PipelineStageFlagBits2::eComputeShader,
				.access = vk::AccessFlagBits2::eShaderRead,
				.layout = vk::ImageLayout::eGeneral,
			};
		case Type::ShaderWrite:
			return {
				.stage = vk::PipelineStageFlagBits2::eComputeShader,
				.access = vk::AccessFlagBits2::eShaderWrite,
				.layout = vk::ImageLayout::eGeneral,
			};
		case Type::ColorAttachmentWrite:
			return {
				.stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
				.access = vk::AccessFlagBits2::eColorAttachmentWrite,
				.layout = vk::ImageLayout::eColorAttachmentOptimal,
			};
		case Type::DepthStencilRead:
		case Type::DepthStencilWrite:
			return {
				.stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests |
				         vk::PipelineStageFlagBits2::eLateFragmentTests,
				.access = vk::AccessFlagBits2::eDepthStencilAttachmentRead |
				          vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
				.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
			};
		case Type::VertexBuffer:
			return {
				.stage = vk::PipelineStageFlagBits2::eVertexInput,
				.access = vk::AccessFlagBits2::eVertexAttributeRead,
				.layout = vk::ImageLayout::eUndefined,
			};
		case Type::IndexBuffer:
			return {
				.stage = vk::PipelineStageFlagBits2::eIndexInput,
				.access = vk::AccessFlagBits2::eIndexRead,
				.layout = vk::ImageLayout::eUndefined,
			};
		case Type::UniformBuffer:
			return {
				.stage = vk::PipelineStageFlagBits2::eVertexShader |
				         vk::PipelineStageFlagBits2::eFragmentShader,
				.access = vk::AccessFlagBits2::eUniformRead,
				.layout = vk::ImageLayout::eUndefined,
			};
		case Type::StorageBufferWrite:
			return {
				.stage = vk::PipelineStageFlagBits2::eComputeShader,
				.access = vk::AccessFlagBits2::eShaderStorageWrite,
				.layout = vk::ImageLayout::eUndefined,
			};
		case Type::StorageBufferRead:
			return {
				.stage = vk::PipelineStageFlagBits2::eComputeShader,
				.access = vk::AccessFlagBits2::eShaderStorageRead,
				.layout = vk::ImageLayout::eUndefined,
			};
		case Type::TransferSrc:
			return {
				.stage = vk::PipelineStageFlagBits2::eTransfer,
				.access = vk::AccessFlagBits2::eTransferRead,
				.layout = vk::ImageLayout::eTransferSrcOptimal,
			};
		case Type::TransferDst:
			return {
				.stage = vk::PipelineStageFlagBits2::eTransfer,
				.access = vk::AccessFlagBits2::eTransferWrite,
				.layout = vk::ImageLayout::eTransferDstOptimal,

			};
	}
}
std::optional<vk::DescriptorType> getDescriptorType(
	ResourceUsage::Type usage, bool isBuffer
);
};  // namespace ResourceUsage
