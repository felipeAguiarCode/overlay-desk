#pragma once

// ARCHITECTURE.md section 2: owns shader compilation and lifetime, with optional hot
// reload in Debug builds.
//
// Release binaries carry the bytecode fxc produced at build time, so there is no shader
// compilation, no file I/O and no d3dcompiler dependency at runtime (PERFORMANCE.md
// section 4).

#include <d3d11_4.h>

#include <filesystem>
#include <string>

#include "util/ComHelpers.h"

namespace overlaydesk {

class ShaderManager {
public:
    ShaderManager() = default;
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;

    // Creates the vertex and pixel shaders from the bytecode embedded at build time.
    void CreateFromEmbedded(ID3D11Device* device);
    void Reset() noexcept;

    bool IsValid() const noexcept { return m_vertexShader && m_pixelShader; }

    ID3D11VertexShader* VertexShader() const noexcept { return m_vertexShader.get(); }
    ID3D11PixelShader* PixelShader() const noexcept { return m_pixelShader.get(); }

    // Debug-only hot reload. Recompiles both stages from the .hlsl files in
    // `shaderDirectory` and swaps them in only if both succeed; on failure the previously
    // working shaders stay bound and the error text lands in `errorOut` and the log.
    // Always returns false in Release, where there is nothing to reload from.
    bool ReloadFromSource(ID3D11Device* device, const std::filesystem::path& shaderDirectory,
                          std::string& errorOut);

    // Directory the build recorded as the shader source location. Empty when hot reload is
    // not available.
    static std::filesystem::path DefaultSourceDirectory();
    static bool HotReloadSupported() noexcept;

    // Incremented on every successful reload, so the UI can show that something happened.
    uint32_t Generation() const noexcept { return m_generation; }

private:
    com_ptr<ID3D11VertexShader> m_vertexShader;
    com_ptr<ID3D11PixelShader> m_pixelShader;
    uint32_t m_generation = 0;
};

}  // namespace overlaydesk
