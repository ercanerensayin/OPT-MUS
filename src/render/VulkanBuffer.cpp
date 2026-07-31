#include "render/VulkanBuffer.hpp"

#include "render/VulkanDevice.hpp"

#include <cstring>
#include <stdexcept>
#include <utility>

namespace optmus::gfx {

VulkanBuffer::VulkanBuffer(VulkanDevice& device,
                           VkDeviceSize size,
                           VkBufferUsageFlags usage,
                           VkMemoryPropertyFlags memoryProperties)
    : m_device(&device), m_size(size) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(m_device->handle(), &bufferInfo, nullptr, &m_buffer));

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(m_device->handle(), m_buffer, &requirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex =
        m_device->findMemoryType(requirements.memoryTypeBits, memoryProperties);

    VK_CHECK(vkAllocateMemory(m_device->handle(), &allocInfo, nullptr, &m_memory));
    VK_CHECK(vkBindBufferMemory(m_device->handle(), m_buffer, m_memory, 0));
}

VulkanBuffer::~VulkanBuffer() {
    destroy();
}

VulkanBuffer::VulkanBuffer(VulkanBuffer&& other) noexcept
    : m_device(std::exchange(other.m_device, nullptr)),
      m_buffer(std::exchange(other.m_buffer, VK_NULL_HANDLE)),
      m_memory(std::exchange(other.m_memory, VK_NULL_HANDLE)),
      m_size(std::exchange(other.m_size, 0)),
      m_mapped(std::exchange(other.m_mapped, nullptr)) {}

VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& other) noexcept {
    if (this != &other) {
        destroy();
        m_device = std::exchange(other.m_device, nullptr);
        m_buffer = std::exchange(other.m_buffer, VK_NULL_HANDLE);
        m_memory = std::exchange(other.m_memory, VK_NULL_HANDLE);
        m_size = std::exchange(other.m_size, 0);
        m_mapped = std::exchange(other.m_mapped, nullptr);
    }
    return *this;
}

void VulkanBuffer::destroy() noexcept {
    if (m_device == nullptr) {
        return;
    }
    unmap();
    if (m_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device->handle(), m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
    }
    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device->handle(), m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
    m_device = nullptr;
    m_size = 0;
}

void VulkanBuffer::map() {
    if (m_mapped == nullptr) {
        VK_CHECK(vkMapMemory(m_device->handle(), m_memory, 0, m_size, 0, &m_mapped));
    }
}

void VulkanBuffer::unmap() noexcept {
    if (m_mapped != nullptr) {
        vkUnmapMemory(m_device->handle(), m_memory);
        m_mapped = nullptr;
    }
}

void VulkanBuffer::write(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (offset + size > m_size) {
        throw std::runtime_error("VulkanBuffer::write tampon sinirlarini asiyor.");
    }
    map();
    std::memcpy(static_cast<char*>(m_mapped) + offset, data, static_cast<size_t>(size));
    // HOST_COHERENT bellek kullandigimiz icin vkFlushMappedMemoryRanges gerekmez.
}

VulkanBuffer VulkanBuffer::createDeviceLocal(VulkanDevice& device,
                                             const void* data,
                                             VkDeviceSize size,
                                             VkBufferUsageFlags usage) {
    VulkanBuffer staging(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    staging.write(data, size);
    staging.unmap();

    VulkanBuffer result(device, size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkCommandBuffer cmd = device.beginSingleTimeCommands();
    VkBufferCopy region{};
    region.size = size;
    vkCmdCopyBuffer(cmd, staging.handle(), result.handle(), 1, &region);
    device.endSingleTimeCommands(cmd);

    return result;
}

}  // namespace optmus::gfx
