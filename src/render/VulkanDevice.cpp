#include "render/VulkanDevice.hpp"

#include "render/VulkanInstance.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>

namespace optmus::gfx {

VulkanDevice::VulkanDevice(VulkanInstance& instance, VkSurfaceKHR surface)
    : m_surface(surface) {
    pickPhysicalDevice(instance.handle());
    createLogicalDevice(instance);
    createCommandPool();
}

VulkanDevice::~VulkanDevice() {
    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    }
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
    }
}

QueueFamilyIndices VulkanDevice::findQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilyIndices indices;

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        if (families[i].queueCount == 0) {
            continue;
        }
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && !indices.graphics.has_value()) {
            indices.graphics = i;
        }

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
        if (presentSupport == VK_TRUE && !indices.present.has_value()) {
            indices.present = i;
        }

        // Grafik ve sunum ayni ailedeyse sunum/paylasim maliyeti dusuk olur; tercih et.
        if (indices.isComplete() && indices.graphics == indices.present) {
            break;
        }
    }

    return indices;
}

bool VulkanDevice::supportsRequiredExtensions(VkPhysicalDevice device) const {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

    return std::all_of(m_deviceExtensions.begin(), m_deviceExtensions.end(), [&](const char* wanted) {
        return std::any_of(available.begin(), available.end(), [&](const VkExtensionProperties& e) {
            return std::strcmp(e.extensionName, wanted) == 0;
        });
    });
}

SwapchainSupportDetails VulkanDevice::querySwapchainSupport(VkPhysicalDevice device) const {
    SwapchainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
    details.formats.resize(formatCount);
    if (formatCount > 0) {
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
    }

    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &modeCount, nullptr);
    details.presentModes.resize(modeCount);
    if (modeCount > 0) {
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &modeCount,
                                                  details.presentModes.data());
    }

    return details;
}

SwapchainSupportDetails VulkanDevice::querySwapchainSupport() const {
    return querySwapchainSupport(m_physicalDevice);
}

uint32_t VulkanDevice::rateDevice(VkPhysicalDevice device) const {
    if (!findQueueFamilies(device).isComplete()) {
        return 0;
    }
    if (!supportsRequiredExtensions(device)) {
        return 0;
    }
    if (!querySwapchainSupport(device).isAdequate()) {
        return 0;
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(device, &props);

    VkPhysicalDeviceFeatures features{};
    vkGetPhysicalDeviceFeatures(device, &features);

    uint32_t score = 1;
    // Harici GPU her zaman tercih edilir; TPS oyununda fill-rate belirleyicidir.
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 10000;
    } else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        score += 1000;
    }
    score += props.limits.maxImageDimension2D / 16;
    if (features.samplerAnisotropy == VK_TRUE) {
        score += 100;
    }
    return score;
}

void VulkanDevice::pickPhysicalDevice(VkInstance instance) {
    uint32_t count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &count, nullptr));
    if (count == 0) {
        throw std::runtime_error("Vulkan destekleyen bir GPU bulunamadi.");
    }

    std::vector<VkPhysicalDevice> devices(count);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &count, devices.data()));

    uint32_t bestScore = 0;
    for (VkPhysicalDevice device : devices) {
        const uint32_t score = rateDevice(device);
        if (score > bestScore) {
            bestScore = score;
            m_physicalDevice = device;
        }
    }

    if (m_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Gereksinimleri karsilayan bir GPU bulunamadi "
                                 "(swapchain / kuyruk ailesi / yuzey destegi eksik).");
    }

    vkGetPhysicalDeviceProperties(m_physicalDevice, &m_properties);
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_memoryProperties);
    m_queueFamilies = findQueueFamilies(m_physicalDevice);

    std::cout << "[vulkan] Secilen GPU: " << m_properties.deviceName << '\n';
}

void VulkanDevice::createLogicalDevice(const VulkanInstance& instance) {
    // Grafik ve sunum ailesi ayni olabilir; set ile tekrarlari eliyoruz.
    const std::set<uint32_t> uniqueFamilies{*m_queueFamilies.graphics, *m_queueFamilies.present};

    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    queueInfos.reserve(uniqueFamilies.size());
    const float queuePriority = 1.0F;
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        queueInfos.push_back(queueInfo);
    }

    VkPhysicalDeviceFeatures supported{};
    vkGetPhysicalDeviceFeatures(m_physicalDevice, &supported);

    VkPhysicalDeviceFeatures enabled{};
    enabled.samplerAnisotropy = supported.samplerAnisotropy;
    enabled.fillModeNonSolid = supported.fillModeNonSolid;  // wireframe hata ayiklama modu icin

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    createInfo.pQueueCreateInfos = queueInfos.data();
    createInfo.pEnabledFeatures = &enabled;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = m_deviceExtensions.data();
    // Modern Vulkan'da cihaz katmanlari yok sayilir, eski surucular icin yine de veriyoruz.
    createInfo.enabledLayerCount = static_cast<uint32_t>(instance.enabledLayers().size());
    createInfo.ppEnabledLayerNames =
        instance.enabledLayers().empty() ? nullptr : instance.enabledLayers().data();

    VK_CHECK(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));

    vkGetDeviceQueue(m_device, *m_queueFamilies.graphics, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, *m_queueFamilies.present, 0, &m_presentQueue);
}

void VulkanDevice::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = *m_queueFamilies.graphics;
    // Her karede komut tamponunu bastan kaydettigimiz icin tekil reset sarttir.
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
                     VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    VK_CHECK(vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool));
}

uint32_t VulkanDevice::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; ++i) {
        const bool typeAllowed = (typeFilter & (1U << i)) != 0;
        const bool hasProperties =
            (m_memoryProperties.memoryTypes[i].propertyFlags & properties) == properties;
        if (typeAllowed && hasProperties) {
            return i;
        }
    }
    throw std::runtime_error("Istenen ozelliklere sahip bellek tipi bulunamadi.");
}

VkFormat VulkanDevice::findSupportedFormat(const std::vector<VkFormat>& candidates,
                                           VkImageTiling tiling,
                                           VkFormatFeatureFlags features) const {
    for (VkFormat format : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);

        const VkFormatFeatureFlags available =
            (tiling == VK_IMAGE_TILING_LINEAR) ? props.linearTilingFeatures : props.optimalTilingFeatures;
        if ((available & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("Desteklenen bir format bulunamadi.");
}

VkFormat VulkanDevice::findDepthFormat() const {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkCommandBuffer VulkanDevice::beginSingleTimeCommands() const {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer));

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    return commandBuffer;
}

void VulkanDevice::endSingleTimeCommands(VkCommandBuffer commandBuffer) const {
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // Yalnizca yukleme (staging) yolunda kullanildigi icin burada beklemek kabul edilebilir;
    // oyun dongusunun sicak yolunda asla vkQueueWaitIdle cagrilmaz.
    VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(m_graphicsQueue));

    vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
}

void VulkanDevice::waitIdle() const {
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
    }
}

}  // namespace optmus::gfx
