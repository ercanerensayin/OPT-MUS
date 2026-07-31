#include "render/VulkanSwapchain.hpp"

#include "render/VulkanDevice.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace optmus::gfx {

VulkanSwapchain::VulkanSwapchain(VulkanDevice& device, VkSurfaceKHR surface, VkExtent2D windowExtent)
    : m_device(device), m_surface(surface) {
    createSwapchain(windowExtent, VK_NULL_HANDLE);
    createImageViews();
    createDepthResources();
    createRenderPass();
    createFramebuffers();
    createSemaphores();
}

VulkanSwapchain::~VulkanSwapchain() {
    destroyResources();
    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device.handle(), m_swapchain, nullptr);
    }
}

void VulkanSwapchain::destroyResources() noexcept {
    VkDevice device = m_device.handle();

    for (VkSemaphore semaphore : m_renderFinished) {
        vkDestroySemaphore(device, semaphore, nullptr);
    }
    m_renderFinished.clear();

    for (VkFramebuffer framebuffer : m_framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    m_framebuffers.clear();

    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }

    for (size_t i = 0; i < m_depthImages.size(); ++i) {
        vkDestroyImageView(device, m_depthImageViews[i], nullptr);
        vkDestroyImage(device, m_depthImages[i], nullptr);
        vkFreeMemory(device, m_depthMemory[i], nullptr);
    }
    m_depthImages.clear();
    m_depthImageViews.clear();
    m_depthMemory.clear();

    for (VkImageView view : m_imageViews) {
        vkDestroyImageView(device, view, nullptr);
    }
    m_imageViews.clear();
    m_images.clear();
}

void VulkanSwapchain::recreate(VkExtent2D windowExtent) {
    // Cagiran taraf vkDeviceWaitIdle yapmis olmalidir: kullanimda olan
    // framebuffer/goruntu gorunumlerini yok etmek tanimsiz davranistir.
    VkSwapchainKHR oldSwapchain = m_swapchain;

    destroyResources();

    createSwapchain(windowExtent, oldSwapchain);
    createImageViews();
    createDepthResources();
    createRenderPass();
    createFramebuffers();
    createSemaphores();

    if (oldSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device.handle(), oldSwapchain, nullptr);
    }
}

VkSurfaceFormatKHR VulkanSwapchain::chooseSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& available) {
    // sRGB tercih ediyoruz: shader ciktisi dogrusal uzayda kalir, sunum dogru gamma'ya donusur.
    for (const VkSurfaceFormatKHR& format : available) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    for (const VkSurfaceFormatKHR& format : available) {
        if (format.format == VK_FORMAT_R8G8B8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return available.front();
}

VkPresentModeKHR VulkanSwapchain::choosePresentMode(const std::vector<VkPresentModeKHR>& available) {
    // MAILBOX: uc tamponlama, dusuk gecikme, yirtilma yok -> TPS icin ideal.
    for (VkPresentModeKHR mode : available) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
    // FIFO her uygulamada garanti edilir (v-sync).
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSwapchain::chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                                         VkExtent2D windowExtent) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    VkExtent2D extent = windowExtent;
    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width,
                              capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height, capabilities.minImageExtent.height,
                               capabilities.maxImageExtent.height);
    return extent;
}

void VulkanSwapchain::createSwapchain(VkExtent2D windowExtent, VkSwapchainKHR oldSwapchain) {
    const SwapchainSupportDetails support = m_device.querySwapchainSupport();

    const VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(support.formats);
    const VkPresentModeKHR presentMode = choosePresentMode(support.presentModes);
    const VkExtent2D extent = chooseExtent(support.capabilities, windowExtent);

    // minImageCount + 1: surucu bir sonraki goruntuyu hazirlarken CPU bloklanmasin.
    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
        imageCount = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const QueueFamilyIndices& families = m_device.queueFamilies();
    const std::array<uint32_t, 2> familyIndices{*families.graphics, *families.present};
    if (families.graphics != families.present) {
        // Farkli kuyruklar ayni goruntuye eristigi icin sahiplik devri gerekmeyen mod.
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = familyIndices.data();
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = oldSwapchain;

    VK_CHECK(vkCreateSwapchainKHR(m_device.handle(), &createInfo, nullptr, &m_swapchain));

    uint32_t actualCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(m_device.handle(), m_swapchain, &actualCount, nullptr));
    m_images.resize(actualCount);
    VK_CHECK(vkGetSwapchainImagesKHR(m_device.handle(), m_swapchain, &actualCount, m_images.data()));

    m_imageFormat = surfaceFormat.format;
    m_extent = extent;
}

