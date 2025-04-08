#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <vulkan/vulkan.hpp>

#include "Task.hpp"
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
	std::shared_ptr<Material> m_material;

public:
	RenderPass(std::shared_ptr<Material> material) : m_material(material) {}
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