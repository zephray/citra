// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <iomanip>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <SDL.h>
#include <inih/cpp/INIReader.h>
#include "citra_sdl1/config.h"
#include "citra_sdl1/default_ini.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/param_package.h"
#include "core/hle/service/service.h"
#include "core/settings.h"
#include "input_common/main.h"
#include "input_common/udp/client.h"

Config::Config() {
    // TODO: Don't hardcode the path; let the frontend decide where to put the config files.
    sdl1_config_loc = FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir) + "sdl1-config.ini";
    sdl1_config = std::make_unique<INIReader>(sdl1_config_loc);

    Reload();
}

Config::~Config() = default;

bool Config::LoadINI(const std::string& default_contents, bool retry) {
    const char* location = this->sdl1_config_loc.c_str();
    if (sdl1_config->ParseError() < 0) {
        if (retry) {
            LOG_WARNING(Config, "Failed to load {}. Creating file from defaults...", location);
            FileUtil::CreateFullPath(location);
            FileUtil::WriteStringToFile(true, default_contents, location);
            sdl1_config = std::make_unique<INIReader>(location); // Reopen file

            return LoadINI(default_contents, false);
        }
        LOG_ERROR(Config, "Failed.");
        return false;
    }
    LOG_INFO(Config, "Successfully loaded {}", location);
    return true;
}

static const std::array<int, Settings::NativeButton::NumButtons> default_buttons = {
    SDLK_a, SDLK_s, SDLK_z, SDLK_x, SDLK_t, SDLK_g,
    SDLK_f, SDLK_h, SDLK_q, SDLK_w, SDLK_m, SDLK_n,
    SDLK_o, SDLK_p, SDLK_1, SDLK_2, SDLK_b,
};

static const std::array<std::array<int, 5>, Settings::NativeAnalog::NumAnalogs> default_analogs{{
    {
        SDLK_UP,
        SDLK_DOWN,
        SDLK_LEFT,
        SDLK_RIGHT,
        SDLK_d,
    },
    {
        SDLK_i,
        SDLK_k,
        SDLK_j,
        SDLK_l,
        SDLK_d,
    },
}};

