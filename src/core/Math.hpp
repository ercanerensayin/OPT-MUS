#pragma once
//
// Tum GLM kullanimi bu basliktan gecmelidir.
// Vulkan'a ozgu iki kritik ayar burada, GLM dahil edilmeden ONCE tanimlanir:
//
//   GLM_FORCE_DEPTH_ZERO_TO_ONE : OpenGL derinlik araligi [-1, 1] iken Vulkan [0, 1]
//                                 kullanir. Bu makro olmadan glm::perspective yanlis
//                                 bir projeksiyon uretir ve z-fighting / kirpilma olur.
//   GLM_FORCE_RADIANS           : Aci birimlerini sabitler (GLM 0.9.6+ zaten varsayilan).
//
// Y ekseni ters cevirme (Y-flip) matris seviyesinde Camera icinde yapilir,
// bkz. scene/Camera.cpp.
//
#ifndef GLM_FORCE_RADIANS
#define GLM_FORCE_RADIANS
#endif
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

namespace optmus {

// Sabit zaman adimli guncelleme ile degisken render hizi arasindaki
// gorsel titremeyi (stutter) engellemek icin kullanilan interpolasyon.
[[nodiscard]] inline glm::vec3 lerp(const glm::vec3& a, const glm::vec3& b, float t) noexcept {
    return a + (b - a) * t;
}

[[nodiscard]] inline float lerp(float a, float b, float t) noexcept {
    return a + (b - a) * t;
}

// Aci interpolasyonunda -pi/pi sinirindan atlamayi onler.
[[nodiscard]] inline float lerpAngle(float a, float b, float t) noexcept {
    float diff = b - a;
    while (diff > glm::pi<float>()) diff -= glm::two_pi<float>();
    while (diff < -glm::pi<float>()) diff += glm::two_pi<float>();
    return a + diff * t;
}

}  // namespace optmus
