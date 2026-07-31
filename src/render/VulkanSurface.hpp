#pragma once

#include "render/VulkanCommon.hpp"

struct GLFWwindow;

namespace optmus::gfx {

class VulkanInstance;

// VkSurfaceKHR sahipligi. Yikim sirasi onemlidir: surface, instance'tan ONCE
// yok edilmelidir. Bu yuzden Renderer icinde uye siralamasi
// (instance -> surface -> device -> swapchain) korunur; C++ uyeleri ters
// sirada yok ettigi icin dogru sira ucretsiz gelir.
class VulkanSurface {
public:
    VulkanSurface(VulkanInstance& instance, GLFWwindow* window);
    ~VulkanSurface();

    VulkanSurface(const VulkanSurface&) = delete;
    VulkanSurface& operator=(const VulkanSurface&) = delete;
    VulkanSurface(VulkanSurface&& other) noexcept;
    VulkanSurface& operator=(VulkanSurface&& other) noexcept;

    [[nodiscard]] VkSurfaceKHR handle() const noexcept { return m_surface; }

private:
    void destroy() noexcept;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
};

}  // namespace optmus::gfx
