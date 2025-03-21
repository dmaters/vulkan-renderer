#pragma once

#include <optional>
#include <vulkan/vulkan.hpp>

#include "Task.hpp"
#include "material/MaterialManager.hpp"
#include "rendergraph/RenderGraphResourceSolver.hpp"

class RenderPass : public Task {
public:
	struct Attachments {
		std::optional<std::string_view> color;
		std::optional<std::string_view> depth;
	};

private:
	Attachments m_attachments;

protected:
	MaterialManager::Material m_material;

public:
	RenderPass(MaterialManager::Material material) : m_material(material) {}
	inline void setAttachments(Attachments attachments) {
		m_attachments = attachments;
	}
	void setup(RenderGraphResourceSolver& solver) override;
	void execute(vk::CommandBuffer& commandBuffer, const Resources& resources)
		override;
};