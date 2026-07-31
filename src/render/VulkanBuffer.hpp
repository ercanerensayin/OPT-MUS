#pragma once

#include "render/VulkanCommon.hpp"

namespace optmus::gfx {

class VulkanDevice;

// Tek bir VkBuffer + arkasindaki VkDeviceMemory icin RAII sarmalayici.
//
// Not: gercek bir oyunda her tampon icin ayri vkAllocateMemory yapilmaz
// (maxMemoryAllocationCount siniri ~4096'dir); ileride VMA benzeri bir
// alt-tahsisci (sub-allocator) devreye alinacak sekilde arayuz sade tutuldu.
class VulkanBuffer {
public:
    VulkanBuffer() = default;
    VulkanBuffer(VulkanDevice& device,
                 VkDeviceSize size,
                 VkBufferUsageFlags usage,
                 VkMemoryPropertyFlags memoryProperties);
    ~VulkanBuffer();

    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;
    VulkanBuffer(VulkanBuffer&& other) noexcept;
    VulkanBuffer& operator=(VulkanBuffer&& other) noexcept;

    // HOST_VISIBLE bellek icin kalici esleme (persistent mapping).
    void map();
    void unmap() noexcept;
    void write(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    [[nodiscard]] VkBuffer handle() const noexcept { return m_buffer; }
    [[nodiscard]] VkDeviceSize size() const noexcept { return m_size; }
    [[nodiscard]] bool valid() const noexcept { return m_buffer != VK_NULL_HANDLE; }

    // GPU-yerel bir tampon olusturur ve veriyi staging uzerinden yukler.
    [[nodiscard]] static VulkanBuffer createDeviceLocal(VulkanDevice& device,
                                                        const void* data,
                                                        VkDeviceSize size,
                                                        VkBufferUsageFlags usage);

private:
    void destroy() noexcept;

    VulkanDevice* m_device = nullptr;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkDeviceSize m_size = 0;
    void* m_mapped = nullptr;
};

}  // namespace optmus::gfx
