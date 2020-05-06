cmake_minimum_required(VERSION 3.8)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(quakespasm-sdl2 VERSION 0.0.1 LANGUAGES CXX)

set(source_list
    "../Quake/bgmusic.cpp"
    "../Quake/cd_sdl.cpp"
    "../Quake/cfgfile.cpp"
    "../Quake/chase.cpp"
    "../Quake/cl_demo.cpp"
    "../Quake/cl_input.cpp"
    "../Quake/cl_main.cpp"
    "../Quake/cl_parse.cpp"
    "../Quake/cl_tent.cpp"
    "../Quake/cmd.cpp"
    "../Quake/common.cpp"
    "../Quake/console.cpp"
    "../Quake/crc.cpp"
    "../Quake/cvar.cpp"
    "../Quake/gl_draw.cpp"
    "../Quake/gl_fog.cpp"
    "../Quake/gl_mesh.cpp"
    "../Quake/gl_model.cpp"
    "../Quake/gl_refrag.cpp"
    "../Quake/gl_rlight.cpp"
    "../Quake/gl_rmain.cpp"
    "../Quake/gl_rmisc.cpp"
    "../Quake/gl_screen.cpp"
    "../Quake/gl_sky.cpp"
    "../Quake/gl_texmgr.cpp"
    "../Quake/gl_vidsdl.cpp"
    "../Quake/gl_warp.cpp"
    "../Quake/host.cpp"
    "../Quake/host_cmd.cpp"
    "../Quake/image.cpp"
    "../Quake/in_sdl.cpp"
    "../Quake/keys.cpp"
    "../Quake/main_sdl.cpp"
    "../Quake/mathlib.cpp"
    "../Quake/menu.cpp"
    "../Quake/menu_util.cpp"
    "../Quake/net_dgrm.cpp"
    "../Quake/net_loop.cpp"
    "../Quake/net_main.cpp"
    "../Quake/net_udp.cpp"
    "../Quake/pr_cmds.cpp"
    "../Quake/pr_edict.cpp"
    "../Quake/pr_exec.cpp"
    "../Quake/r_alias.cpp"
    "../Quake/r_brush.cpp"
    "../Quake/r_part.cpp"
    "../Quake/r_sprite.cpp"
    "../Quake/r_world.cpp"
    "../Quake/sbar.cpp"
    "../Quake/snd_codec.cpp"
    "../Quake/snd_dma.cpp"
    "../Quake/snd_flac.cpp"
    "../Quake/snd_mem.cpp"
    "../Quake/snd_mikmod.cpp"
    "../Quake/snd_mix.cpp"
    "../Quake/snd_modplug.cpp"
    "../Quake/snd_mp3.cpp"
    "../Quake/snd_mp3tag.cpp"
    "../Quake/snd_mpg123.cpp"
    "../Quake/snd_opus.cpp"
    "../Quake/snd_sdl.cpp"
    "../Quake/snd_umx.cpp"
    "../Quake/snd_vorbis.cpp"
    "../Quake/snd_wave.cpp"
    "../Quake/snd_xmp.cpp"
    "../Quake/strlcat.cpp"
    "../Quake/strlcpy.cpp"
    "../Quake/sv_main.cpp"
    "../Quake/sv_move.cpp"
    "../Quake/sv_phys.cpp"
    "../Quake/sv_user.cpp"
    "../Quake/view.cpp"
    "../Quake/vr.cpp"
    "../Quake/wad.cpp"
    "../Quake/world.cpp"
    "../Quake/zone.cpp"
)

if (WIN32)
    list(APPEND source_list
        "../Quake/net_win.cpp"
        "../Quake/net_wins.cpp"
        "../Quake/net_wipx.cpp"
        "../Quake/pl_win.cpp"
        "../Quake/sys_sdl_win.cpp"
    )
else()
    list(APPEND source_list
        "../Quake/net_bsd.cpp"
        "../Quake/pl_linux.cpp"
        "../Quake/sys_sdl_unix.cpp"
    )
endif()


set_source_files_properties(${source_list} PROPERTIES LANGUAGE CXX )

add_executable(quakespasm-sdl2 "${CMAKE_CURRENT_SOURCE_DIR}/../Quake/vr.cpp" "${source_list}")
set_target_properties(quakespasm-sdl2 PROPERTIES LINKER_LANGUAGE CXX)

target_compile_features(quakespasm-sdl2 PUBLIC cxx_std_17)
target_compile_options(quakespasm-sdl2
    PRIVATE -x c++ -Wall -Wextra -Wno-missing-field-initializers -fdiagnostics-color=always -Wpedantic -Wimplicit-fallthrough -g
    -Wno-language-extension-token -Wno-nested-anon-types -Wno-gnu-anonymous-struct -Wno-deprecated-declarations -Wno-microsoft-enum-value # WIN32 only
    -Wfallthrough
)
    # -fsanitize=address)
target_compile_definitions(quakespasm-sdl2 PRIVATE USE_SDL2=1 _AMD64_=1)

target_include_directories(
    quakespasm-sdl2 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/SDL2/include/>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../Quake/>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../glm/>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

include(FindOpenGL)

if (WIN32)
    set(SDL2_LIBRARIES
        "C:/OHWorkspace/quakevr/Windows/SDL2/lib64/SDL2main.lib"
        "C:/OHWorkspace/quakevr/Windows/SDL2/lib64/SDL2.lib"
    )

    target_link_libraries(quakespasm-sdl2
        ${OPENGL_gl_LIBRARY} ${SDL2_LIBRARIES} wsock32 winmm ws2_32
    )

    set(OPENVR_LIBRARIES
        "C:/OHWorkspace/quakevr/Windows/OpenVR/lib/win64/openvr_api.lib"
    )

    target_link_libraries(quakespasm-sdl2
        ${OPENGL_gl_LIBRARY} ${SDL2_LIBRARIES} ${OPENVR_LIBRARIES}
    )
else()
    find_package(SDL2 REQUIRED)
    include_directories(${SDL2_INCLUDE_DIRS})

    FIND_PACKAGE(PkgConfig)
    PKG_SEARCH_MODULE(OPENVR REQUIRED openvr)

    # set(OPENVR_LIBRARIES
    #     "/home/vittorioromeo/Repos/openvr/bin/linux64/libopenvr_api.so"
    # )

    target_link_libraries(quakespasm-sdl2
        asan ${OPENGL_gl_LIBRARY} SDL2::SDL2 ${OPENVR_LIBRARIES}
    )
endif()


