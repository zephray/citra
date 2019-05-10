// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "common/common_types.h"
#include "common/math_util.h"
#include "core/hw/gpu.h"
#include "video_core/renderer_base.h"

namespace Layout {
struct FramebufferLayout;
}

namespace SWONLY {

class RendererSoftware : public RendererBase {
public:
    explicit RendererSoftware(Frontend::EmuWindow& window);
    ~RendererSoftware() override;

    /// Swap buffers (render frame)
    void SwapBuffers() override;

    /// Initialize the renderer
    Core::System::ResultStatus Init() override;

    /// Shutdown the renderer
    void ShutDown() override;

private:
    // Loads framebuffer from emulated memory into the display information structure
    void LoadFB(const GPU::Regs::FramebufferConfig& framebuffer,
                            bool bottom);
    // Fills active OpenGL texture with the given RGB color.
    void LoadColor(u8 color_r, u8 color_g, u8 color_b);

    void memcpy_transpose(uint32_t *dst, uint32_t *src,
        int32_t x_modifier, int32_t y_modifier, uint32_t x_count,
        uint32_t y_count);

    uint32_t *render_buffer;
};

} // namespace SWONLY
