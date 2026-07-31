#include "render/Renderer.hpp"

#include "core/Window.hpp"
#include "render/Mesh.hpp"

#include <array>
#include <stdexcept>

namespace optmus::gfx {

Renderer::Renderer(core::Window& window, const std::string& shaderDirectory) : m_window(window) {
    VulkanInstance::Config config;
    config.applicationName = "OPT-MUS";
    config.requiredExtensions = window.requiredInstanceExtensions();
#ifdef NDEBUG
    config.enableValidation = false;
#else
    config.enableValidation = true;
#endif

    m_instance = std::make_unique<VulkanInstance>(config);
    m_surface = std::make_unique<VulkanSurface>(*m_instance, window.handle());
    m_device = std::make_unique<VulkanDevice>(*m_instance, m_surface->handle());
    m_swapchain = std::make_unique<VulkanSwapchain>(*m_device, m_surface->handle(),
                                                    window.framebufferExtent());
    m_pipeline = std::make_unique<VulkanPipeline>(*m_device, m_swapchain->renderPass(),
                                                  shaderDirectory + "/shader.vert.spv",
                                                  shaderDirectory + "/shader.frag.spv");

    createCommandBuffers();
    createSyncObjects();
}

Renderer::~Renderer() {
    // Kaynaklari yok etmeden once GPU'nun isini bitirmesi SART.
    m_device->waitIdle();
    destroySyncObjects();
}

void Renderer::createCommandBuffers() {
    std::array<VkCommandBuffer, kMaxFramesInFlight> buffers{};

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_device->commandPool();
    allocInfo.commandBufferCount = kMaxFramesInFlight;

    VK_CHECK(vkAllocateCommandBuffers(m_device->handle(), &allocInfo, buffers.data()));

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        m_frames[i].commandBuffer = buffers[i];
    }
}

void Renderer::createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // Ilk karede vkWaitForFences sonsuza kadar beklemesin diye sinyalli baslar.
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (FrameResources& frame : m_frames) {
        VK_CHECK(vkCreateSemaphore(m_device->handle(), &semaphoreInfo, nullptr, &frame.imageAvailable));
        VK_CHECK(vkCreateFence(m_device->handle(), &fenceInfo, nullptr, &frame.inFlight));
    }

    m_imagesInFlight.assign(m_swapchain->imageCount(), VK_NULL_HANDLE);
}

void Renderer::destroySyncObjects() noexcept {
    for (FrameResources& frame : m_frames) {
        if (frame.imageAvailable != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device->handle(), frame.imageAvailable, nullptr);
            frame.imageAvailable = VK_NULL_HANDLE;
        }
        if (frame.inFlight != VK_NULL_HANDLE) {
            vkDestroyFence(m_device->handle(), frame.inFlight, nullptr);
            frame.inFlight = VK_NULL_HANDLE;
        }
    }
    m_imagesInFlight.clear();
}

void Renderer::recreateSwapchain() {
    m_window.waitWhileMinimized();
    m_device->waitIdle();

    m_swapchain->recreate(m_window.framebufferExtent());

    // Goruntu sayisi degisebilir; takip dizisini sifirla.
    m_imagesInFlight.assign(m_swapchain->imageCount(), VK_NULL_HANDLE);

    // Not: render pass formati degismedigi surece pipeline'i yeniden derlemeye
    // gerek yok; viewport/scissor zaten dinamik durum.
}

