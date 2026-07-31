#include "core/Window.hpp"

#include <stdexcept>
#include <utility>

namespace optmus::core {
namespace {

int g_glfwRefCount = 0;

void initGlfwOnce() {
    if (g_glfwRefCount++ == 0) {
        if (glfwInit() != GLFW_TRUE) {
            g_glfwRefCount = 0;
            throw std::runtime_error("GLFW baslatilamadi.");
        }
    }
}

void terminateGlfwOnce() noexcept {
    if (--g_glfwRefCount == 0) {
        glfwTerminate();
    }
}

}  // namespace

Window::Window(int width, int height, std::string title) : m_title(std::move(title)) {
    initGlfwOnce();

    if (glfwVulkanSupported() != GLFW_TRUE) {
        terminateGlfwOnce();
        throw std::runtime_error("Bu sistemde Vulkan yukleyicisi (loader) bulunamadi.");
    }

    // OpenGL baglami OLUSTURULMAZ: pencere tamamen Vulkan tarafindan surulur.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(width, height, m_title.c_str(), nullptr, nullptr);
    if (m_window == nullptr) {
        terminateGlfwOnce();
        throw std::runtime_error("GLFW penceresi olusturulamadi.");
    }

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
    glfwSetCursorPosCallback(m_window, cursorPositionCallback);

    setCursorLocked(true);
    if (glfwRawMouseMotionSupported() == GLFW_TRUE) {
        // Isletim sistemi ivmelendirmesi olmadan ham girdi: nisan almada kritik.
        glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
}

Window::~Window() {
    if (m_window != nullptr) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    terminateGlfwOnce();
}

void Window::framebufferResizeCallback(GLFWwindow* window, int /*width*/, int /*height*/) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self != nullptr) {
        self->m_framebufferResized = true;
    }
}

void Window::cursorPositionCallback(GLFWwindow* window, double x, double y) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self == nullptr) {
        return;
    }

    if (self->m_firstCursorEvent) {
        // Ilk olayda dev bir delta olusmasini engelle (kamera aniden savrulmasin).
        self->m_lastCursor = {x, y};
        self->m_firstCursorEvent = false;
        return;
    }

    const glm::dvec2 current{x, y};
    const glm::dvec2 delta = current - self->m_lastCursor;
    self->m_lastCursor = current;

    if (self->m_cursorLocked) {
        self->m_mouseDelta += glm::vec2(delta);
    }
}

void Window::pollEvents() {
    m_mouseDelta = glm::vec2(0.0F);
    glfwPollEvents();
}

VkExtent2D Window::framebufferExtent() const {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    return VkExtent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
}

void Window::waitWhileMinimized() {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(m_window, &width, &height);
        glfwWaitEvents();
    }
}

bool Window::isKeyDown(int key) const {
    return glfwGetKey(m_window, key) == GLFW_PRESS;
}

glm::vec2 Window::consumeMouseDelta() noexcept {
    const glm::vec2 delta = m_mouseDelta;
    m_mouseDelta = glm::vec2(0.0F);
    return delta;
}

void Window::setCursorLocked(bool locked) {
    m_cursorLocked = locked;
    glfwSetInputMode(m_window, GLFW_CURSOR, locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    // Kilit degisiminde imlec isinlanir; bir sonraki olayin deltasini yok say.
    m_firstCursorEvent = true;
}

std::vector<const char*> Window::requiredInstanceExtensions() const {
    uint32_t count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    if (extensions == nullptr) {
        throw std::runtime_error("GLFW gerekli Vulkan eklentilerini bildiremedi.");
    }
    return std::vector<const char*>(extensions, extensions + count);
}

}  // namespace optmus::core
