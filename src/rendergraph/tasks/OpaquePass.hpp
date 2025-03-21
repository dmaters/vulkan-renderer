#pragma once
#include <vector>

#include "Primitive.hpp"
#include "RenderPass.hpp"
#include "Task.hpp"
#include "material/MaterialManager.hpp"

class OpaquePass : public RenderPass {
private:
public:
	OpaquePass(MaterialManager::Material material) : RenderPass(material) {}
	void setup(RenderGraphResourceSolver& graph) override;
	void execute(vk::CommandBuffer& buffer, const Resources& resources)
		override;
};
