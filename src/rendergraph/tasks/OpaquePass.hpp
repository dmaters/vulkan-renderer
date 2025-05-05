#pragma once
#include <memory>
#include <vector>

#include "RenderPass.hpp"
#include "Task.hpp"
#include "material/MaterialManager.hpp"
#include "scene/Primitive.hpp"

class OpaquePass : public RenderPass {
private:
	bool m_clear;

public:
	OpaquePass(MaterialManager& materialManager, bool clear) :
		RenderPass(materialManager), m_clear(clear) {}
	void setup(
		std::vector<ImageDependencyInfo>& requiredImages,
		std::vector<BufferDependencyInfo>& requiredBuffers
	) override;
	void execute(vk::CommandBuffer& buffer, const Resources& resources)
		override;
};
