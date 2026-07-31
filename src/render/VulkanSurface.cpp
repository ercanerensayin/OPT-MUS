#include "render/VulkanSurface.hpp"

#include "render/VulkanInstance.hpp"

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>

#include <utility>

namespace optmus::gfx {

VulkanSurface::VulkanSurface(VulkanInstance& instance, GLFWwindow* window)
    : m_instance(instance.handle()) {
    VK_CHECK(glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface));
}

VulkanSurface::~VulkanSurface() {
    destroy();
}

VulkanSurface::VulkanSurface(VulkanSurface&& other) noexcept
    : m_instance(std::exchange(other.m_instance, VK_NULL_HANDLE)),
      m_surface(std::exchange(other.m_surface, VK_NULL_HANDLE)) {}

VulkanSurface& VulkanSurface::operator=(VulkanSurface&& other) noexcept {
    if (this != &other) {
        destroy();
        m_instance = std::exchange(other.m_instance, VK_NULL_HANDLE);
        m_surface = std::exchange(other.m_surface, VK_NULL_HANDLE);
    }
    return *this;
}

void VulkanSurface::destroy() noexcept {
    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
}

}  // namespace optmus::gfx
