// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdlib>
#include <string>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <fmt/format.h>
#include <glad/glad.h>
#include "citra/emu_window/emu_window_sdl1.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "core/3ds.h"
#include "core/settings.h"
#include "input_common/keyboard.h"
#include "input_common/main.h"
#include "input_common/motion_emu.h"
#include "input_common/sdl/sdl.h"
#include "network/network.h"

void EmuWindow_SDL1::OnMouseMotion(s32 x, s32 y) {
    TouchMoved((unsigned)std::max(x, 0), (unsigned)std::max(y, 0));
    InputCommon::GetMotionEmu()->Tilt(x, y);
}

void EmuWindow_SDL1::OnMouseButton(u32 button, u8 state, s32 x, s32 y) {
    if (button == SDL_BUTTON_LEFT) {
        if (state == SDL_PRESSED) {
            TouchPressed((unsigned)std::max(x, 0), (unsigned)std::max(y, 0));
        } else {
            TouchReleased();
        }
    } else if (button == SDL_BUTTON_RIGHT) {
        if (state == SDL_PRESSED) {
            InputCommon::GetMotionEmu()->BeginTilt(x, y);
        } else {
            InputCommon::GetMotionEmu()->EndTilt();
        }
    }
}

void EmuWindow_SDL1::OnKeyEvent(int key, u8 state) {
    if (state == SDL_PRESSED) {
        InputCommon::GetKeyboard()->PressKey(key);
    } else if (state == SDL_RELEASED) {
        InputCommon::GetKeyboard()->ReleaseKey(key);
    }
}

bool EmuWindow_SDL1::IsOpen() const {
    return is_open;
}

void EmuWindow_SDL1::Fullscreen() {
    SDL_WM_ToggleFullScreen(screen);
}

EmuWindow_SDL1::EmuWindow_SDL1(bool fullscreen) {
    // Initialize the window
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        LOG_CRITICAL(Frontend, "Failed to initialize SDL1! Exiting...");
        exit(1);
    }

    InputCommon::Init();
    Network::Init();

    SDL_SetMainReady();

    std::string window_title = fmt::format("Citra {} | {}-{}", Common::g_build_fullname,
                                           Common::g_scm_branch, Common::g_scm_desc);
    SDL_WM_SetCaption(window_title.c_str(), NULL);

    screen = SDL_SetVideoMode(Core::kScreenTopWidth,
            Core::kScreenTopHeight + Core::kScreenBottomHeight,
            32, SDL_SWSURFACE);

    if (screen == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to create SDL1 window: {}", SDL_GetError());
        exit(1);
    }

    if (fullscreen) {
        Fullscreen();
    }

    LOG_INFO(Frontend, "Citra Version: {} | {}-{}", Common::g_build_fullname, Common::g_scm_branch,
             Common::g_scm_desc);
    Settings::LogSettings();
}

EmuWindow_SDL1::~EmuWindow_SDL1() {
    Network::Shutdown();
    InputCommon::Shutdown();
    SDL_Quit();
}

void EmuWindow_SDL1::SwapBuffers() {
    SDL_Flip(screen);
}

void EmuWindow_SDL1::PollEvents() {
    SDL_Event event;

    // SDL_PollEvent returns 0 when there are no more events in the event queue
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            is_open = false;
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            OnKeyEvent(static_cast<int>(event.key.keysym.scancode), event.key.state);
            break;
        case SDL_MOUSEMOTION:
            // ignore if it came from touch
            if (event.button.which != SDL_TOUCH_MOUSEID)
                OnMouseMotion(event.motion.x, event.motion.y);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            // ignore if it came from touch
            if (event.button.which != SDL_TOUCH_MOUSEID) {
                OnMouseButton(event.button.button, event.button.state, event.button.x,
                              event.button.y);
            }
            break;
        case SDL_QUIT:
            is_open = false;
            break;
        default:
            break;
        }
    }
}
