cmake_minimum_required(VERSION 3.8)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(quakevr VERSION 0.0.7 LANGUAGES CXX)

set(source_list
    "Quake/bgmusic.cpp"
    "Quake/byteorder.cpp"
    "Quake/cd_sdl.cpp"
    "Quake/cfgfile.cpp"
    "Quake/chase.cpp"
    "Quake/cl_demo.cpp"
    "Quake/cl_input.cpp"
    "Quake/cl_main.cpp"
    "Quake/cl_parse.cpp"
    "Quake/cl_tent.cpp"
    "Quake/client.cpp"
    "Quake/cmd.cpp"
    "Quake/common.cpp"
    "Quake/console.cpp"
    "Quake/crc.cpp"
    "Quake/cvar.cpp"
    "Quake/developer.cpp"
    "Quake/fakeqcvm.cpp"
    "Quake/fs_zip.cpp"
    "Quake/fshandle.cpp"
    "Quake/gl_draw.cpp"
    "Quake/gl_fog.cpp"
    "Quake/gl_mesh.cpp"
    "Quake/gl_model.cpp"
    "Quake/gl_refrag.cpp"
    "Quake/gl_rlight.cpp"
    "Quake/gl_rmain.cpp"
    "Quake/gl_rmisc.cpp"
    "Quake/gl_screen.cpp"
    "Quake/gl_sky.cpp"
    "Quake/gl_texmgr.cpp"
    "Quake/gl_util.cpp"
    "Quake/gl_vidsdl.cpp"
    "Quake/gl_warp.cpp"
    "Quake/host_cmd.cpp"
    "Quake/host.cpp"
    "Quake/image.cpp"
    "Quake/in_sdl.cpp"
    "Quake/keys.cpp"
    "Quake/link.cpp"
    "Quake/main_sdl.cpp"
    "Quake/mathlib.cpp"
    "Quake/menu_keyboard.cpp"
    "Quake/menu_util.cpp"
    "Quake/menu.cpp"
    "Quake/msg.cpp"
    "Quake/net_dgrm.cpp"
    "Quake/net_loop.cpp"
    "Quake/net_main.cpp"
    "Quake/pr_cmds.cpp"
    "Quake/pr_edict.cpp"
    "Quake/pr_exec.cpp"
    "Quake/pr_ext.cpp"
    "Quake/quakeglm.cpp"
    "Quake/r_alias.cpp"
    "Quake/r_brush.cpp"
    "Quake/r_part.cpp"
    "Quake/r_sprite.cpp"
    "Quake/r_world.cpp"
    "Quake/saveutil.cpp"
    "Quake/sbar.cpp"
    "Quake/server.cpp"
    "Quake/shader.cpp"
    "Quake/sizebuf.cpp"
    "Quake/snd_codec.cpp"
    "Quake/snd_dma.cpp"
    "Quake/snd_flac.cpp"
    "Quake/snd_mem.cpp"
    "Quake/snd_mikmod.cpp"
    "Quake/snd_mix.cpp"
    "Quake/snd_modplug.cpp"
    "Quake/snd_mp3.cpp"
    "Quake/snd_mp3tag.cpp"
    "Quake/snd_opus.cpp"
    "Quake/snd_sdl.cpp"
    "Quake/snd_umx.cpp"
    "Quake/snd_voip.cpp"
    "Quake/snd_vorbis.cpp"
    "Quake/snd_wave.cpp"
    "Quake/snd_xmp.cpp"
    "Quake/strlcat.cpp"
    "Quake/strlcpy.cpp"
    "Quake/sv_main.cpp"
    "Quake/sv_move.cpp"
    "Quake/sv_phys.cpp"
    "Quake/sv_user.cpp"
    "Quake/view.cpp"
    "Quake/vr_cvars.cpp"
    "Quake/vr_showfn.cpp"
    "Quake/vr.cpp"
    "Quake/wad.cpp"
    "Quake/world.cpp"
    "Quake/zone.cpp"
)

if (WIN32)
    list(APPEND source_list
        "Quake/net_win.cpp"
        "Quake/net_wins.cpp"
        "Quake/net_wipx.cpp"
        "Quake/pl_win.cpp"
        "Quake/sys_sdl_win.cpp"
    )
else()
    list(APPEND source_list
        "Quake/net_bsd.cpp"
        "Quake/pl_linux.cpp"
        "Quake/sys_sdl_unix.cpp"
        "Quake/net_udp.cpp"
    )
endif()


set_source_files_properties(${source_list} PROPERTIES LANGUAGE CXX )

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(QUAKEVR_TARGET_NAME "quakevr-debug")
else()
    set(QUAKEVR_TARGET_NAME "quakevr")
endif()

add_executable(${QUAKEVR_TARGET_NAME} "${source_list}")
set_target_properties(${QUAKEVR_TARGET_NAME} PROPERTIES LINKER_LANGUAGE CXX)

