#include "ShadowPass.hpp"

#include <string>

#include "../BuildContext.hpp"
#include "../SetupContext.hpp"
#include "DrawPass.hpp"
#include "RenderPass.hpp"

static const int32_t CASCADE_SIZE = 2048;

void ShadowPass::Setup(Task::SetupContext& context) {
	auto& data = context.getData<ShadowPass>();
	uint32_t indirectBufferSize = static_cast<uint32_t>(
		context.scene.primitives.size() *
		sizeof(vk::DrawIndexedIndirectCommand) * 3
	);

	uint32_t primitiveMapSize = static_cast<uint32_t>(
		context.scene.primitives.size() * sizeof(uint32_t) * 3
	);

	for (int i = 0; i < 3; i++) {
		auto indirectBuffer = context.createBuffer(
			"indirect_shadowpass_buffer_local_" + std::to_string(i),
			{
				.size = indirectBufferSize,
				.usage = vk::BufferUsageFlagBits::eTransferSrc,
			},
			ResourceManager::MemoryLocation::HostVisible
		);
		auto primitiveMap = context.createBuffer(
			"primitive_shadowpass_buffer_local_" + std::to_string(i),
			{
				.size = primitiveMapSize,
				.usage = vk::BufferUsageFlagBits::eTransferSrc,
			},
			ResourceManager::MemoryLocation::HostVisible
		);

		data._indirectBuffer[i] = indirectBuffer;
		data._primitiveMap[i] = primitiveMap;
	}

	auto indirectBuffer = context.createBuffer(
		"indirect_shadowmap_buffer",
		{
			.size = indirectBufferSize,
			.usage = vk::BufferUsageFlagBits::eIndirectBuffer |
	                 vk::BufferUsageFlagBits::eTransferDst,
		},
		ResourceManager::MemoryLocation::HostVisible
	);
	auto primitiveMap = context.createBuffer(
		"primitive_shadowpass_buffer",
		{
			.size = primitiveMapSize,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer |
	                 vk::BufferUsageFlagBits::eTransferDst,
		},
		ResourceManager::MemoryLocation::HostVisible

	);

	context.registerInput(
		data.sceneUpdateTask, 1, ResourceUsage::Type::UniformBuffer
	);
	context.registerInput(
		data.sceneUpdateTask, 0, ResourceUsage::Type::UniformBuffer
	);

	context.registerInput(
		indirectBuffer, ResourceUsage::Type::IndirectBufferRead
	);
	context.registerInput(primitiveMap, ResourceUsage::Type::StorageBufferRead);

	rendergraph::ResourceIndex shadowAtlas = context.createImage(
		"shadow_atlas",
		{
			.width = 6144,
			.height = 2048,
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eD16Unorm,
			.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment |
	                 vk::ImageUsageFlagBits::eSampled,
		}
	);
	context.registerOutput(shadowAtlas, ResourceUsage::Type::DepthStencilWrite);
}

void ShadowPass::Build(Task::BuildContext& context) {
	auto data = context.getData<ShadowPass>();

	DrawPass::LoadIndirect(
		context.commandBuffer,
		context.scene.buckets.at(data.material),
		context.scene.primitives,
		context.getBuffer(data._indirectBuffer[context.currentFrame % 3]),
		context.getInput<Buffer&>(2),
		context.getBuffer(data._primitiveMap[context.currentFrame % 3]),
		context.getInput<Buffer&>(3)
	);

	for (int i = 0; i < 3; i++) {
		RenderPass::Begin(
			context,
			AttachmentOp::Read,
			AttachmentOp::ClearWrite,
			vk::Rect2D {
				{ CASCADE_SIZE * i, 0            },
				{ CASCADE_SIZE,     CASCADE_SIZE },
        }
		);

		DrawPass::Indirect(
			context,
			data.material,
			context.scene.buckets.at(data.material),
			i,
			2,
			3
		);

		RenderPass::End(context.commandBuffer);
	}
}
