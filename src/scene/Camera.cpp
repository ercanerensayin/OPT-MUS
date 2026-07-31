#include "scene/Camera.hpp"

#include <algorithm>
#include <cmath>

namespace optmus::scene {

void Camera::setPerspective(float verticalFovRadians,
                            float aspectRatio,
                            float nearPlane,
                            float farPlane) {
    m_verticalFov = verticalFovRadians;
    m_aspectRatio = aspectRatio;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
    updateProjection();
}

void Camera::setAspectRatio(float aspectRatio) {
    if (std::abs(aspectRatio - m_aspectRatio) > 1e-6F) {
        m_aspectRatio = aspectRatio;
        updateProjection();
    }
}

void Camera::updateProjection() {
    // GLM_FORCE_DEPTH_ZERO_TO_ONE tanimli oldugu icin glm::perspective zaten
    // Vulkan'in [0, 1] derinlik araligini uretir.
    m_projection = glm::perspective(m_verticalFov, m_aspectRatio, m_nearPlane, m_farPlane);

    // === Y-FLIP ===
    // GLM, OpenGL kuralina gore +Y'yi ekranda YUKARI kabul eder. Vulkan NDC'sinde
    // +Y ASAGI dogrudur. [1][1] elemanini negatiflemek, klip uzayinda Y'yi ters
    // cevirir; bu olmadan sahne dikeyde aynalanmis gorunur.
    m_projection[1][1] *= -1.0F;
}

void Camera::addLookInput(glm::vec2 mouseDelta) {
    // GLFW'de imlecin +Y'si ekranda asagidir; asagi hareket = asagi bakis.
    m_yaw -= mouseDelta.x * m_sensitivity;
    m_pitch += mouseDelta.y * m_sensitivity;

    m_pitch = std::clamp(m_pitch, kMinPitch, kMaxPitch);

    // Yaw'i [-pi, pi] araliginda tut: uzun oturumlarda float hassasiyeti bozulmasin.
    if (m_yaw > glm::pi<float>()) {
        m_yaw -= glm::two_pi<float>();
    } else if (m_yaw < -glm::pi<float>()) {
        m_yaw += glm::two_pi<float>();
    }
}

void Camera::snapTo(const glm::vec3& targetPosition) {
    m_smoothedTarget = targetPosition;
    m_hasTarget = true;
    updateView();
}

void Camera::follow(const glm::vec3& targetPosition, float deltaSeconds) {
    if (!m_hasTarget) {
        snapTo(targetPosition);
        return;
    }

    // Kare hizindan bagimsiz ustel yumusatma. Basit lerp(a, b, k*dt) yuksek FPS'te
    // farkli, dusuk FPS'te farkli davranir; bu formul her iki durumda ayni his verir.
    const float alpha = 1.0F - std::exp(-m_followSharpness * deltaSeconds);
    m_smoothedTarget = lerp(m_smoothedTarget, targetPosition, alpha);

    updateView();
}

void Camera::updateView() {
    const float cosPitch = std::cos(m_pitch);
    const float sinPitch = std::sin(m_pitch);
    const float sinYaw = std::sin(m_yaw);
    const float cosYaw = std::cos(m_yaw);

    // Kameradan hedefe degil, hedeften kameraya bakan yon (yorunge vektoru).
    const glm::vec3 orbitDirection{cosPitch * sinYaw, sinPitch, cosPitch * cosYaw};

    const glm::vec3 right = rightOnPlane();
    const glm::vec3 up{0.0F, 1.0F, 0.0F};

    // Omuz ustu kaydirma: karakter ekranin ortasini kapatmasin.
    const glm::vec3 pivot =
        m_smoothedTarget + right * m_shoulderOffset.x + up * m_shoulderOffset.y;

    m_position = pivot + orbitDirection * m_distance;
    m_view = glm::lookAt(m_position, pivot, up);
}

glm::vec3 Camera::forwardOnPlane() const noexcept {
    // Yatay duzleme yansitilmis bakis yonu; karakter hareketi bunu referans alir.
    return glm::vec3{-std::sin(m_yaw), 0.0F, -std::cos(m_yaw)};
}

glm::vec3 Camera::rightOnPlane() const noexcept {
    return glm::vec3{std::cos(m_yaw), 0.0F, -std::sin(m_yaw)};
}

}  // namespace optmus::scene
