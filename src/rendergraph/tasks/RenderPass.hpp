#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <vulkan/vulkan.hpp>

#include "Task.hpp"
#include "material/Material.hpp"
#include "material/MaterialManager.hpp"

class RenderPass : public Task {
public:
	struct Attachment {
		std::string_view name;
		bool clear;
	};
	struct Attachments {
		std::optional<Attachment> color;
		std::optional<Attachment> depth;
	};

private:
	Attachments m_attachments;

protected:
	MaterialManager& m_materialManager;

public:
	RenderPass(MaterialManager& materialManager) :
		m_materialManager(materialManager) {}
	inline void setAttachments(Attachments attachments) {
		m_attachments = attachments;
	}
	void setup(
		std::vector<ImageDependencyInfo>& requiredImages,
		std::vector<BufferDependencyInfo>& requiredBuffers
	) override;
	void execute(vk::CommandBuffer& commandBuffer, const Resources& resources)
		override;
};