#include "GBufferPass.hpp"

#include <string>

#include "../BuildContext.hpp"
#include "../SetupContext.hpp"
#include "DrawPass.hpp"
#include "FrustumCulling.hpp"
#include "RenderPass.hpp"
#include "ui/UI.hpp"

void GBUfferPass::Setup(Task::SetupContext& context) {
	auto& data = context.getData<GBUfferPass>();
	uint32_t indirectBufferSize = static_cast<uint32_t>(
		context.scene.primitives.size() * sizeof(vk::DrawIndexedIndirectCommand)
	);
	uint32_t primitiveMapSize = static_cast<uint32_t>(
		context.scene.primitives.size() * sizeof(uint32_t)
	);

	for (int i = 0; i < 3; i++) {
		auto indirectBuffer = context.createBuffer(
			"indirect_gpass_buffer_local_" + std::to_string(i),
			{
				.size = indirectBufferSize,
				.usage = vk::BufferUsageFlagBits::eTransferSrc,
			},
			ResourceManager::MemoryLocation::HostVisible
		);
		auto primitiveMap = context.createBuffer(
			"primitive_gpass_buffer_local_" + std::to_string(i),
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
		"indirect_gpass_buffer",
		{
			.size = indirectBufferSize,
			.usage = vk::BufferUsageFlagBits::eIndirectBuffer |
	                 vk::BufferUsageFlagBits::eTransferDst,
		},
		ResourceManager::MemoryLocation::HostVisible
	);
	auto primitiveMap = context.createBuffer(
		"primitive_gpass_buffer",
		{
			.size = primitiveMapSize,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer |
	                 vk::BufferUsageFlagBits::eTransferDst,
		},
		ResourceManager::MemoryLocation::HostVisible
	);
	context.registerInput(
		data.sceneUpdatePass, 0, ResourceUsage::Type::UniformBuffer
	);

	context.registerInput(
		data.pbrMaterialData, ResourceUsage::Type::StorageBufferRead
	);
	context.registerInput(
		data.pbrMaterialInstances, ResourceUsage::Type::StorageBufferRead
	);

	context.registerInput(
		indirectBuffer, ResourceUsage::Type::IndirectBufferRead
	);
	context.registerInput(primitiveMap, ResourceUsage::Type::StorageBufferRead);

	auto resolution = context.scene.camera.getResolution();
	auto albedo = context.createImage(
		"gbuffer_albedo",
		{
			.width = static_cast<uint32_t>(resolution.x),
			.height = static_cast<uint32_t>(resolution.y),
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eR16G16B16A16Sfloat,
			.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                 vk::ImageUsageFlagBits::eInputAttachment |
	                 vk::ImageUsageFlagBits::eSampled,

		}
	);
	auto normal = context.createImage(
		"gbuffer_normal",
		{
			.width = static_cast<uint32_t>(resolution.x),
			.height = static_cast<uint32_t>(resolution.y),
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eR16G16B16A16Sfloat,
			.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                 vk::ImageUsageFlagBits::eInputAttachment |
	                 vk::ImageUsageFlagBits::eSampled,

		}

	);
	auto worldPos = context.createImage(
		"gbuffer_worldpos",
		{
			.width = static_cast<uint32_t>(resolution.x),
			.height = static_cast<uint32_t>(resolution.y),
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eR16G16B16A16Sfloat,
			.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                 vk::ImageUsageFlagBits::eInputAttachment |
	                 vk::ImageUsageFlagBits::eSampled,

		}

	);
	auto roughnessMetallic = context.createImage(
		"gbuffer_roughnessMetallic",
		{
			.width = static_cast<uint32_t>(resolution.x),
			.height = static_cast<uint32_t>(resolution.y),
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eR16G16B16A16Sfloat,
			.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                 vk::ImageUsageFlagBits::eInputAttachment |
	                 vk::ImageUsageFlagBits::eSampled,

		}

	);
	auto depth = context.createImage(
		"depth",
		{
			.width = static_cast<uint32_t>(resolution.x),
			.height = static_cast<uint32_t>(resolution.y),
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eD24UnormS8Uint,
			.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,

		}
	);

	context.registerOutput(albedo, ResourceUsage::Type::ColorAttachmentWrite);
	context.registerOutput(normal, ResourceUsage::Type::ColorAttachmentWrite);
	context.registerOutput(worldPos, ResourceUsage::Type::ColorAttachmentWrite);
	context.registerOutput(
		roughnessMetallic, ResourceUsage::Type::ColorAttachmentWrite
	);
	context.registerOutput(depth, ResourceUsage::Type::DepthStencilWrite);
}
void GBUfferPass::Build(Task::BuildContext& context) {
	auto& data = context.getData<GBUfferPass>();

	auto visiblePrimitives = FrustumCulling(
		context.scene,
		context.scene.buckets.at(data.material),
		context.scene.camera.getFrustumPlanes()
	);

	UI::Data.sceneData.gbufferCount = visiblePrimitives.size();

	DrawPass::LoadIndirect(
		context.commandBuffer,
		visiblePrimitives,
		context.scene.primitives,
		context.getBuffer(data._indirectBuffer[context.currentFrame % 3]),
		context.getInput<Buffer&>(3),
		context.getBuffer(data._primitiveMap[context.currentFrame % 3]),
		context.getInput<Buffer&>(4)
	);

	RenderPass::Begin(
		context, AttachmentOp::ClearWrite, AttachmentOp::ClearWrite
	);

	DrawPass::Indirect(context, data.material, visiblePrimitives, 0, 3, 4);

	RenderPass::End(context.commandBuffer);
}
