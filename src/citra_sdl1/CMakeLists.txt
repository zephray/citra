set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)

add_executable(citra_sdl1
    citra.cpp
    citra.rc
    config.cpp
    config.h
    default_ini.h
    emu_window/emu_window_sdl1.cpp
    emu_window/emu_window_sdl1.h
    resource.h
)

create_target_directory_groups(citra_sdl1)

target_link_libraries(citra_sdl1 PRIVATE common core input_common network)
target_link_libraries(citra_sdl1 PRIVATE inih glad)
if (MSVC)
    target_link_libraries(citra_sdl1 PRIVATE getopt)
endif()
target_link_libraries(citra_sdl1 PRIVATE ${PLATFORM_LIBRARIES} SDL Threads::Threads)

if(UNIX AND NOT APPLE)
    install(TARGETS citra_sdl1 RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()

if (MSVC)
    include(CopyCitraSDLDeps)
    copy_citra_SDL_deps(citra_sdl1)
endif()
