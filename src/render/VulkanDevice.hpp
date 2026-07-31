#pragma once

#include "render/VulkanCommon.hpp"

#include <optional>
#include <string>
#include <vector>

namespace optmus::gfx {

class VulkanInstance;

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;

    [[nodiscard]] bool isComplete() const noexcept {
        return graphics.has_value() && present.has_value();
    }
};

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;

    [[nodiscard]] bool isAdequate() const noexcept {
        return !formats.empty() && !presentModes.empty();
    }
};

// Fiziksel cihaz secimi + mantiksal cihaz ve kuyruklar.
// Ayrica butun alt sistemlerin ihtiyac duydugu yardimcilar (bellek tipi bulma,
// derinlik formati secimi, tek seferlik komut tamponlari) burada toplanir.
class VulkanDevice {
public:
    VulkanDevice(VulkanInstance& instance, VkSurfaceKHR surface);
    ~VulkanDevice();

    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;
    VulkanDevice(VulkanDevice&&) = delete;
    VulkanDevice& operator=(VulkanDevice&&) = delete;

    [[nodiscard]] VkDevice handle() const noexcept { return m_device; }
    [[nodiscard]] VkPhysicalDevice physicalDevice() const noexcept { return m_physicalDevice; }
    [[nodiscard]] VkQueue graphicsQueue() const noexcept { return m_graphicsQueue; }
    [[nodiscard]] VkQueue presentQueue() const noexcept { return m_presentQueue; }
    [[nodiscard]] VkCommandPool commandPool() const noexcept { return m_commandPool; }
    [[nodiscard]] const QueueFamilyIndices& queueFamilies() const noexcept { return m_queueFamilies; }
    [[nodiscard]] const VkPhysicalDeviceProperties& properties() const noexcept { return m_properties; }
    [[nodiscard]] std::string deviceName() const { return m_properties.deviceName; }

    // Swapchain yeniden olusturulurken guncel yuzey yeteneklerine ihtiyac duyulur.
    [[nodiscard]] SwapchainSupportDetails querySwapchainSupport() const;
    [[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    [[nodiscard]] VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
                                               VkImageTiling tiling,
                                               VkFormatFeatureFlags features) const;
    [[nodiscard]] VkFormat findDepthFormat() const;

    // Staging kopyalari gibi kisa isler icin.
    [[nodiscard]] VkCommandBuffer beginSingleTimeCommands() const;
    void endSingleTimeCommands(VkCommandBuffer commandBuffer) const;

    void waitIdle() const;

private:
    void pickPhysicalDevice(VkInstance instance);
    void createLogicalDevice(const VulkanInstance& instance);
    void createCommandPool();

    [[nodiscard]] uint32_t rateDevice(VkPhysicalDevice device) const;
    [[nodiscard]] QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    [[nodiscard]] bool supportsRequiredExtensions(VkPhysicalDevice device) const;
    [[nodiscard]] SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device) const;

    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;  // Instance ile yok olur, elle silinmez.
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;

    QueueFamilyIndices m_queueFamilies;
    VkPhysicalDeviceProperties m_properties{};
    VkPhysicalDeviceMemoryProperties m_memoryProperties{};

    std::vector<const char*> m_deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
};

}  // namespace optmus::gfx
