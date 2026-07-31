#pragma once

#include "core/Math.hpp"

namespace optmus::scene {

// Sabit adimda guncellenen karakter durumu.
struct PlayerState {
    glm::vec3 position{0.0F, 0.0F, 0.0F};
    float yaw = 0.0F;  // Karakterin baktigi yon (kamera yaw'indan bagimsiz).
};

// Kare basina toplanan ham girdi. Sabit adimli guncellemeye "niyet" olarak gecer.
struct MovementInput {
    glm::vec2 axis{0.0F};   // x: sag(+)/sol(-), y: ileri(+)/geri(-)
    bool jump = false;
    bool sprint = false;
};

// Ucuncu sahis karakter denetleyicisi.
// fixedUpdate SABIT dt ile cagrilir; boylece fizik davranisi kare hizindan
// tamamen bagimsiz olur (determinizm ve ag replikasyonu icin sart).
// Render tarafi interpolate() ile onceki ve guncel durum arasinda ara deger alir.
class Player {
public:
    void fixedUpdate(const MovementInput& input, float cameraYaw, float fixedDelta);

    // alpha = birikmis artik zaman / sabit adim  (0.0 .. 1.0)
    [[nodiscard]] PlayerState interpolate(float alpha) const;
    [[nodiscard]] const PlayerState& current() const noexcept { return m_current; }
    [[nodiscard]] glm::vec3 velocity() const noexcept { return m_velocity; }
    [[nodiscard]] bool grounded() const noexcept { return m_grounded; }

    [[nodiscard]] glm::mat4 modelMatrix(float alpha) const;

private:
    PlayerState m_previous;
    PlayerState m_current;
    glm::vec3 m_velocity{0.0F};
    bool m_grounded = true;

    static constexpr float kWalkSpeed = 4.5F;
    static constexpr float kSprintSpeed = 8.0F;
    static constexpr float kAcceleration = 45.0F;
    static constexpr float kGroundFriction = 12.0F;
    static constexpr float kGravity = -22.0F;   // Oyun hissi icin gercekcilikten hizli.
    static constexpr float kJumpSpeed = 7.5F;
    static constexpr float kTurnSharpness = 14.0F;
    static constexpr float kBodyHeight = 1.8F;
};

}  // namespace optmus::scene
