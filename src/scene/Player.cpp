#include "scene/Player.hpp"

#include <algorithm>
#include <cmath>

namespace optmus::scene {

void Player::fixedUpdate(const MovementInput& input, float cameraYaw, float fixedDelta) {
    // Interpolasyonun dogru calismasi icin guncellemeden ONCE durumu sakla.
    m_previous = m_current;

    // Girdi ekseni kamera yonelimine cevrilir: "ileri" = kameranin baktigi yon.
    const glm::vec3 forward{-std::sin(cameraYaw), 0.0F, -std::cos(cameraYaw)};
    const glm::vec3 right{std::cos(cameraYaw), 0.0F, -std::sin(cameraYaw)};

    glm::vec3 wish = forward * input.axis.y + right * input.axis.x;
    const float wishLength = glm::length(wish);
    if (wishLength > 1e-4F) {
        wish /= wishLength;  // Capraz harekette hizlanmayi engelle.
    } else {
        wish = glm::vec3{0.0F};
    }

    const float targetSpeed = input.sprint ? kSprintSpeed : kWalkSpeed;
    const glm::vec3 targetVelocity = wish * targetSpeed;

    // Yatay hiz: hedefe dogru sinirli ivmeyle yaklas, girdi yoksa surtunmeyle yavasla.
    glm::vec3 horizontal{m_velocity.x, 0.0F, m_velocity.z};
    if (wishLength > 1e-4F) {
        const glm::vec3 delta = targetVelocity - horizontal;
        const float maxStep = kAcceleration * fixedDelta;
        const float deltaLength = glm::length(delta);
        horizontal += (deltaLength > maxStep) ? (delta / deltaLength) * maxStep : delta;

        // Govde, hareket yonune yumusakca doner.
        const float desiredYaw = std::atan2(-wish.x, -wish.z);
        const float turnAlpha = 1.0F - std::exp(-kTurnSharpness * fixedDelta);
        m_current.yaw = lerpAngle(m_current.yaw, desiredYaw, turnAlpha);
    } else if (m_grounded) {
        const float damping = std::max(0.0F, 1.0F - kGroundFriction * fixedDelta);
        horizontal *= damping;
    }

    m_velocity.x = horizontal.x;
    m_velocity.z = horizontal.z;

    // Dikey eksen: yercekimi + ziplama.
    if (input.jump && m_grounded) {
        m_velocity.y = kJumpSpeed;
        m_grounded = false;
    }
    m_velocity.y += kGravity * fixedDelta;

    m_current.position += m_velocity * fixedDelta;

    // Yer tutucu zemin carpismasi: y = 0 duzlemi.
    if (m_current.position.y <= 0.0F) {
        m_current.position.y = 0.0F;
        m_velocity.y = 0.0F;
        m_grounded = true;
    }
}

PlayerState Player::interpolate(float alpha) const {
    PlayerState state;
    state.position = lerp(m_previous.position, m_current.position, alpha);
    state.yaw = lerpAngle(m_previous.yaw, m_current.yaw, alpha);
    return state;
}

glm::mat4 Player::modelMatrix(float alpha) const {
    const PlayerState state = interpolate(alpha);

    glm::mat4 model = glm::translate(glm::mat4{1.0F},
                                     state.position + glm::vec3{0.0F, kBodyHeight * 0.5F, 0.0F});
    model = glm::rotate(model, state.yaw, glm::vec3{0.0F, 1.0F, 0.0F});
    return model;
}

}  // namespace optmus::scene