void Config::ReadValues() {
    // Controls
    // TODO: add multiple input profile support
    for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
        std::string default_param = InputCommon::GenerateKeyboardParam(default_buttons[i]);
        Settings::values.current_input_profile.buttons[i] =
            sdl1_config->GetString("Controls", Settings::NativeButton::mapping[i], default_param);
        if (Settings::values.current_input_profile.buttons[i].empty())
            Settings::values.current_input_profile.buttons[i] = default_param;
    }

    for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
        std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
            default_analogs[i][0], default_analogs[i][1], default_analogs[i][2],
            default_analogs[i][3], default_analogs[i][4], 0.5f);
        Settings::values.current_input_profile.analogs[i] =
            sdl1_config->GetString("Controls", Settings::NativeAnalog::mapping[i], default_param);
        if (Settings::values.current_input_profile.analogs[i].empty())
            Settings::values.current_input_profile.analogs[i] = default_param;
    }

    Settings::values.current_input_profile.motion_device = sdl1_config->GetString(
        "Controls", "motion_device",
        "engine:motion_emu,update_period:100,sensitivity:0.01,tilt_clamp:90.0");
    Settings::values.current_input_profile.touch_device =
        sdl1_config->GetString("Controls", "touch_device", "engine:emu_window");
    Settings::values.current_input_profile.udp_input_address = sdl1_config->GetString(
        "Controls", "udp_input_address", InputCommon::CemuhookUDP::DEFAULT_ADDR);
    Settings::values.current_input_profile.udp_input_port =
        static_cast<u16>(sdl1_config->GetInteger("Controls", "udp_input_port",
                                                 InputCommon::CemuhookUDP::DEFAULT_PORT));

    // Core
    Settings::values.use_cpu_jit = sdl1_config->GetBoolean("Core", "use_cpu_jit", true);

    // Renderer
    // Always use software rendering
    Settings::values.use_gles = false;
    Settings::values.use_hw_renderer = false;
    Settings::values.use_hw_shader = false;
    Settings::values.shaders_accurate_gs =
        sdl1_config->GetBoolean("Renderer", "shaders_accurate_gs", true);
    Settings::values.shaders_accurate_mul =
        sdl1_config->GetBoolean("Renderer", "shaders_accurate_mul", false);
    Settings::values.use_shader_jit = false;
    Settings::values.resolution_factor =
        static_cast<u16>(sdl1_config->GetInteger("Renderer", "resolution_factor", 1));
    Settings::values.vsync_enabled = sdl1_config->GetBoolean("Renderer", "vsync_enabled", false);
    Settings::values.use_frame_limit = sdl1_config->GetBoolean("Renderer", "use_frame_limit", true);
    Settings::values.frame_limit =
        static_cast<u16>(sdl1_config->GetInteger("Renderer", "frame_limit", 100));

    Settings::values.toggle_3d = sdl1_config->GetBoolean("Renderer", "toggle_3d", false);
    Settings::values.factor_3d =
        static_cast<u8>(sdl1_config->GetInteger("Renderer", "factor_3d", 0));

    Settings::values.bg_red = static_cast<float>(sdl1_config->GetReal("Renderer", "bg_red", 0.0));
    Settings::values.bg_green =
        static_cast<float>(sdl1_config->GetReal("Renderer", "bg_green", 0.0));
    Settings::values.bg_blue = static_cast<float>(sdl1_config->GetReal("Renderer", "bg_blue", 0.0));

    // Layout
    Settings::values.layout_option =
        static_cast<Settings::LayoutOption>(sdl1_config->GetInteger("Layout", "layout_option", 0));
    Settings::values.swap_screen = sdl1_config->GetBoolean("Layout", "swap_screen", false);
    Settings::values.custom_layout = sdl1_config->GetBoolean("Layout", "custom_layout", false);
    Settings::values.custom_top_left =
        static_cast<u16>(sdl1_config->GetInteger("Layout", "custom_top_left", 0));
    Settings::values.custom_top_top =
        static_cast<u16>(sdl1_config->GetInteger("Layout", "custom_top_top", 0));
    Settings::values.custom_top_right =
        static_cast<u16>(sdl1_config->GetInteger("Layout", "custom_top_right", 400));
    Settings::values.custom_top_bottom =
        static_cast<u16>(sdl1_config->GetInteger("Layout", "custom_top_bottom", 240));
    Settings::values.custom_bottom_left =
        static_cast<u16>(sdl1_config->GetInteger("Layout", "custom_bottom_left", 40));
    Settings::values.custom_bottom_top =
        static_cast<u16>(sdl1_config->GetInteger("Layout", "custom_bottom_top", 240));
    Settings::values.custom_bottom_right =
        static_cast<u16>(sdl1_config->GetInteger("Layout", "custom_bottom_right", 360));
    Settings::values.custom_bottom_bottom =
        static_cast<u16>(sdl1_config->GetInteger("Layout", "custom_bottom_bottom", 480));

    // Audio
    Settings::values.enable_dsp_lle = sdl1_config->GetBoolean("Audio", "enable_dsp_lle", false);
    Settings::values.enable_dsp_lle_multithread =
        sdl1_config->GetBoolean("Audio", "enable_dsp_lle_multithread", false);
    Settings::values.sink_id = sdl1_config->GetString("Audio", "output_engine", "auto");
    Settings::values.enable_audio_stretching =
        sdl1_config->GetBoolean("Audio", "enable_audio_stretching", true);
    Settings::values.audio_device_id = sdl1_config->GetString("Audio", "output_device", "auto");
    Settings::values.volume = static_cast<float>(sdl1_config->GetReal("Audio", "volume", 1));
    Settings::values.mic_input_device =
        sdl1_config->GetString("Audio", "mic_input_device", "Default");
    Settings::values.mic_input_type =
        static_cast<Settings::MicInputType>(sdl1_config->GetInteger("Audio", "mic_input_type", 0));

    // Data Storage
    Settings::values.use_virtual_sd =
        sdl1_config->GetBoolean("Data Storage", "use_virtual_sd", true);

    // System
    Settings::values.is_new_3ds = sdl1_config->GetBoolean("System", "is_new_3ds", false);
    Settings::values.region_value =
        sdl1_config->GetInteger("System", "region_value", Settings::REGION_VALUE_AUTO_SELECT);
    Settings::values.init_clock =
        static_cast<Settings::InitClock>(sdl1_config->GetInteger("System", "init_clock", 1));
    {
        std::tm t;
        t.tm_sec = 1;
        t.tm_min = 0;
        t.tm_hour = 0;
        t.tm_mday = 1;
        t.tm_mon = 0;
        t.tm_year = 100;
        t.tm_isdst = 0;
        std::istringstream string_stream(
            sdl1_config->GetString("System", "init_time", "2000-01-01 00:00:01"));
        string_stream >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
        if (string_stream.fail()) {
            LOG_ERROR(Config, "Failed To parse init_time. Using 2000-01-01 00:00:01");
        }
        Settings::values.init_time =
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::from_time_t(std::mktime(&t)).time_since_epoch())
                .count();
    }

    // Camera
    using namespace Service::CAM;
    Settings::values.camera_name[OuterRightCamera] =
        sdl1_config->GetString("Camera", "camera_outer_right_name", "blank");
    Settings::values.camera_config[OuterRightCamera] =
        sdl1_config->GetString("Camera", "camera_outer_right_config", "");
    Settings::values.camera_flip[OuterRightCamera] =
        sdl1_config->GetInteger("Camera", "camera_outer_right_flip", 0);
    Settings::values.camera_name[InnerCamera] =
        sdl1_config->GetString("Camera", "camera_inner_name", "blank");
    Settings::values.camera_config[InnerCamera] =
        sdl1_config->GetString("Camera", "camera_inner_config", "");
    Settings::values.camera_flip[InnerCamera] =
        sdl1_config->GetInteger("Camera", "camera_inner_flip", 0);
    Settings::values.camera_name[OuterLeftCamera] =
        sdl1_config->GetString("Camera", "camera_outer_left_name", "blank");
    Settings::values.camera_config[OuterLeftCamera] =
        sdl1_config->GetString("Camera", "camera_outer_left_config", "");
    Settings::values.camera_flip[OuterLeftCamera] =
        sdl1_config->GetInteger("Camera", "camera_outer_left_flip", 0);

    // Miscellaneous
    Settings::values.log_filter = sdl1_config->GetString("Miscellaneous", "log_filter", "*:Info");

    // Debugging
    Settings::values.use_gdbstub = sdl1_config->GetBoolean("Debugging", "use_gdbstub", false);
    Settings::values.gdbstub_port =
        static_cast<u16>(sdl1_config->GetInteger("Debugging", "gdbstub_port", 24689));

    for (const auto& service_module : Service::service_module_map) {
        bool use_lle = sdl1_config->GetBoolean("Debugging", "LLE\\" + service_module.name, false);
        Settings::values.lle_modules.emplace(service_module.name, use_lle);
    }

    // Web Service
    Settings::values.enable_telemetry =
        sdl1_config->GetBoolean("WebService", "enable_telemetry", true);
    Settings::values.web_api_url =
        sdl1_config->GetString("WebService", "web_api_url", "https://api.citra-emu.org");
    Settings::values.citra_username = sdl1_config->GetString("WebService", "citra_username", "");
    Settings::values.citra_token = sdl1_config->GetString("WebService", "citra_token", "");
}

void Config::Reload() {
    LoadINI(DefaultINI::sdl1_config_file);
    ReadValues();
}
