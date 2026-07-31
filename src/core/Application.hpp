#pragma once

#include "core/Window.hpp"
#include "render/Mesh.hpp"
#include "render/Renderer.hpp"
#include "scene/Camera.hpp"
#include "scene/Player.hpp"

#include <chrono>
#include <string>

namespace optmus::core {

// Oyunun kompozisyon koku ve ana dongusu.
//
// SABIT ZAMAN ADIMI (FIXED TIMESTEP)
// ----------------------------------
// Mantik/fizik her zaman kFixedDelta (1/60 sn) araliklarla ilerler; render ise
// ekranin verdigi hizda calisir. Aradaki artik zaman "accumulator"da birikir ve
// alpha = accumulator / kFixedDelta orani ile onceki-guncel durum arasinda
// interpolasyon yapilir. Boylece:
//   * Fizik kare hizindan bagimsiz ve deterministik olur (144 Hz ile 60 Hz ayni sonuc),
//   * Render, mantik hizindan hizli olsa bile goruntu akici kalir.
//
// "Olum sarmali" (spiral of death) korumasi: cok uzun bir kare olustugunda
// (hata ayiklayicida durma, pencere tasima) birikimi kMaxFrameTime ile kirpiyoruz;
// aksi halde her kare daha fazla fizik adimi gerektirir ve oyun kilitlenir.
class Application {
public:
    explicit Application(const std::string& shaderDirectory);

    void run();

private:
    void processInput(float deltaSeconds);
    void fixedUpdate(float fixedDelta);
    void render(float alpha, float deltaSeconds);
    void reportPerformance(double frameSeconds);

    static constexpr float kFixedDelta = 1.0F / 60.0F;
    static constexpr float kMaxFrameTime = 0.25F;
    static constexpr uint32_t kMaxStepsPerFrame = 8;

    Window m_window;
    gfx::Renderer m_renderer;

    gfx::Mesh m_playerMesh;
    gfx::Mesh m_groundMesh;
    gfx::Mesh m_propMesh;

    scene::Camera m_camera;
    scene::Player m_player;
    scene::MovementInput m_input;

    float m_accumulator = 0.0F;
    bool m_paused = false;

    // Basit kare hizi olcumu.
    double m_fpsTimer = 0.0;
    uint32_t m_fpsFrames = 0;
    uint32_t m_fixedStepsThisSecond = 0;
};

}  // namespace optmus::core
