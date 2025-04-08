#pragma once
#include <memory>
#include <vector>

#include "Primitive.hpp"
#include "RenderPass.hpp"
#include "Task.hpp"
#include "material/MaterialManager.hpp"

class OpaquePass : public RenderPass {
private:
	bool m_clear;

public:
	OpaquePass(std::shared_ptr<Material> material, bool clear) :
		RenderPass(material), m_clear(clear) {}
	void setup(
		std::vector<ImageDependencyInfo>& requiredImages,
		std::vector<BufferDependencyInfo>& requiredBuffers
	) override;
	void execute(vk::CommandBuffer& buffer, const Resources& resources)
		override;
};
