#pragma once

#include "../BuildContext.hpp"

struct DrawPass {
	static void Quad(Task::BuildContext& context, MaterialIndex materialIndex);
	static void LoadIndirect(
		vk::CommandBuffer& commandBuffer,
		const std::vector<uint32_t>& primitivesIndices,
		const std::vector<Primitive>& primitives,
		Buffer& indirectBufferLocal,
		Buffer& indirectBufferDevice,
		Buffer& primitiveMapLocal,
		Buffer& primitiveMapDevice
	);
	static void Indirect(
		Task::BuildContext& context,
		MaterialIndex material,
		const std::vector<uint32_t>& primitives,
		uint32_t pushConstant,
		std::size_t indirectSlot,
		std::size_t primitiveMapSlot
	);
};
