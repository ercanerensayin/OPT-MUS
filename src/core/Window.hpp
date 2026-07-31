#pragma once

#include "core/Math.hpp"

#include <string>
#include <vector>

// GLFW'nin vulkan.h'yi kendi basligiyla birlikte dahil etmesini saglar
// (CMake de ayni tanimi verir, bu yuzden korumali).
#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>

namespace optmus::core {

// GLFW baslatma/kapatma ve pencere sahipligi (RAII).
// Vulkan yuzeyini kendisi olusturmaz; yalnizca ham GLFWwindow* saglar,
// boylece render katmani pencere kutuphanesinden bagimsiz kalir.
class Window {
public:
    Window(int width, int height, std::string title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] GLFWwindow* handle() const noexcept { return m_window; }
    [[nodiscard]] bool shouldClose() const noexcept { return glfwWindowShouldClose(m_window) != 0; }
    void requestClose() noexcept { glfwSetWindowShouldClose(m_window, GLFW_TRUE); }

    // Kare basina bir kez cagrilir: olaylari isler ve fare deltasini toplar.
    void pollEvents();

    [[nodiscard]] VkExtent2D framebufferExtent() const;
    // Simge durumuna kucultuldugunde (genislik veya yukseklik 0) render edilemez;
    // pencere geri gelene kadar olay bekleyerek bosa donguden kacinir.
    void waitWhileMinimized();

    [[nodiscard]] bool framebufferResized() const noexcept { return m_framebufferResized; }
    void clearResizedFlag() noexcept { m_framebufferResized = false; }

    [[nodiscard]] bool isKeyDown(int key) const;
    // Son cagridan bu yana biriken fare hareketini dondurur ve sifirlar.
    [[nodiscard]] glm::vec2 consumeMouseDelta() noexcept;
    void setCursorLocked(bool locked);
    [[nodiscard]] bool cursorLocked() const noexcept { return m_cursorLocked; }

    // Instance olustururken gereken pencere sistemi eklentileri (VK_KHR_surface vb.).
    // Donen isaretciler GLFW'ye aittir ve pencere yasadigi surece gecerlidir;
    // bu yuzden bilerek statik degil, uye fonksiyondur.
    [[nodiscard]] std::vector<const char*> requiredInstanceExtensions() const;

private:
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void cursorPositionCallback(GLFWwindow* window, double x, double y);

    GLFWwindow* m_window = nullptr;
    std::string m_title;

    bool m_framebufferResized = false;
    bool m_cursorLocked = true;

    glm::dvec2 m_lastCursor{0.0};
    glm::vec2 m_mouseDelta{0.0F};
    bool m_firstCursorEvent = true;
};

}  // namespace optmus::core
