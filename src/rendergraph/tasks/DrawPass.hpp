#pragma once

#include "../BuildContext.hpp"

struct DrawPass {
	static void Quad(Task::BuildContext& context, MaterialIndex materialIndex);
	static void Indirect(
		Task::BuildContext& context,
		MaterialIndex material,
		std::span<uint32_t> primitives,
		uint32_t pushConstant,
		rendergraph::ResourceIndex indirectBufferLocalIndex,
		rendergraph::ResourceIndex primitiveBufferLocalIndex
	);
};
