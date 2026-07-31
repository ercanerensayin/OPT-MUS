#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>

namespace optmus::gfx {

// Vulkan cagrilarindan donen hatalari tasiyan istisna turu.
class VulkanError : public std::runtime_error {
public:
    VulkanError(VkResult result, std::string message)
        : std::runtime_error(std::move(message)), m_result(result) {}

    [[nodiscard]] VkResult result() const noexcept { return m_result; }

private:
    VkResult m_result;
};

[[nodiscard]] const char* toString(VkResult result) noexcept;

// C++20 std::source_location sayesinde makro icinde __FILE__/__LINE__ tasimaya gerek kalmaz.
void checkResult(VkResult result,
                 std::string_view expression,
                 const std::source_location& location = std::source_location::current());

// Kullanim: VK_CHECK(vkCreateInstance(&info, nullptr, &instance));
#define VK_CHECK(expr) ::optmus::gfx::checkResult((expr), #expr)

}  // namespace optmus::gfx
