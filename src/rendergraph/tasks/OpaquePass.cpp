#include "OpaquePass.hpp"

#include <unordered_set>
#include <vector>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "../RenderGraph.hpp"
#include "RenderPass.hpp"
#include "material/Material.hpp"
#include "scene/Primitive.hpp"

void OpaquePass::setup(
	std::vector<ImageDependencyInfo>& requiredImages,
	std::vector<BufferDependencyInfo>& requiredBuffers
) {
	requiredImages.push_back({
		.name = "main_color",
		.usage =
		{
			.type = ResourceUsage::Type::WRITE,
			.access = vk::AccessFlagBits2::eColorAttachmentWrite,
			.stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		},
		.requiredLayout = vk::ImageLayout::eColorAttachmentOptimal,
		
	});
	requiredImages.push_back({
		.name = "main_depth",
		.usage =
		{
			.type = ResourceUsage::Type::WRITE,
			.access = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			.stage = vk::PipelineStageFlagBits2::eLateFragmentTests,
		},
		.requiredLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
	});

	RenderPass::setAttachments({
		.color = Attachment { "main_color", m_clear },
		.depth = Attachment { "main_depth", m_clear },
	});

	requiredBuffers.push_back({
		.name = "gset_buffer",
		.usage =  
		{
			.type = ResourceUsage::Type::READ,
			.access = vk::AccessFlagBits2::eShaderRead,
			.stage = vk::PipelineStageFlagBits2::eVertexShader,
		},
	});
	requiredBuffers.push_back({
		.name = "light_buffer",
		.usage =  
		{
			.type = ResourceUsage::Type::READ,
			.access = vk::AccessFlagBits2::eShaderRead,
			.stage = vk::PipelineStageFlagBits2::eFragmentShader,
		},
	});
}

void OpaquePass::execute(
	vk::CommandBuffer& commandBuffer, const Resources& resources
) {
	RenderPass::execute(commandBuffer, resources);
	std::unordered_map<MaterialIndex, std::vector<uint32_t>> materials;

	for (int i = 0; i < resources.primitives.size(); i++) {
		materials[resources.primitives[i].material.index].push_back(i);
	}

	for (auto& [materialIndex, primitives] : materials) {
		Material& material = m_materialManager.getMaterial(materialIndex);
		commandBuffer.bindPipeline(
			vk::PipelineBindPoint::eGraphics, material.pipeline.pipeline
		);

		commandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			material.pipeline.pipelineLayout,
			0,
			{ material.globalSet, material.materialSet },
			{}
		);

		for (auto primitive : resources.primitives) {
			commandBuffer.pushConstants(
				material.pipeline.pipelineLayout,
				vk::ShaderStageFlagBits::eVertex,
				0,
				64,
				&primitive.modelMatrix
			);
			if (!material.instanceSets.empty()) {
				commandBuffer.bindDescriptorSets(
					vk::PipelineBindPoint::eGraphics,
					material.pipeline.pipelineLayout,
					2,
					{ material.instanceSets[primitive.material.instance] },
					nullptr
				);
			}

			commandBuffer.drawIndexed(
				primitive.indexCount,
				1,
				primitive.baseIndex,
				primitive.baseVertex,
				0
			);
		}
	}
	commandBuffer.endRendering();
}