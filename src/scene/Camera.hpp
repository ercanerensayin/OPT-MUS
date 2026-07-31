#pragma once

#include "core/Math.hpp"

namespace optmus::scene {

// Ucuncu sahis (TPS) omuz ustu yorunge kamerasi.
//
// VULKAN NDC NOTU
// ---------------
// Vulkan'da NDC'nin +Y ekseni EKRANDA ASAGI dogrudur (OpenGL'de yukari).
// Ayrica derinlik araligi [0, 1]'dir ([-1, 1] degil).
//   * Derinlik : GLM_FORCE_DEPTH_ZERO_TO_ONE (core/Math.hpp) ile cozulur.
//   * Y ekseni : projeksiyon matrisinin [1][1] elemani -1 ile carpilir (Y-flip).
// Bu sayede oyun mantiginda "+Y yukari" saglam sol/sag el karisikligi olmadan
// kullanilabilir. Y-flip sarim yonunu (winding) ters cevirdigi icin pipeline'da
// on yuz VK_FRONT_FACE_CLOCKWISE olarak ayarlanmistir.
class Camera {
public:
    void setPerspective(float verticalFovRadians, float aspectRatio, float nearPlane, float farPlane);
    void setAspectRatio(float aspectRatio);

    // Fare girdisi: kare basina bir kez, degisken zaman adiminda uygulanir.
    void addLookInput(glm::vec2 mouseDelta);
    // Hedefi (oyuncuyu) yumusak takip. dt sabit adim degil, render adimidir.
    void follow(const glm::vec3& targetPosition, float deltaSeconds);
    // Ilk karede kamerayi hedefe isinla (yumusatma olmadan).
    void snapTo(const glm::vec3& targetPosition);

    [[nodiscard]] const glm::mat4& projection() const noexcept { return m_projection; }
    [[nodiscard]] const glm::mat4& view() const noexcept { return m_view; }
    [[nodiscard]] glm::mat4 viewProjection() const noexcept { return m_projection * m_view; }
    [[nodiscard]] glm::vec3 position() const noexcept { return m_position; }

    // Karakter hareketi kamera yonelimine gore hesaplanir (TPS standardi).
    [[nodiscard]] glm::vec3 forwardOnPlane() const noexcept;
    [[nodiscard]] glm::vec3 rightOnPlane() const noexcept;
    [[nodiscard]] float yaw() const noexcept { return m_yaw; }

    void setDistance(float distance) noexcept { m_distance = distance; }
    void setSensitivity(float radiansPerPixel) noexcept { m_sensitivity = radiansPerPixel; }

private:
    void updateProjection();
    void updateView();

    glm::mat4 m_projection{1.0F};
    glm::mat4 m_view{1.0F};

    glm::vec3 m_position{0.0F, 2.0F, 5.0F};
    glm::vec3 m_smoothedTarget{0.0F};
    bool m_hasTarget = false;

    float m_yaw = 0.0F;  // Y ekseni etrafinda, radyan.
    // Pozitif pitch kamerayi pivotun USTUNE tasir, yani asagi baktirir.
    // Varsayilan: karakteri hafif yukaridan goren omuz ustu acisi.
    float m_pitch = glm::radians(14.0F);
    float m_distance = 6.0F;                  // Omuz mesafesi.
    glm::vec3 m_shoulderOffset{0.7F, 1.7F, 0.0F};  // Sag omuz + goz hizasi.

    float m_sensitivity = 0.0022F;            // Piksel basina radyan.
    float m_followSharpness = 12.0F;          // Buyuk deger = daha sert takip.

    float m_verticalFov = glm::radians(60.0F);
    float m_aspectRatio = 16.0F / 9.0F;
    float m_nearPlane = 0.1F;
    float m_farPlane = 500.0F;

    static constexpr float kMinPitch = -0.85F;  // ~ -49 derece: yukari bakis siniri
    static constexpr float kMaxPitch = 1.30F;   // ~ +74 derece: neredeyse tepeden bakis
};

}  // namespace optmus::scene
