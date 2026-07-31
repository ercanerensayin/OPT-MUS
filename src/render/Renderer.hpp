#pragma once

#include "core/Math.hpp"
#include "render/VulkanDevice.hpp"
#include "render/VulkanInstance.hpp"
#include "render/VulkanPipeline.hpp"
#include "render/VulkanSurface.hpp"
#include "render/VulkanSwapchain.hpp"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace optmus::core {
class Window;
}

namespace optmus::gfx {

class Mesh;

// Render alt sistemlerinin kompozisyon koku ve kare senkronizasyonunun sahibi.
//
// SENKRONIZASYON MODELI
// ---------------------
// * kMaxFramesInFlight (2) kadar kare ayni anda "ucusta" olabilir. CPU, GPU'yu
//   beklemeden bir sonraki karenin komutlarini kaydeder.
// * m_frames[i].inFlight (VkFence)      : CPU <- GPU. i numarali karenin komut
//   tamponu tekrar kullanilmadan once GPU'nun onu bitirdigini garanti eder.
// * m_frames[i].imageAvailable (Semaphore) : GPU <- Sunum motoru. Swapchain
//   goruntusu hazir olmadan renk ekine yazilmaz.
// * swapchain.renderFinishedSemaphore(imageIndex) : GPU -> Sunum. Cizim bitmeden
//   sunum yapilmaz. Goruntu basina tutulur (bkz. VulkanSwapchain).
// * m_imagesInFlight                    : Ayni swapchain goruntusune iki karenin
//   ust uste yazmasini engeller (goruntu sayisi ile kare sayisi esit olmayabilir).
//
// Kullanim:
//   if (VkCommandBuffer cmd = renderer.beginFrame()) {
//       renderer.beginRenderPass(cmd);
//       renderer.drawMesh(cmd, mesh, mvp, model);
//       renderer.endRenderPass(cmd);
//       renderer.endFrame();
//   }
class Renderer {
public:
    static constexpr uint32_t kMaxFramesInFlight = 2;

    Renderer(core::Window& window, const std::string& shaderDirectory);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    // Kare atlanirsa (yeniden boyutlandirma, simge durumu) VK_NULL_HANDLE doner.
    [[nodiscard]] VkCommandBuffer beginFrame();
    void endFrame();

    void beginRenderPass(VkCommandBuffer commandBuffer, const glm::vec3& clearColor);
    void endRenderPass(VkCommandBuffer commandBuffer);
    void drawMesh(VkCommandBuffer commandBuffer,
                  const Mesh& mesh,
                  const glm::mat4& viewProjection,
                  const glm::mat4& model);

    [[nodiscard]] VulkanDevice& device() noexcept { return *m_device; }
    [[nodiscard]] float aspectRatio() const noexcept { return m_swapchain->aspectRatio(); }
    [[nodiscard]] bool frameInProgress() const noexcept { return m_frameStarted; }
    void waitIdle() const { m_device->waitIdle(); }

private:
    struct FrameResources {
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;  // Havuzla birlikte serbest kalir.
        VkSemaphore imageAvailable = VK_NULL_HANDLE;
        VkFence inFlight = VK_NULL_HANDLE;
    };

    void createCommandBuffers();
    void createSyncObjects();
    void destroySyncObjects() noexcept;
    void recreateSwapchain();

    core::Window& m_window;

    // Uye siralamasi = olusturma sirasi; yikim ters sirada oldugu icin dogru.
    std::unique_ptr<VulkanInstance> m_instance;
    std::unique_ptr<VulkanSurface> m_surface;
    std::unique_ptr<VulkanDevice> m_device;
    std::unique_ptr<VulkanSwapchain> m_swapchain;
    std::unique_ptr<VulkanPipeline> m_pipeline;

    std::array<FrameResources, kMaxFramesInFlight> m_frames{};
    std::vector<VkFence> m_imagesInFlight;  // Sahiplenmez, m_frames fence'lerine isaret eder.

    uint32_t m_currentImageIndex = 0;
    uint32_t m_currentFrame = 0;
    bool m_frameStarted = false;
};

}  // namespace optmus::gfx
