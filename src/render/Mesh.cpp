#include "render/Mesh.hpp"

#include "render/VulkanDevice.hpp"

#include <cstddef>

namespace optmus::gfx {

VkVertexInputBindingDescription Vertex::bindingDescription() {
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding;
}

std::array<VkVertexInputAttributeDescription, 3> Vertex::attributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> attributes{};

    attributes[0].location = 0;
    attributes[0].binding = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = offsetof(Vertex, position);

    attributes[1].location = 1;
    attributes[1].binding = 0;
    attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset = offsetof(Vertex, normal);

    attributes[2].location = 2;
    attributes[2].binding = 0;
    attributes[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[2].offset = offsetof(Vertex, color);

    return attributes;
}

Mesh::Mesh(VulkanDevice& device,
           const std::vector<Vertex>& vertices,
           const std::vector<uint32_t>& indices)
    : m_indexCount(static_cast<uint32_t>(indices.size())) {
    m_vertexBuffer = VulkanBuffer::createDeviceLocal(
        device, vertices.data(), sizeof(Vertex) * vertices.size(),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    m_indexBuffer = VulkanBuffer::createDeviceLocal(
        device, indices.data(), sizeof(uint32_t) * indices.size(),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

void Mesh::bind(VkCommandBuffer commandBuffer) const {
    const VkBuffer buffers[] = {m_vertexBuffer.handle()};
    const VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer.handle(), 0, VK_INDEX_TYPE_UINT32);
}

void Mesh::draw(VkCommandBuffer commandBuffer) const {
    vkCmdDrawIndexed(commandBuffer, m_indexCount, 1, 0, 0, 0);
}

Mesh Mesh::createBox(VulkanDevice& device, glm::vec3 halfExtents, glm::vec3 color) {
    const glm::vec3 h = halfExtents;

    // Her yuz kendi normaline sahip olmali; bu yuzden koseler paylasilmaz (24 kose).
    const std::array<glm::vec3, 6> normals{
        glm::vec3{0.0F, 0.0F, 1.0F},  glm::vec3{0.0F, 0.0F, -1.0F},
        glm::vec3{1.0F, 0.0F, 0.0F},  glm::vec3{-1.0F, 0.0F, 0.0F},
        glm::vec3{0.0F, 1.0F, 0.0F},  glm::vec3{0.0F, -1.0F, 0.0F},
    };

    const std::array<std::array<glm::vec3, 4>, 6> faces{{
        {{{-h.x, -h.y, h.z}, {h.x, -h.y, h.z}, {h.x, h.y, h.z}, {-h.x, h.y, h.z}}},      // +Z
        {{{h.x, -h.y, -h.z}, {-h.x, -h.y, -h.z}, {-h.x, h.y, -h.z}, {h.x, h.y, -h.z}}},  // -Z
        {{{h.x, -h.y, h.z}, {h.x, -h.y, -h.z}, {h.x, h.y, -h.z}, {h.x, h.y, h.z}}},      // +X
        {{{-h.x, -h.y, -h.z}, {-h.x, -h.y, h.z}, {-h.x, h.y, h.z}, {-h.x, h.y, -h.z}}},  // -X
        {{{-h.x, h.y, h.z}, {h.x, h.y, h.z}, {h.x, h.y, -h.z}, {-h.x, h.y, -h.z}}},      // +Y
        {{{-h.x, -h.y, -h.z}, {h.x, -h.y, -h.z}, {h.x, -h.y, h.z}, {-h.x, -h.y, h.z}}},  // -Y
    }};

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(24);
    indices.reserve(36);

    for (size_t face = 0; face < faces.size(); ++face) {
        const auto base = static_cast<uint32_t>(vertices.size());
        // Yuzeyin yonune gore hafif renk farki: isiklandirma olmasa da hacim okunur.
        const float shade = 0.80F + 0.20F * static_cast<float>(face % 3);
        for (const glm::vec3& position : faces[face]) {
            vertices.push_back(Vertex{position, normals[face], color * shade});
        }
        // Saat yonunun tersi (CCW) sarim; pipeline'da front face = CCW.
        indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});
    }

    return Mesh(device, vertices, indices);
}

Mesh Mesh::createGroundPlane(VulkanDevice& device, float halfSize, int cells) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const float step = (2.0F * halfSize) / static_cast<float>(cells);
    const glm::vec3 up{0.0F, 1.0F, 0.0F};

    for (int z = 0; z < cells; ++z) {
        for (int x = 0; x < cells; ++x) {
            const float x0 = -halfSize + static_cast<float>(x) * step;
            const float z0 = -halfSize + static_cast<float>(z) * step;
            const float x1 = x0 + step;
            const float z1 = z0 + step;

            // Satranc tahtasi deseni: sabit zaman adimli hareketi gozle dogrulamak icin.
            const bool dark = ((x + z) % 2) == 0;
            const glm::vec3 color = dark ? glm::vec3{0.18F, 0.20F, 0.24F}
                                         : glm::vec3{0.26F, 0.29F, 0.33F};

            const auto base = static_cast<uint32_t>(vertices.size());
            vertices.push_back(Vertex{{x0, 0.0F, z0}, up, color});
            vertices.push_back(Vertex{{x0, 0.0F, z1}, up, color});
            vertices.push_back(Vertex{{x1, 0.0F, z1}, up, color});
            vertices.push_back(Vertex{{x1, 0.0F, z0}, up, color});
            indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});
        }
    }

    return Mesh(device, vertices, indices);
}

}  // namespace optmus::gfx
