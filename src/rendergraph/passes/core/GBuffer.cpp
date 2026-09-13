#include <string>

#include "../FrustumCulling.hpp"
#include "../RenderPass.hpp"
#include "rendergraph/RenderGraph.hpp"
#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/SetupContext.hpp"
#include "ui/UI.hpp"

struct GBufferPass {
	TaskIndex sceneData;
	MaterialIndex material;
	rendergraph::ResourceIndex pbrMaterialData;
	rendergraph::ResourceIndex primitiveData;

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
	auto& data = context.getData<GBufferPass>();
	uint32_t indirectBufferSize =
		static_cast<uint32_t>(context.scene.primitives.size() * sizeof(vk::DrawIndexedIndirectCommand));

	uint32_t primitiveMapSize = static_cast<uint32_t>(context.scene.primitives.size() * sizeof(uint32_t));

	if (indirectBufferSize == 0) indirectBufferSize = 1;
	if (primitiveMapSize == 0) primitiveMapSize = 1;

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

	auto albedo = context.createImage(
		"gbuffer_albedo",
		{
			.width = static_cast<uint32_t>(context.renderingConfiguration.resolution.x),
			.height = static_cast<uint32_t>(context.renderingConfiguration.resolution.y),
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
			.width = static_cast<uint32_t>(context.renderingConfiguration.resolution.x),
			.height = static_cast<uint32_t>(context.renderingConfiguration.resolution.y),
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
			.width = static_cast<uint32_t>(context.renderingConfiguration.resolution.x),
			.height = static_cast<uint32_t>(context.renderingConfiguration.resolution.y),
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
			.width = static_cast<uint32_t>(context.renderingConfiguration.resolution.x),
			.height = static_cast<uint32_t>(context.renderingConfiguration.resolution.y),
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
			.width = static_cast<uint32_t>(context.renderingConfiguration.resolution.x),
			.height = static_cast<uint32_t>(context.renderingConfiguration.resolution.y),
			.depth = 1,
			.miplevels = 1,
			.format = vk::Format::eD24UnormS8Uint,
			.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransferSrc |
					 vk::ImageUsageFlagBits::eSampled,

		}
	);

	return {
		.inputs = {
			{ cameraBuffer, ResourceUsage::Type::UniformBuffer },
			{ data.pbrMaterialData, ResourceUsage::Type::StorageBufferRead },
			{ data.primitiveData, ResourceUsage::Type::StorageBufferRead },
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
	auto& data = context.getData<GBufferPass>();

	std::vector<PrimitiveIndex> primitives;
	for (int i = 0; i < context.scene.primitives.size(); i++) {
		if (context.scene.materialHints[i] & Scene::MaterialHintBits::Opaque) primitives.push_back(i);
	}

	auto visiblePrimitives = FrustumCulling(
		context.scene,
		primitives,
		context.scene.camera.position,
		context.scene.camera.getFrustumPlanes(context.scene.size)
	);

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

	RenderPass::IndirectDraw(context, data.material, visiblePrimitives, 0, 3);

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
		GBufferPass {
			.sceneData = sceneData,
			.material = context.materialManager.getMaterialIndex("gbuffer"),
			.pbrMaterialData = resources.pbrMaterialData,
			.primitiveData = resources.primitiveData,
		}
	);
}
