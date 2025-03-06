#include "Image.hpp"

#include <vulkan/vulkan_core.h>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>
/*
void Image::layoutTransition(
    vk::CommandBuffer& commandBuffer, vk::ImageLayout newLayout
) {
    LayoutTransition(commandBuffer, image, layout, newLayout);
    layout = newLayout;
}
void Image::LayoutTransition(
    vk::CommandBuffer& commandBuffer,
    vk::Image& image,
    vk::ImageLayout oldLayout,
    vk::ImageLayout newLayout
) {
    vk::ImageMemoryBarrier2 layoutBarrier {
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = image,
        .subresourceRange = { .aspectMask = vk::ImageAspectFlagBits::eColor,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1,
                            }
                        };

                        if (oldLayout ==
vk::ImageLayout::eColorAttachmentOptimal) { layoutBarrier.srcStageMask =
                            vk::PipelineStageFlagBits2::eColorAttachmentOutput;
                            layoutBarrier.srcAccessMask =
                            vk::AccessFlagBits2::eColorAttachmentWrite;
                            layoutBarrier.dstStageMask =
vk::PipelineStageFlagBits2::eBottomOfPipe; layoutBarrier.dstAccessMask =
vk::AccessFlagBits2::eNone; } else if (newLayout ==
vk::ImageLayout::eTransferDstOptimal || oldLayout ==
vk::ImageLayout::eTransferDstOptimal) { layoutBarrier.srcStageMask =
vk::PipelineStageFlagBits2::eTopOfPipe; layoutBarrier.srcAccessMask =
vk::AccessFlagBits2::eNone; layoutBarrier.dstStageMask =
vk::PipelineStageFlagBits2::eTransfer; layoutBarrier.dstAccessMask =
vk::AccessFlagBits2::eTransferWrite; } else { layoutBarrier.srcStageMask =
vk::PipelineStageFlagBits2::eTopOfPipe; layoutBarrier.srcAccessMask =
vk::AccessFlagBits2::eNone; layoutBarrier.dstStageMask =
    vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    layoutBarrier.dstAccessMask =
    vk::AccessFlagBits2::eColorAttachmentWrite;
}

vk::DependencyInfo pipelineBarrier {
    .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &layoutBarrier
};

commandBuffer.pipelineBarrier2(pipelineBarrier);
}
*/