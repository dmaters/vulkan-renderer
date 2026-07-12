#include <string>

#include "../RenderPass.hpp"
#include "rendergraph/BuildContext.hpp"
#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/SetupContext.hpp"

static const int32_t CASCADE_SIZE = 2048;

struct ShadowPass {
	std::array<rendergraph::ResourceIndex, 3> _indirectBuffer;
	std::array<rendergraph::ResourceIndex, 3> _primitiveMap;

	TaskIndex sceneData;

	MaterialIndex material;
};

static Task::Dependencies setup(Task::SetupContext& context) {
	auto& data = context.getData<ShadowPass>();
	uint32_t indirectBufferSize =
		static_cast<uint32_t>(context.scene.primitives.size() * sizeof(vk::DrawIndexedIndirectCommand) * 3);

	uint32_t primitiveMapSize = static_cast<uint32_t>(context.scene.primitives.size() * sizeof(uint32_t) * 3);

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
			.usage = vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst,
		},
		ResourceManager::MemoryLocation::HostVisible
	);
	auto primitiveMap = context.createBuffer(
		"primitive_shadowpass_buffer",
		{
			.size = primitiveMapSize,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
		},
		ResourceManager::MemoryLocation::HostVisible

	);

	auto lightBuffer = context.getReference(data.sceneData, rendergraph::passes::core::SceneDataSlots::Lights);
	auto cameraBuffer = context.getReference(data.sceneData, rendergraph::passes::core::SceneDataSlots::Camera);

	rendergraph::ResourceIndex shadowAtlas = context.createImage(
		"shadow_atlas",
		{
			.width = 6144,
			.height = 2048,
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eD16Unorm,
			.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
		}
	);

	return {
		.inputs ={
			{ lightBuffer, ResourceUsage::Type::UniformBuffer },
			{ cameraBuffer, ResourceUsage::Type::UniformBuffer },
			{ indirectBuffer, ResourceUsage::Type::IndirectBufferRead },
			{ primitiveMap, ResourceUsage::Type::StorageBufferRead },
		 },
		.outputs = {
			{ shadowAtlas, ResourceUsage::Type::DepthStencilWrite },
		 },
	};
}

static void build(Task::BuildContext& context) {
	auto data = context.getData<ShadowPass>();

	RenderPass::LoadIndirect(
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
				{ CASCADE_SIZE * i, 0			  },
				{ CASCADE_SIZE,		CASCADE_SIZE },
		}
		);

		RenderPass::IndirectDraw(context, data.material, context.scene.buckets.at(data.material), i, 2, 3);

		RenderPass::End(context.commandBuffer);
	}
}
TaskIndex rendergraph::passes::core::shadows(PassBuildContext& context, TaskIndex sceneData) {
	return context.renderGraph.addTask(
		"shadow_map",
		{
			.setup = setup,
			.build = build,

		},
		ShadowPass {
			.sceneData = sceneData,
			.material = context.materialManager.getMaterialIndex("shadowmap"),
		}
	);
}
