#pragma once

#include <array>

#include "../ResourceIndex.hpp"
#include "../Task.hpp"

struct ShadowPass {
	std::array<rendergraph::ResourceIndex, 3> indirectBuffers;
	std::array<rendergraph::ResourceIndex, 3> indirectBuffersAlpha;

	static void Setup(Task::SetupContext&);
	static void Build(Task::BuildContext&);
};
