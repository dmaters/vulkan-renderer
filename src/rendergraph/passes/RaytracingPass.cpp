#include "RaytracingPass.hpp"

#include "Instance.hpp"
#include "rendergraph/BuildContext.hpp"

static const uint32_t GroupSize = 32;
static const uint32_t Alignment = 64;

ResourceManager::BufferDescription RaytracingPass::GetSBTDescription() {
	return {
		.size = GroupSize * 4,
		.usage = vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
	};
}

void RaytracingPass::TraceRays(
	Task::BuildContext& context, MaterialIndex materialIndex, rendergraph::ResourceIndex stb, glm::uvec3 dispatchSize
) {
	if (context.scene.tlas == nullptr) return;
	auto device = Instance::Get().device;
	auto pipeline = context.materialManager.getPipeline(materialIndex);

	auto& buffer = context.getBuffer(stb);

	context.commandBuffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, pipeline.pipeline);

	context.commandBuffer.bindDescriptorSets(
		vk::PipelineBindPoint::eRayTracingKHR,
		pipeline.pipelineLayout,
		1,
		{ context.materialManager.getTextureSet() },
		{}
	);

	Task::BuildContext::Descriptors descriptors = context.getDescriptors();

	if (descriptors.descriptors.size() > 0) {
		context.commandBuffer.pushDescriptorSet(
			vk::PipelineBindPoint::eRayTracingKHR, pipeline.pipelineLayout, 0, descriptors.descriptors
		);
	} else
		context.commandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eRayTracingKHR,
			pipeline.pipelineLayout,
			0,
			{ context.materialManager.getEmptySet() },
			{}
		);

	using GroupHandles = std::array<std::byte, GroupSize * 4>;
	auto handlesData = device.getRayTracingShaderGroupHandleKHR<GroupHandles>(pipeline.pipeline, 0, 4);

	auto groupMappedAddress = (std::byte*)buffer.data;
	for (int i = 0; i < 4; i++) {
		std::memcpy(groupMappedAddress + Alignment * i, handlesData.data() + GroupSize * i, GroupSize);
	}

	auto groupAddress = device.getBufferAddress(vk::BufferDeviceAddressInfo { .buffer = buffer.buffer });

	auto genGroup = vk::StridedDeviceAddressRegionKHR {
		.deviceAddress = groupAddress + 64 * 0,
		.stride = GroupSize,
		.size = GroupSize,
	};
	auto missGroup = vk::StridedDeviceAddressRegionKHR {
		.deviceAddress = groupAddress + 64 * 1,
		.stride = GroupSize,
		.size = GroupSize,
	};
	auto triangleHitGroup = vk::StridedDeviceAddressRegionKHR {
		.deviceAddress = groupAddress + 64 * 2,
		.stride = GroupSize,
		.size = GroupSize,
	};

	auto callableGroup = vk::StridedDeviceAddressRegionKHR {
		.deviceAddress = groupAddress + 64 * 3,
		.stride = GroupSize,
		.size = GroupSize,
	};

	context.commandBuffer.traceRaysKHR(
		genGroup, missGroup, triangleHitGroup, callableGroup, dispatchSize.x, dispatchSize.y, dispatchSize.z
	);
}
