#include "render/VulkanInstance.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <utility>

namespace optmus::gfx {
namespace {

constexpr const char* kValidationLayerName = "VK_LAYER_KHRONOS_validation";

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* /*userData*/) {
    // Info/verbose seviyesini yutuyoruz; oyun dongusunde konsolu bogmasin.
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "[vulkan] " << (data->pMessage ? data->pMessage : "") << '\n';
    }
    return VK_FALSE;  // VK_TRUE dondurmek cagriyi iptal ettirir, istemiyoruz.
}

VkDebugUtilsMessengerCreateInfoEXT makeDebugMessengerInfo() {
    VkDebugUtilsMessengerCreateInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = debugCallback;
    return info;
}

}  // namespace

VulkanInstance::VulkanInstance(const Config& config) {
    createInstance(config);
    if (m_validationEnabled) {
        createDebugMessenger();
    }
}

VulkanInstance::~VulkanInstance() {
    destroy();
}

VulkanInstance::VulkanInstance(VulkanInstance&& other) noexcept
    : m_instance(std::exchange(other.m_instance, VK_NULL_HANDLE)),
      m_debugMessenger(std::exchange(other.m_debugMessenger, VK_NULL_HANDLE)),
      m_layers(std::move(other.m_layers)),
      m_validationEnabled(std::exchange(other.m_validationEnabled, false)) {}

VulkanInstance& VulkanInstance::operator=(VulkanInstance&& other) noexcept {
    if (this != &other) {
        destroy();
        m_instance = std::exchange(other.m_instance, VK_NULL_HANDLE);
        m_debugMessenger = std::exchange(other.m_debugMessenger, VK_NULL_HANDLE);
        m_layers = std::move(other.m_layers);
        m_validationEnabled = std::exchange(other.m_validationEnabled, false);
    }
    return *this;
}

void VulkanInstance::destroy() noexcept {
    if (m_debugMessenger != VK_NULL_HANDLE) {
        auto destroyFn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroyFn != nullptr) {
            destroyFn(m_instance, m_debugMessenger, nullptr);
        }
        m_debugMessenger = VK_NULL_HANDLE;
    }
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

bool VulkanInstance::areLayersSupported(const std::vector<const char*>& layers) {
    uint32_t count = 0;
    if (vkEnumerateInstanceLayerProperties(&count, nullptr) != VK_SUCCESS) {
        return false;
    }
    std::vector<VkLayerProperties> available(count);
    if (vkEnumerateInstanceLayerProperties(&count, available.data()) != VK_SUCCESS) {
        return false;
    }

    return std::all_of(layers.begin(), layers.end(), [&](const char* wanted) {
        return std::any_of(available.begin(), available.end(), [&](const VkLayerProperties& props) {
            return std::strcmp(props.layerName, wanted) == 0;
        });
    });
}

bool VulkanInstance::isExtensionSupported(const char* extension) {
    uint32_t count = 0;
    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) != VK_SUCCESS) {
        return false;
    }
    std::vector<VkExtensionProperties> available(count);
    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, available.data()) != VK_SUCCESS) {
        return false;
    }
    return std::any_of(available.begin(), available.end(), [&](const VkExtensionProperties& props) {
        return std::strcmp(props.extensionName, extension) == 0;
    });
}

void VulkanInstance::createInstance(const Config& config) {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = config.applicationName.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = config.engineName.c_str();
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = config.apiVersion;

    std::vector<const char*> extensions = config.requiredExtensions;

    m_validationEnabled = false;
    if (config.enableValidation) {
        m_layers = {kValidationLayerName};
        if (areLayersSupported(m_layers) && isExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            m_validationEnabled = true;
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        } else {
            std::cerr << "[vulkan] Validation layer bulunamadi, dogrulama olmadan devam ediliyor.\n";
            m_layers.clear();
        }
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(m_layers.size());
    createInfo.ppEnabledLayerNames = m_layers.empty() ? nullptr : m_layers.data();

    // Instance olusturma/yok etme asamasindaki hatalarin da yakalanmasi icin
    // messenger bilgisini pNext zincirine takiyoruz.
    VkDebugUtilsMessengerCreateInfoEXT debugInfo = makeDebugMessengerInfo();
    if (m_validationEnabled) {
        createInfo.pNext = &debugInfo;
    }

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_instance));
}

void VulkanInstance::createDebugMessenger() {
    auto createFn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
    if (createFn == nullptr) {
        m_validationEnabled = false;
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT info = makeDebugMessengerInfo();
    VK_CHECK(createFn(m_instance, &info, nullptr, &m_debugMessenger));
}

}  // namespace optmus::gfx