target_compile_features(${QUAKEVR_TARGET_NAME} PUBLIC cxx_std_17)
target_compile_options(${QUAKEVR_TARGET_NAME}
    PRIVATE -Wall -Wextra -Wno-missing-field-initializers -Wpedantic -Wimplicit-fallthrough
    -Wno-language-extension-token -Wno-nested-anon-types -Wno-gnu-anonymous-struct -Wno-deprecated-declarations -Wno-microsoft-enum-value # WIN32 only
    -Wno-implicit-fallthrough -Wno-c++98-compat -Wno-c++98-compat-pedantic
)

# -g -fsanitize=address -fdiagnostics-color=always

# -Wfallthrough

target_compile_definitions(${QUAKEVR_TARGET_NAME} PRIVATE
    USE_SDL2=1
    _AMD64_=1
    -DPARANOID=1
    #WIN32=1
    #NDEBUG=1
    #_WINDOWS=1
    _USE_WINSOCK2=1
    _CRT_NONSTDC_NO_DEPRECATE=1
    _CRT_SECURE_NO_WARNINGS=1
    _WINSOCK_DEPRECATED_NO_WARNINGS=1
    USE_SDL2=1
    USE_CODEC_MP3=1
    USE_CODEC_VORBIS=1
    USE_CODEC_WAVE=1
    USE_CODEC_FLAC=1
    USE_CODEC_OPUS=1
    USE_CODEC_MIKMOD=1
    USE_CODEC_UMX=1
    #__clang__=1
    GLM_COMPILER=0
)

target_include_directories(
    ${QUAKEVR_TARGET_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Quake/>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_include_directories(
    ${QUAKEVR_TARGET_NAME} SYSTEM PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Windows/SDL2/include/>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Windows/glew/include/>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Windows/codecs/include/>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/glm/>
)


# target_compile_options(${QUAKEVR_TARGET_NAME} PRIVATE
#     "-isystem $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Windows/SDL2/include/>"
#     "-isystem $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Windows/glew/include/>"
#     "-isystem $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Windows/codecs/include/>"
#     "-isystem $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/glm/>"
# )

set_property(TARGET ${QUAKEVR_TARGET_NAME} PROPERTY CXX_STANDARD 17)

include(FindOpenGL)

if (WIN32)
    set(SDL2_LIBRARIES
        "C:/OHWorkspace/quakevr/Windows/SDL2/lib64/SDL2main.lib"
        "C:/OHWorkspace/quakevr/Windows/SDL2/lib64/SDL2.lib"
    )

    target_link_libraries(${QUAKEVR_TARGET_NAME}
        ${OPENGL_gl_LIBRARY} ${SDL2_LIBRARIES} wsock32 winmm ws2_32
    )

    set(OPENVR_LIBRARIES
        "C:/OHWorkspace/quakevr/Windows/OpenVR/lib/win64/openvr_api.lib"
    )

    # find_package(GLEW)
    # if (GLEW_FOUND)
    #     include_directories(${GLEW_INCLUDE_DIRS})
    # endif()

    set(OTHER_LIBS
        "openvr_api.lib"
        "libvorbisfile.lib"
        "libvorbis.lib"
        "libopusfile.lib"
        "libopus.lib"
        "libFLAC.lib"
        "libogg.lib"
        "libmad.lib"
        "libmikmod.lib"
        "ws2_32.lib"
        "opengl32.lib"
        "winmm.lib"
        "SDL2.lib"
        "SDL2main.lib"
        "glew32.lib"
    )

    target_link_directories(${QUAKEVR_TARGET_NAME} PUBLIC
        "C:/OHWorkspace/openvr/lib/win64"
        "Windows/codecs/x64"
        "Windows/SDL2/lib64"
        "Windows/glew/lib"
    )

    target_link_libraries(${QUAKEVR_TARGET_NAME}
        ${OPENGL_gl_LIBRARY} ${SDL2_LIBRARIES} ${OPENVR_LIBRARIES} ${GLEW_LIBRARIES} ${OTHER_LIBS}
    )
else()
    find_package(SDL2 REQUIRED)
    include_directories(${SDL2_INCLUDE_DIRS})

    # FIND_PACKAGE(PkgConfig)
    # PKG_SEARCH_MODULE(OPENVR REQUIRED openvr)

    set(OPENVR_LIBRARIES
        "/home/vittorioromeo/Repos/openvr/bin/linux64/libopenvr_api.so"
    )

    find_package(GLEW REQUIRED)
    if (GLEW_FOUND)
        include_directories(${GLEW_INCLUDE_DIRS})
    endif()

    target_link_libraries(${QUAKEVR_TARGET_NAME}
        asan ${OPENGL_gl_LIBRARY} SDL2::SDL2 ${OPENVR_LIBRARIES} ${GLEW_LIBRARIES} opus FLAC ogg mad mikmod vorbis vorbisfile opusfile mpg123
    )
endif()


