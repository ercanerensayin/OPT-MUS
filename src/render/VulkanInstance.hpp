#pragma once

#include "render/VulkanCommon.hpp"

#include <string>
#include <vector>

namespace optmus::gfx {

// VkInstance + (istege bagli) validation layer / debug messenger sahipligi.
// RAII: nesne yasarsa handle gecerlidir, yikiciyla birlikte yok edilir.
// Kopyalanamaz, tasinabilir.
class VulkanInstance {
public:
    struct Config {
        std::string applicationName = "OPT-MUS";
        std::string engineName = "OPT-MUS Engine";
        uint32_t apiVersion = VK_API_VERSION_1_2;
        // Pencere katmanindan (GLFW) gelen zorunlu eklentiler.
        std::vector<const char*> requiredExtensions;
        // Debug build'de varsayilan olarak acilir; layer yoksa sessizce kapatilir.
        bool enableValidation = true;
    };

    explicit VulkanInstance(const Config& config);
    ~VulkanInstance();

    VulkanInstance(const VulkanInstance&) = delete;
    VulkanInstance& operator=(const VulkanInstance&) = delete;
    VulkanInstance(VulkanInstance&& other) noexcept;
    VulkanInstance& operator=(VulkanInstance&& other) noexcept;

    [[nodiscard]] VkInstance handle() const noexcept { return m_instance; }
    [[nodiscard]] bool validationEnabled() const noexcept { return m_validationEnabled; }
    [[nodiscard]] const std::vector<const char*>& enabledLayers() const noexcept { return m_layers; }

private:
    void createInstance(const Config& config);
    void createDebugMessenger();
    void destroy() noexcept;

    static bool areLayersSupported(const std::vector<const char*>& layers);
    static bool isExtensionSupported(const char* extension);

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    std::vector<const char*> m_layers;
    bool m_validationEnabled = false;
};

}  // namespace optmus::gfx
