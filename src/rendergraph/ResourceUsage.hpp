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

static vk::ImageLayout GetLayout(Type type) {
	switch (type) {
		case Type::VertexBuffer:
		case Type::IndexBuffer:
		case Type::UniformBuffer:
		case Type::StorageBufferRead:
		case Type::StorageBufferWrite:
			return vk::ImageLayout::eUndefined;
		case Type::ShaderWrite:
		case Type::ShaderRead:
			return vk::ImageLayout::eGeneral;
		case Type::SampledRead:
			return vk::ImageLayout::eShaderReadOnlyOptimal;
		case Type::ColorAttachmentWrite:
			return vk::ImageLayout::eColorAttachmentOptimal;
		case Type::DepthStencilRead:
			return vk::ImageLayout::eDepthStencilReadOnlyOptimal;
		case Type::DepthStencilWrite:
			return vk::ImageLayout::eDepthStencilAttachmentOptimal;
		case Type::TransferSrc:
			return vk::ImageLayout::eTransferSrcOptimal;
		case Type::TransferDst:
			return vk::ImageLayout::eTransferDstOptimal;
	}
}
struct ResourceAccess {
	vk::PipelineStageFlags2 stage;
	vk::AccessFlags2 access;
};
static ResourceAccess GetAccess(Type type) {
	switch (type) {
		case Type::SampledRead:
			return {
				.stage = vk::PipelineStageFlagBits2::eFragmentShader,
				.access = vk::AccessFlagBits2::eShaderSampledRead,
			};
		case Type::ShaderRead:
			return {
				.stage = vk::PipelineStageFlagBits2::eComputeShader,
				.access = vk::AccessFlagBits2::eShaderRead,
			};
		case Type::ShaderWrite:
			return {
				.stage = vk::PipelineStageFlagBits2::eComputeShader,
				.access = vk::AccessFlagBits2::eShaderWrite,
			};
		case Type::ColorAttachmentWrite:
			return {
				.stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
				.access = vk::AccessFlagBits2::eColorAttachmentWrite,
			};
		case Type::DepthStencilRead:
		case Type::DepthStencilWrite:
			return {
				.stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests |
				         vk::PipelineStageFlagBits2::eLateFragmentTests,
				.access = vk::AccessFlagBits2::eDepthStencilAttachmentRead |
				          vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			};
		case Type::VertexBuffer:
			return {
				.stage = vk::PipelineStageFlagBits2::eVertexInput,
				.access = vk::AccessFlagBits2::eVertexAttributeRead,
			};
		case Type::IndexBuffer:
			return {
				.stage = vk::PipelineStageFlagBits2::eIndexInput,
				.access = vk::AccessFlagBits2::eIndexRead,
			};
		case Type::UniformBuffer:
			return {
				.stage = vk::PipelineStageFlagBits2::eVertexShader |
				         vk::PipelineStageFlagBits2::eFragmentShader,
				.access = vk::AccessFlagBits2::eUniformRead,
			};
		case Type::StorageBufferWrite:
			return {
				.stage = vk::PipelineStageFlagBits2::eComputeShader,
				.access = vk::AccessFlagBits2::eShaderStorageWrite,
			};
		case Type::StorageBufferRead:
			return {
				.stage = vk::PipelineStageFlagBits2::eComputeShader,
				.access = vk::AccessFlagBits2::eShaderStorageRead,
			};
		case Type::TransferSrc:
			return {
				.stage = vk::PipelineStageFlagBits2::eTransfer,
				.access = vk::AccessFlagBits2::eTransferRead,
			};
		case Type::TransferDst:
			return {
				.stage = vk::PipelineStageFlagBits2::eTransfer,
				.access = vk::AccessFlagBits2::eTransferWrite,
			};
	}
}

};  // namespace ResourceUsage