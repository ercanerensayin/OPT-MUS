#pragma once

#include "render/VulkanCommon.hpp"

#include <vector>

namespace optmus::gfx {

class VulkanDevice;

// Swapchain + goruntu gorunumleri + derinlik eki + render pass + framebuffer'lar.
// Bunlar pencere boyutuna bagli oldugu icin tek bir sinifta toplanir ve
// yeniden boyutlandirmada topluca yeniden olusturulur (recreate).
//
// renderFinished semaphore'lari KARE basina degil GORUNTU (image) basina tutulur:
// vkQueuePresentKHR'in bekledigi semaphore, o goruntu tekrar sunulana kadar
// mesgul kalabilir; kare basina tutmak dogrulama katmaninda gercek bir yaris
// kosulu uyarisi uretir.
class VulkanSwapchain {
public:
    VulkanSwapchain(VulkanDevice& device, VkSurfaceKHR surface, VkExtent2D windowExtent);
    ~VulkanSwapchain();

    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
    VulkanSwapchain(VulkanSwapchain&&) = delete;
    VulkanSwapchain& operator=(VulkanSwapchain&&) = delete;

    // Pencere boyutu degistiginde cagrilir. Cagirmadan once cihaz bosa cikarilmalidir.
    void recreate(VkExtent2D windowExtent);

    // VK_ERROR_OUT_OF_DATE_KHR / VK_SUBOPTIMAL_KHR dogrudan doner; karar cagirana aittir.
    [[nodiscard]] VkResult acquireNextImage(VkSemaphore imageAvailable, uint32_t& imageIndex) const;
    [[nodiscard]] VkResult present(uint32_t imageIndex, VkQueue presentQueue) const;

    [[nodiscard]] VkSwapchainKHR handle() const noexcept { return m_swapchain; }
    [[nodiscard]] VkRenderPass renderPass() const noexcept { return m_renderPass; }
    [[nodiscard]] VkExtent2D extent() const noexcept { return m_extent; }
    [[nodiscard]] VkFormat imageFormat() const noexcept { return m_imageFormat; }
    [[nodiscard]] VkFormat depthFormat() const noexcept { return m_depthFormat; }
    [[nodiscard]] uint32_t imageCount() const noexcept {
        return static_cast<uint32_t>(m_images.size());
    }
    [[nodiscard]] VkFramebuffer framebuffer(uint32_t index) const { return m_framebuffers[index]; }
    [[nodiscard]] VkSemaphore renderFinishedSemaphore(uint32_t imageIndex) const {
        return m_renderFinished[imageIndex];
    }
    [[nodiscard]] float aspectRatio() const noexcept {
        return static_cast<float>(m_extent.width) / static_cast<float>(m_extent.height);
    }

private:
    void createSwapchain(VkExtent2D windowExtent, VkSwapchainKHR oldSwapchain);
    void createImageViews();
    void createDepthResources();
    void createRenderPass();
    void createFramebuffers();
    void createSemaphores();
    void destroyResources() noexcept;

    [[nodiscard]] static VkSurfaceFormatKHR chooseSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& available);
    [[nodiscard]] static VkPresentModeKHR choosePresentMode(
        const std::vector<VkPresentModeKHR>& available);
    [[nodiscard]] static VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                                                 VkExtent2D windowExtent);

    VulkanDevice& m_device;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_imageFormat = VK_FORMAT_UNDEFINED;
    VkFormat m_depthFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent{};

    std::vector<VkImage> m_images;              // Swapchain'e ait, elle yok edilmez.
    std::vector<VkImageView> m_imageViews;
    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<VkSemaphore> m_renderFinished;  // Goruntu basina bir adet.

    std::vector<VkImage> m_depthImages;
    std::vector<VkDeviceMemory> m_depthMemory;
    std::vector<VkImageView> m_depthImageViews;

    VkRenderPass m_renderPass = VK_NULL_HANDLE;
};

}  // namespace optmus::gfx
