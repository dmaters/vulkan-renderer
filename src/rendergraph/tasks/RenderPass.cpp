#include "RenderPass.hpp"

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "rendergraph/RenderGraph.hpp"
#include "rendergraph/RenderGraphBuilder.hpp"
#include "scene/Primitive.hpp"

void RenderPass::setup(
	std::unordered_map<std::string_view, ResourceDependency>& images,
	std::unordered_map<std::string_view, ResourceDependency>& buffers
) {
	for (auto& dependency : m_dependencies) {
		switch (dependency.kind) {
			case MaterialManager::ResourceDependency::Kind::RenderTarget:
				images[dependency.name] = {
					.usage = dependency.usage,
					.requiredLayout = dependency.requiredLayout,
				};
				break;
			case MaterialManager::ResourceDependency::Kind::Sampler:
				images[dependency.name] = {
					.usage = dependency.usage,
					.requiredLayout = dependency.requiredLayout,
				};
				break;
			case MaterialManager::ResourceDependency::Kind::Buffer:
				buffers[dependency.name] = {
					.usage = dependency.usage,
				};
				break;
		}
	}
}

std::vector<vk::WriteDescriptorSet> getTransientResources(
	vk::CommandBuffer& commandBuffer,
	const std::vector<MaterialManager::ResourceDependency> dependencies,
	const Resources& resources,
	std::vector<vk::DescriptorImageInfo>& imageInfo,
	std::vector<vk::DescriptorBufferInfo>& bufferInfo

) {
	std::vector<vk::WriteDescriptorSet> descriptors;
	uint32_t bindingCount = 0;

	for (auto& dependency : dependencies) {
		if (dependency.kind ==
		    MaterialManager::ResourceDependency::Kind::Sampler) {
			Image& image =
				resources.resourceManager.getNamedImage(dependency.name);

			imageInfo.push_back({
				.sampler = nullptr,
				.imageView = image.accesses[resources.currentFrame].view,
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			});

			descriptors.push_back(vk::WriteDescriptorSet {
				.dstSet = nullptr,
				.dstBinding = bindingCount,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.pImageInfo = &imageInfo.back(),
			});
		}
		if (dependency.kind ==
		    MaterialManager::ResourceDependency::Kind::Buffer) {
			Buffer& buffer =
				resources.resourceManager.getNamedBuffer(dependency.name);

			bufferInfo.push_back({
				.buffer = buffer.buffer,
				.offset = buffer.bufferAccess[resources.currentFrame].offset,
				.range = buffer.bufferAccess[resources.currentFrame].length,
			});

			descriptors.push_back(vk::WriteDescriptorSet {
				.dstSet = nullptr,
				.dstBinding = bindingCount,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.pBufferInfo = &bufferInfo.back(),
			});
		}
	}
	return descriptors;
}

void setupAttachments(
	vk::CommandBuffer& commandBuffer,
	const std::vector<MaterialManager::ResourceDependency> dependencies,
	const Resources& resources
) {
	std::vector<vk::RenderingAttachmentInfo> attachments;

	std::vector<vk::RenderingAttachmentInfo> colorAttachments;
	std::optional<vk::RenderingAttachmentInfo> depthAttachment;

	uint32_t width = 0, height = 0;

	for (auto& dependency : dependencies) {
		if (dependency.kind !=
		    MaterialManager::ResourceDependency::Kind::RenderTarget)
			continue;

		Image& attachment =
			resources.resourceManager.getNamedImage(dependency.name);

		if (attachment.format == vk::Format::eD16Unorm) {
			depthAttachment = {
				.imageView = attachment.accesses[resources.currentFrame].view,
				.imageLayout =
					attachment.accesses[resources.currentFrame].layout,
				.loadOp = vk::AttachmentLoadOp::eClear,
				.storeOp = vk::AttachmentStoreOp::eStore,
				.clearValue = { .depthStencil = { 1, 1 } },
			};
			width = attachment.size.width;
			height = attachment.size.height;

		} else {
			colorAttachments.push_back({
			.imageView = attachment.accesses[resources.currentFrame].view,
			.imageLayout = attachment.accesses[resources.currentFrame].layout,
	
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = { .color =
								vk::ClearColorValue {
									.float32 =
										std::array<float, 4> {
											0.f, 0.f, 0.f, 0.f }, }, },
		});
		}

		width = attachment.size.width;
		height = attachment.size.height;
	}

	assert(colorAttachments.size() > 0 || !depthAttachment.has_value());

	vk::RenderingInfoKHR renderingInfo {
		.layerCount = 1,
		.viewMask = 0,
		.colorAttachmentCount = (uint32_t)colorAttachments.size(),
		.pColorAttachments = colorAttachments.data(),
		.pDepthAttachment =
			depthAttachment.has_value() ? &depthAttachment.value() : nullptr,
	};
	renderingInfo.renderArea = vk::Rect2D({ 0, 0 }, { width, height }),
	commandBuffer.beginRendering(renderingInfo);
	commandBuffer.setScissor(
		0,
		{
			vk::Rect2D { { 0, 0 }, { width, height } }
    }
	);

	commandBuffer.setViewport(
		0,
		{
			vk::Viewport { 0, 0, (float)width, (float)height, 0, 1 }
    }
	);

	Buffer& vertexBuffer =
		resources.resourceManager.getNamedBuffer("vertex_buffer"

	    );
	Buffer& indexBuffer =
		resources.resourceManager.getNamedBuffer("index_buffer");

	commandBuffer.bindVertexBuffers(0, { vertexBuffer.buffer }, { 0 });
	commandBuffer.bindIndexBuffer(
		indexBuffer.buffer, 0, vk::IndexType::eUint32
	);
}

void RenderPass::execute(
	vk::CommandBuffer& commandBuffer, const Resources& resources
) {
	setupAttachments(commandBuffer, m_dependencies, resources);

	const Material& material =
		resources.materialManager.getMaterial(m_material);

	commandBuffer.bindPipeline(
		vk::PipelineBindPoint::eGraphics, material.pipeline.pipeline
	);

	vk::DescriptorSet globalSet = resources.materialManager.getGlobalSet();

	commandBuffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		material.pipeline.pipelineLayout,
		0,
		{ globalSet, material.materialSet },
		{}
	);

	std::vector<vk::DescriptorImageInfo> imageInfo;
	imageInfo.reserve(m_dependencies.size());
	std::vector<vk::DescriptorBufferInfo> bufferInfo;
	bufferInfo.reserve(m_dependencies.size());
	std::vector<vk::WriteDescriptorSet> transientResources =
		getTransientResources(
			commandBuffer, m_dependencies, resources, imageInfo, bufferInfo
		);

	if (transientResources.size() > 0)
		commandBuffer.pushDescriptorSetKHR(
			vk::PipelineBindPoint::eGraphics,
			material.pipeline.pipelineLayout,
			2,
			transientResources
		);

	for (const Primitive& primitive : resources.primitives) {
		if (primitive.materials[0].index != m_material) continue;

		struct PushConstants {
			glm::mat4 transform;
			uint32_t instanceIndex;
		};
		PushConstants data = { primitive.modelMatrix,
			                   primitive.materials[0].instance };
		commandBuffer.pushConstants(
			material.pipeline.pipelineLayout,
			vk::ShaderStageFlagBits::eVertex |
				vk::ShaderStageFlagBits::eFragment,
			0,
			sizeof(glm::mat4) + sizeof(uint32_t),
			&data
		);

		commandBuffer.drawIndexed(
			primitive.indexCount,
			1,
			primitive.baseIndex,
			primitive.baseVertex,
			0
		);
	}

	commandBuffer.endRendering();
}