void VulkanSwapchain::createImageViews() {
    m_imageViews.resize(m_images.size());
    for (size_t i = 0; i < m_images.size(); ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_imageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(m_device.handle(), &viewInfo, nullptr, &m_imageViews[i]));
    }
}

void VulkanSwapchain::createDepthResources() {
    m_depthFormat = m_device.findDepthFormat();

    const size_t count = m_images.size();
    m_depthImages.resize(count);
    m_depthMemory.resize(count);
    m_depthImageViews.resize(count);

    for (size_t i = 0; i < count; ++i) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_extent.width;
        imageInfo.extent.height = m_extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = m_depthFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateImage(m_device.handle(), &imageInfo, nullptr, &m_depthImages[i]));

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(m_device.handle(), m_depthImages[i], &requirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = requirements.size;
        allocInfo.memoryTypeIndex = m_device.findMemoryType(requirements.memoryTypeBits,
                                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VK_CHECK(vkAllocateMemory(m_device.handle(), &allocInfo, nullptr, &m_depthMemory[i]));
        VK_CHECK(vkBindImageMemory(m_device.handle(), m_depthImages[i], m_depthMemory[i], 0));

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_depthImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_depthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(m_device.handle(), &viewInfo, nullptr, &m_depthImageViews[i]));
    }
}

void VulkanSwapchain::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_imageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = m_depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // Kare sonrasi gerekmiyor.
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    // Disaridan gelen bagimlilik.
    //
    // RENK eki icin: imageAvailable semaphore'u COLOR_ATTACHMENT_OUTPUT asamasinda
    // beklendigi icin sunum motoruyla yaris yoktur.
    //
    // DERINLIK eki icin: derinlik goruntusu swapchain'e ait DEGILDIR, dolayisiyla
    // hicbir semaphore onu korumaz. Render pass basindaki duzen gecisi (layout
    // transition) bir YAZMA sayilir ve onceki karenin LATE_FRAGMENT_TESTS
    // asamasindaki derinlik yazmasiyla WRITE_AFTER_WRITE yarisina girer.
    // (Bu, senkronizasyon dogrulamasinin SYNC-HAZARD-WRITE-AFTER-WRITE olarak
    // bildirdigi somut hatadir.) Cozum: kaynak tarafta hem EARLY hem LATE
    // fragment test asamalarini ve derinlik yazma erisimini bildirmek.
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                              VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                              VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    const std::array<VkAttachmentDescription, 2> attachments{colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VK_CHECK(vkCreateRenderPass(m_device.handle(), &renderPassInfo, nullptr, &m_renderPass));
}

void VulkanSwapchain::createFramebuffers() {
    m_framebuffers.resize(m_images.size());
    for (size_t i = 0; i < m_images.size(); ++i) {
        const std::array<VkImageView, 2> attachments{m_imageViews[i], m_depthImageViews[i]};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_extent.width;
        framebufferInfo.height = m_extent.height;
        framebufferInfo.layers = 1;

        VK_CHECK(vkCreateFramebuffer(m_device.handle(), &framebufferInfo, nullptr, &m_framebuffers[i]));
    }
}

void VulkanSwapchain::createSemaphores() {
    m_renderFinished.resize(m_images.size());

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (VkSemaphore& semaphore : m_renderFinished) {
        VK_CHECK(vkCreateSemaphore(m_device.handle(), &semaphoreInfo, nullptr, &semaphore));
    }
}

VkResult VulkanSwapchain::acquireNextImage(VkSemaphore imageAvailable, uint32_t& imageIndex) const {
    // UINT64_MAX zaman asimi: CPU burada bloklanir ama GPU beslemesi bittigi icin
    // bu, kare hizini duzenleyen dogal geri baskidir (frame pacing).
    return vkAcquireNextImageKHR(m_device.handle(), m_swapchain, UINT64_MAX, imageAvailable,
                                 VK_NULL_HANDLE, &imageIndex);
}

VkResult VulkanSwapchain::present(uint32_t imageIndex, VkQueue presentQueue) const {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_renderFinished[imageIndex];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &imageIndex;

    return vkQueuePresentKHR(presentQueue, &presentInfo);
}

}  // namespace optmus::gfx
