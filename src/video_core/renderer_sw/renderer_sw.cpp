// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include "common/assert.h"
#include "common/bit_field.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/frontend/emu_window.h"
#include "core/hw/gpu.h"
#include "core/hw/hw.h"
#include "core/hw/lcd.h"
#include "core/memory.h"
#include "core/settings.h"
#include "core/tracer/recorder.h"
#include "video_core/debug_utils/debug_utils.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_sw/renderer_sw.h"
#include "video_core/video_core.h"

namespace SWONLY {

RendererSoftware::RendererSoftware(Frontend::EmuWindow& window) : RendererBase{window} {
    render_buffer = (uint32_t *)window.GetBuffer();
}

RendererSoftware::~RendererSoftware() = default;

/// Swap buffers (render frame)
void RendererSoftware::SwapBuffers() {

    for (int i : {0, 1}) {
        int fb_id = i;
        const auto& framebuffer = GPU::g_regs.framebuffer_config[fb_id];

        // Main LCD (0): 0x1ED02204, Sub LCD (1): 0x1ED02A04
        u32 lcd_color_addr =
            (fb_id == 0) ? LCD_REG_INDEX(color_fill_top) : LCD_REG_INDEX(color_fill_bottom);
        lcd_color_addr = HW::VADDR_LCD + 4 * lcd_color_addr;
        LCD::Regs::ColorFill color_fill = {0};
        LCD::Read(color_fill.raw, lcd_color_addr);

        if (color_fill.is_enabled) {
            LoadColor(color_fill.color_r, color_fill.color_g, color_fill.color_b);
        } else {
            LoadFB(framebuffer, i == 1);
        }
    }

    if (VideoCore::g_renderer_screenshot_requested) {
        VideoCore::g_renderer_screenshot_requested = false;
    }

    Core::System::GetInstance().perf_stats.EndSystemFrame();

    // Swap buffers
    render_window.PollEvents();
    render_window.SwapBuffers();

    Core::System::GetInstance().frame_limiter.DoFrameLimiting(
        Core::System::GetInstance().CoreTiming().GetGlobalTimeUs());
    Core::System::GetInstance().perf_stats.BeginSystemFrame();

    RefreshRasterizerSetting();

    if (Pica::g_debug_context && Pica::g_debug_context->recorder) {
        Pica::g_debug_context->recorder->FrameFinished();
    }
}

void RendererSoftware::memcpy_transpose(uint32_t *dst, uint8_t *src,
        int32_t x_modifier, int32_t y_modifier, uint32_t x_count,
        uint32_t y_count, bool rgb565) {
    uint8_t *src_ptr = src;
    uint32_t src_color;
    uint32_t *dst_ptr = dst;
    for (int y = 0; y < x_count; y++) { // X is flipped to Y
        for (int x = 0; x < y_count; x++) { // Y is flipped to X
            if (rgb565) {
                src_color = (*src_ptr) | (*(src_ptr + 1) << 8);
                src_color = ((src_color & 0x1F) << 3) | ((src_color & 0x7E0) << 5) | ((src_color & 0xF800) << 8) | 0xFF000000;
            }
            else {
                src_color = (*src_ptr) | (*(src_ptr + 1) << 8) | (*(src_ptr + 2) << 16) | 0xFF000000;
            }

            *dst_ptr ++ = src_color;
            src_ptr += x_modifier;
        }
        src_ptr += y_modifier;
    }
}

/**
 * Loads framebuffer from emulated memory into the screen buffer
 */
void RendererSoftware::LoadFB(const GPU::Regs::FramebufferConfig& framebuffer, bool bottom) {

    GPU::Regs::PixelFormat format = framebuffer.color_format;

    const PAddr framebuffer_addr =
        framebuffer.active_fb == 0
            ? (framebuffer.address_left1)
            : (framebuffer.address_left2);

    LOG_TRACE(Render_Software, "0x{:08x} bytes from 0x{:08x}({}x{}), fmt {:x}",
              framebuffer.stride * framebuffer.height, framebuffer_addr, (int)framebuffer.width,
              (int)framebuffer.height, (int)framebuffer.format);

    int bpp = GPU::Regs::BytesPerPixel(framebuffer.color_format);
    std::size_t pixel_stride = framebuffer.stride / bpp;

    // OpenGL only supports specifying a stride in units of pixels, not bytes, unfortunately
    ASSERT(pixel_stride * bpp == framebuffer.stride);

    // Ensure no bad interactions with GL_UNPACK_ALIGNMENT, which by default
    // only allows rows to have a memory alignement of 4.
    ASSERT(pixel_stride % 4 == 0);

    uint32_t modifier_multiplier = 4;
    bool rgb565 = false;

    switch (format) {
    case GPU::Regs::PixelFormat::RGBA8:
        modifier_multiplier = 4;
        rgb565 = false;
        break;

    case GPU::Regs::PixelFormat::RGB8:
        modifier_multiplier = 3;
        rgb565 = false;
        break;

    case GPU::Regs::PixelFormat::RGB565:
        modifier_multiplier = 2;
        rgb565 = true;
        break;

    default:
        UNIMPLEMENTED();
    }

    uint8_t * framebuffer_data = (VideoCore::g_memory->GetPhysicalPointer(framebuffer_addr));

    if (!bottom) {
        memcpy_transpose(render_buffer, framebuffer_data, 240 * modifier_multiplier, (0-240*400+1) * modifier_multiplier, 240, 400, rgb565);
        //memcpy(render_buffer, framebuffer_data, 400*240*4);
    }
    else {
        memcpy_transpose(render_buffer + 400 * 240, framebuffer_data, 240 * modifier_multiplier, (0-240*320+1) * modifier_multiplier, 240, 320, rgb565);
    }

}

/**
 * Fills active OpenGL texture with the given RGB color. Since the color is solid, the texture can
 * be 1x1 but will stretch across whatever it's rendered on.
 */
void RendererSoftware::LoadColor(u8 color_r, u8 color_g, u8 color_b) {
    LOG_DEBUG(Render_Software, "Setting BG color");
}

/// Initialize the renderer
Core::System::ResultStatus RendererSoftware::Init() {
    RefreshRasterizerSetting();

    return Core::System::ResultStatus::Success;
}

/// Shutdown the renderer
void RendererSoftware::ShutDown() {}

} // namespace
