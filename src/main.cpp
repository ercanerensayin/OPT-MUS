#include "core/Application.hpp"
#include "render/VulkanCommon.hpp"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

// SPIR-V dosyalari calisma dizinine gore degil, yurutulebilir dosyaya gore aranir;
// boylece IDE'den veya terminalden calistirmak fark etmez.
std::string resolveShaderDirectory(int argc, char** argv) {
    if (argc > 1) {
        return argv[1];
    }

    std::error_code ec;
    const std::filesystem::path executable = std::filesystem::absolute(argv[0], ec);
    if (!ec) {
        const std::filesystem::path candidate = executable.parent_path() / "shaders";
        if (std::filesystem::exists(candidate, ec)) {
            return candidate.string();
        }
    }
    return "shaders";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        optmus::core::Application application(resolveShaderDirectory(argc, argv));
        application.run();
    } catch (const optmus::gfx::VulkanError& error) {
        std::cerr << "\n[HATA] Vulkan: " << error.what() << '\n';
        return EXIT_FAILURE;
    } catch (const std::exception& error) {
        std::cerr << "\n[HATA] " << error.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
