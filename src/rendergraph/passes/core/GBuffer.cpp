#include <string>

#include "../FrustumCulling.hpp"
#include "../RenderPass.hpp"
#include "rendergraph/RenderGraph.hpp"
#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/SetupContext.hpp"
#include "ui/UI.hpp"

struct GBUfferPass {
	TaskIndex sceneData;
	MaterialIndex material;
	rendergraph::ResourceIndex pbrMaterialData;
	rendergraph::ResourceIndex pbrMaterialInstances;

	std::array<rendergraph::ResourceIndex, 3> _indirectBuffer;
	std::array<rendergraph::ResourceIndex, 3> _primitiveMap;

	std::array<rendergraph::ResourceIndex, 3> _alphaIndirectBuffer;
	std::array<rendergraph::ResourceIndex, 3> _alphaPrimitiveMap;

	enum Slot {
		Albedo,
		Normal,
		WorldPos,
		RoughnessMetallic,
		Depth
	};
};

static Task::Dependencies setup(Task::SetupContext& context) {
	auto& data = context.getData<GBUfferPass>();
	uint32_t indirectBufferSize =
		static_cast<uint32_t>(context.scene.primitives.size() * sizeof(vk::DrawIndexedIndirectCommand));

	uint32_t primitiveMapSize = static_cast<uint32_t>(context.scene.primitives.size() * sizeof(uint32_t));

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
			.usage = vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst,
		},
		ResourceManager::MemoryLocation::HostVisible
	);
	auto primitiveMap = context.createBuffer(
		"primitive_gpass_buffer",
		{
			.size = primitiveMapSize,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
		},
		ResourceManager::MemoryLocation::HostVisible
	);
	auto cameraBuffer = context.getReference(data.sceneData, rendergraph::passes::core::SceneDataSlots::Camera);

	auto resolution = context.scene.camera.getResolution();
	auto albedo = context.createImage(
		"gbuffer_albedo",
		{
			.width = static_cast<uint32_t>(resolution.x),
			.height = static_cast<uint32_t>(resolution.y),
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eR16G16B16A16Sfloat,
			.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment |
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
			.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment |
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
			.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment |
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
			.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment |
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

	return {
		.inputs = {
			{ cameraBuffer, ResourceUsage::Type::UniformBuffer },
			{ data.pbrMaterialData, ResourceUsage::Type::StorageBufferRead },
			{ data.pbrMaterialInstances, ResourceUsage::Type::StorageBufferRead },
			{ indirectBuffer, ResourceUsage::Type::IndirectBufferRead },
			{ primitiveMap, ResourceUsage::Type::StorageBufferRead },
		 },
		.outputs = {
			{ albedo, ResourceUsage::Type::ColorAttachmentWrite },
			{ normal, ResourceUsage::Type::ColorAttachmentWrite },
			{ worldPos, ResourceUsage::Type::ColorAttachmentWrite },
			{ roughnessMetallic, ResourceUsage::Type::ColorAttachmentWrite },
			{ depth, ResourceUsage::Type::DepthStencilWrite },
		 },
	};
}
static void build(Task::BuildContext& context) {
	auto& data = context.getData<GBUfferPass>();

	auto visiblePrimitives =
		FrustumCulling(context.scene, context.scene.buckets.at(data.material), context.scene.camera.getFrustumPlanes());

	UI::Data.sceneData.gbufferCount = visiblePrimitives.size();

	RenderPass::LoadIndirect(
		context.commandBuffer,
		visiblePrimitives,
		context.scene.primitives,
		context.getBuffer(data._indirectBuffer[context.currentFrame % 3]),
		context.getInput<Buffer&>(3),
		context.getBuffer(data._primitiveMap[context.currentFrame % 3]),
		context.getInput<Buffer&>(4)
	);

	RenderPass::Begin(context, AttachmentOp::ClearWrite, AttachmentOp::ClearWrite);

	RenderPass::IndirectDraw(context, data.material, visiblePrimitives, 0, 3, 4);

	RenderPass::End(context.commandBuffer);
}

TaskIndex rendergraph::passes::core::gbuffer(
	PassBuildContext& context, const ExternalResources& resources, TaskIndex sceneData
) {
	return context.renderGraph.addTask(
		"gbuffer",
		{
			.setup = setup,
			.build = build,
		},
		GBUfferPass {
			.sceneData = sceneData,
			.material = context.materialManager.getMaterialIndex("gbuffer"),
			.pbrMaterialData = resources.pbrMaterialData,
			.pbrMaterialInstances = resources.pbrMaterialInstances,
		}
	);
}