VkCommandBuffer Renderer::beginFrame() {
    if (m_frameStarted) {
        throw std::logic_error("beginFrame, endFrame cagrilmadan tekrar cagrildi.");
    }

    FrameResources& frame = m_frames[m_currentFrame];

    // 1) Bu kare yuvasinin onceki isi bitmis mi? (CPU'nun GPU'yu en fazla
    //    kMaxFramesInFlight kare geride birakmasini saglayan tek nokta.)
    VK_CHECK(vkWaitForFences(m_device->handle(), 1, &frame.inFlight, VK_TRUE, UINT64_MAX));

    // 2) Sunum motorundan bir goruntu iste.
    const VkResult acquireResult =
        m_swapchain->acquireNextImage(frame.imageAvailable, m_currentImageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        // Swapchain artik yuzeyle uyumsuz: yeniden olustur, bu kareyi atla.
        // imageAvailable sinyallenmedigi icin fence'i SIFIRLAMADIK -> kilitlenme yok.
        recreateSwapchain();
        return VK_NULL_HANDLE;
    }
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        VK_CHECK(acquireResult);
    }

    // 3) Ayni goruntu hala baska bir karenin isinde olabilir; onun fence'ini bekle.
    if (m_imagesInFlight[m_currentImageIndex] != VK_NULL_HANDLE) {
        VK_CHECK(vkWaitForFences(m_device->handle(), 1, &m_imagesInFlight[m_currentImageIndex],
                                 VK_TRUE, UINT64_MAX));
    }
    m_imagesInFlight[m_currentImageIndex] = frame.inFlight;

    // 4) Fence'i ancak isi gercekten gonderecegimizden emin olunca sifirla.
    VK_CHECK(vkResetFences(m_device->handle(), 1, &frame.inFlight));

    VK_CHECK(vkResetCommandBuffer(frame.commandBuffer, 0));

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK(vkBeginCommandBuffer(frame.commandBuffer, &beginInfo));

    m_frameStarted = true;
    return frame.commandBuffer;
}

void Renderer::beginRenderPass(VkCommandBuffer commandBuffer, const glm::vec3& clearColor) {
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{clearColor.r, clearColor.g, clearColor.b, 1.0F}};
    clearValues[1].depthStencil = {1.0F, 0};  // Uzak duzlem = 1.0 (Vulkan derinlik araligi [0,1]).

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_swapchain->renderPass();
    renderPassInfo.framebuffer = m_swapchain->framebuffer(m_currentImageIndex);
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapchain->extent();
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    const VkExtent2D extent = m_swapchain->extent();

    VkViewport viewport{};
    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    m_pipeline->bind(commandBuffer);
}

void Renderer::endRenderPass(VkCommandBuffer commandBuffer) {
    vkCmdEndRenderPass(commandBuffer);
}

void Renderer::drawMesh(VkCommandBuffer commandBuffer,
                        const Mesh& mesh,
                        const glm::mat4& viewProjection,
                        const glm::mat4& model) {
    PushConstantData push{};
    push.mvp = viewProjection * model;
    push.model = model;

    m_pipeline->pushConstants(commandBuffer, push);
    mesh.bind(commandBuffer);
    mesh.draw(commandBuffer);
}

void Renderer::endFrame() {
    if (!m_frameStarted) {
        throw std::logic_error("endFrame, beginFrame cagrilmadan cagrildi.");
    }

    FrameResources& frame = m_frames[m_currentFrame];
    VK_CHECK(vkEndCommandBuffer(frame.commandBuffer));

    // Sadece renk ekine yazma asamasi bekler; vertex isleme goruntu hazir olmadan
    // baslayabilir. Tum boru hattini beklemek gereksiz duraksama (stall) yaratirdi.
    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const VkSemaphore signalSemaphore = m_swapchain->renderFinishedSemaphore(m_currentImageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frame.imageAvailable;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &signalSemaphore;

    // Gonderim asenkrondur: bu cagri GPU'yu beklemez, CPU hemen bir sonraki kareye gecer.
    VK_CHECK(vkQueueSubmit(m_device->graphicsQueue(), 1, &submitInfo, frame.inFlight));

    const VkResult presentResult =
        m_swapchain->present(m_currentImageIndex, m_device->presentQueue());

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR ||
        m_window.framebufferResized()) {
        m_window.clearResizedFlag();
        recreateSwapchain();
    } else if (presentResult != VK_SUCCESS) {
        VK_CHECK(presentResult);
    }

    m_frameStarted = false;
    m_currentFrame = (m_currentFrame + 1) % kMaxFramesInFlight;
}

}  // namespace optmus::gfx
