#pragma once

#include "core/Math.hpp"
#include "render/VulkanBuffer.hpp"

#include <array>
#include <vector>

namespace optmus::gfx {

class VulkanDevice;

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;

    [[nodiscard]] static VkVertexInputBindingDescription bindingDescription();
    [[nodiscard]] static std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions();
};

// Kose ve indeks tamponlarini tutan basit statik mesh.
class Mesh {
public:
    Mesh() = default;
    Mesh(VulkanDevice& device,
         const std::vector<Vertex>& vertices,
         const std::vector<uint32_t>& indices);

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&) noexcept = default;
    Mesh& operator=(Mesh&&) noexcept = default;

    void bind(VkCommandBuffer commandBuffer) const;
    void draw(VkCommandBuffer commandBuffer) const;

    [[nodiscard]] bool valid() const noexcept { return m_vertexBuffer.valid(); }

    // Yer tutucu geometriler: oyuncu kapsulu yerine kutu, zemin icin izgarali dortgen.
    [[nodiscard]] static Mesh createBox(VulkanDevice& device, glm::vec3 halfExtents, glm::vec3 color);
    [[nodiscard]] static Mesh createGroundPlane(VulkanDevice& device, float halfSize, int cells);

private:
    VulkanBuffer m_vertexBuffer;
    VulkanBuffer m_indexBuffer;
    uint32_t m_indexCount = 0;
};

}  // namespace optmus::gfx
