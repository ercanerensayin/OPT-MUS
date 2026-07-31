#include "core/Application.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace optmus::core {

Application::Application(const std::string& shaderDirectory)
    : m_window(1600, 900, "OPT-MUS | Vulkan TPS"),
      m_renderer(m_window, shaderDirectory) {
    gfx::VulkanDevice& device = m_renderer.device();

    m_playerMesh = gfx::Mesh::createBox(device, glm::vec3{0.4F, 0.9F, 0.35F},
                                        glm::vec3{0.85F, 0.35F, 0.25F});
    m_groundMesh = gfx::Mesh::createGroundPlane(device, 60.0F, 40);
    m_propMesh = gfx::Mesh::createBox(device, glm::vec3{1.5F, 1.5F, 1.5F},
                                      glm::vec3{0.30F, 0.55F, 0.75F});

    m_camera.setPerspective(glm::radians(60.0F), m_renderer.aspectRatio(), 0.1F, 500.0F);
    m_camera.snapTo(m_player.current().position);

    std::cout << "[opt-mus] Kontroller: WASD hareket, SHIFT kos, SPACE zipla, "
                 "TAB imlec kilidi, ESC cikis\n";
}

void Application::run() {
    using Clock = std::chrono::steady_clock;
    auto previousTime = Clock::now();

    while (!m_window.shouldClose()) {
        // --- 1) Zaman olcumu -------------------------------------------------
        const auto currentTime = Clock::now();
        const std::chrono::duration<double> elapsed = currentTime - previousTime;
        previousTime = currentTime;

        const double frameSeconds = elapsed.count();
        // Kirpma: uzun duraklamalardan sonra fizigin patlamasini engeller.
        const auto deltaSeconds =
            static_cast<float>(std::min(frameSeconds, static_cast<double>(kMaxFrameTime)));

        // --- 2) Girdi (kare basina bir kez) ----------------------------------
        m_window.pollEvents();
        processInput(deltaSeconds);

        // --- 3) Sabit adimli mantik/fizik ------------------------------------
        if (!m_paused) {
            m_accumulator += deltaSeconds;

            uint32_t steps = 0;
            while (m_accumulator >= kFixedDelta && steps < kMaxStepsPerFrame) {
                fixedUpdate(kFixedDelta);
                m_accumulator -= kFixedDelta;
                ++steps;
                ++m_fixedStepsThisSecond;
            }

            // Adim siniri asildiysa kalan borcu sil: yavaslama olur ama donma olmaz.
            if (steps == kMaxStepsPerFrame && m_accumulator > kFixedDelta) {
                m_accumulator = 0.0F;
            }
        }

        // --- 4) Render (degisken hiz + interpolasyon) ------------------------
        const float alpha = m_paused ? 1.0F : (m_accumulator / kFixedDelta);
        render(alpha, deltaSeconds);

        reportPerformance(frameSeconds);
    }

    // Pencere kapanirken GPU hala is yapiyor olabilir; kaynak yikimindan once bekle.
    m_renderer.waitIdle();
}

void Application::processInput(float /*deltaSeconds*/) {
    if (m_window.isKeyDown(GLFW_KEY_ESCAPE)) {
        m_window.requestClose();
    }

    // TAB ile imlec kilidini ac/kapa (kenar tetikleme: basili tutmak tekrar etmesin).
    static bool tabWasDown = false;
    const bool tabDown = m_window.isKeyDown(GLFW_KEY_TAB);
    if (tabDown && !tabWasDown) {
        m_window.setCursorLocked(!m_window.cursorLocked());
    }
    tabWasDown = tabDown;

    static bool pWasDown = false;
    const bool pDown = m_window.isKeyDown(GLFW_KEY_P);
    if (pDown && !pWasDown) {
        m_paused = !m_paused;
        std::cout << "[opt-mus] " << (m_paused ? "Duraklatildi" : "Devam") << '\n';
    }
    pWasDown = pDown;

    // Bakis, mantik adimindan bagimsiz olarak kare basina uygulanir: fare girdisi
    // zaten kareye gore orneklenir, sabit adimda tekrar uygulamak cift sayardi.
    m_camera.addLookInput(m_window.consumeMouseDelta());

    m_input.axis = glm::vec2{0.0F};
    if (m_window.isKeyDown(GLFW_KEY_W)) m_input.axis.y += 1.0F;
    if (m_window.isKeyDown(GLFW_KEY_S)) m_input.axis.y -= 1.0F;
    if (m_window.isKeyDown(GLFW_KEY_D)) m_input.axis.x += 1.0F;
    if (m_window.isKeyDown(GLFW_KEY_A)) m_input.axis.x -= 1.0F;

    m_input.jump = m_window.isKeyDown(GLFW_KEY_SPACE);
    m_input.sprint = m_window.isKeyDown(GLFW_KEY_LEFT_SHIFT);
}

void Application::fixedUpdate(float fixedDelta) {
    m_player.fixedUpdate(m_input, m_camera.yaw(), fixedDelta);

    // Ziplama tek adimda tuketilir; tusa basili tutmak surekli ziplatmasin.
    m_input.jump = false;
}

void Application::render(float alpha, float deltaSeconds) {
    // Kamera, interpolasyonlu oyuncu konumunu takip eder; sabit adima kilitlenmez.
    const scene::PlayerState state = m_player.interpolate(alpha);
    m_camera.setAspectRatio(m_renderer.aspectRatio());
    m_camera.follow(state.position, deltaSeconds);

    VkCommandBuffer commandBuffer = m_renderer.beginFrame();
    if (commandBuffer == VK_NULL_HANDLE) {
        return;  // Swapchain yeniden olusturuldu; bu kare atlanir.
    }

    m_renderer.beginRenderPass(commandBuffer, glm::vec3{0.05F, 0.06F, 0.09F});

    const glm::mat4 viewProjection = m_camera.viewProjection();

    m_renderer.drawMesh(commandBuffer, m_groundMesh, viewProjection, glm::mat4{1.0F});
    m_renderer.drawMesh(commandBuffer, m_playerMesh, viewProjection, m_player.modelMatrix(alpha));

    // Derinlik ve hareket hissini dogrulamak icin birkac sabit engel.
    const glm::vec3 propPositions[] = {
        {10.0F, 1.5F, -6.0F}, {-12.0F, 1.5F, 4.0F}, {4.0F, 1.5F, 16.0F}, {-6.0F, 1.5F, -14.0F},
    };
    for (const glm::vec3& position : propPositions) {
        m_renderer.drawMesh(commandBuffer, m_propMesh, viewProjection,
                            glm::translate(glm::mat4{1.0F}, position));
    }

    m_renderer.endRenderPass(commandBuffer);
    m_renderer.endFrame();
}

void Application::reportPerformance(double frameSeconds) {
    m_fpsTimer += frameSeconds;
    ++m_fpsFrames;

    if (m_fpsTimer >= 1.0) {
        std::cout << "[opt-mus] " << m_fpsFrames << " FPS | " << m_fixedStepsThisSecond
                  << " sabit adim/sn\n";
        m_fpsTimer = 0.0;
        m_fpsFrames = 0;
        m_fixedStepsThisSecond = 0;
    }
}

}  // namespace optmus::core
