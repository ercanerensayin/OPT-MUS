#pragma once

#include "core/Math.hpp"
#include "render/VulkanCommon.hpp"

#include <string>
#include <vector>

namespace optmus::gfx {

class VulkanDevice;

// Push constant blogu: 2 x mat4 = 128 bayt.
// 128 bayt, Vulkan spesifikasyonunun garanti ettigi minimum maxPushConstantsSize
// degeridir; bu yuzden descriptor set'e gerek kalmadan her cihazda calisir.
struct PushConstantData {
    glm::mat4 mvp{1.0F};    // projection * view * model  (Y-flip projeksiyonun icinde)
    glm::mat4 model{1.0F};  // normalleri dunya uzayina tasimak icin
};
static_assert(sizeof(PushConstantData) == 128, "Push constant blogu 128 bayti asmamalidir.");

// Grafik pipeline'i + pipeline layout. Viewport ve scissor DINAMIK durumdur:
// pencere yeniden boyutlandiginda pipeline'i yeniden derlemek gerekmez.
class VulkanPipeline {
public:
    VulkanPipeline(VulkanDevice& device,
                   VkRenderPass renderPass,
                   const std::string& vertexShaderPath,
                   const std::string& fragmentShaderPath);
    ~VulkanPipeline();

    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;
    VulkanPipeline(VulkanPipeline&&) = delete;
    VulkanPipeline& operator=(VulkanPipeline&&) = delete;

    void bind(VkCommandBuffer commandBuffer) const;
    void pushConstants(VkCommandBuffer commandBuffer, const PushConstantData& data) const;

    [[nodiscard]] VkPipeline handle() const noexcept { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout layout() const noexcept { return m_layout; }

private:
    [[nodiscard]] static std::vector<char> readFile(const std::string& path);
    [[nodiscard]] VkShaderModule createShaderModule(const std::vector<char>& code) const;

    VulkanDevice& m_device;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

}  // namespace optmus::gfx
