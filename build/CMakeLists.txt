cmake_minimum_required(VERSION 3.5)

project(SkylandEngine)

set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(SOURCE_DIR ${ROOT_DIR}/source)
set(IMAGE_DIR ${ROOT_DIR}/images)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_language(ASM)



option(GAMEBOY_ADVANCE "GameboyAdvance" ON)
option(GBA_AUTOBUILD_IMG "AutobuildImg" OFF)
option(GBA_AUTOBUILD_CONF "AutobuildConf" OFF)


if(GAMEBOY_ADVANCE AND NOT DEVKITARM)
  message(WARNING "Note: GAMEBOY_ADVANCE option is ON by default.")
  message(FATAL_ERROR "GAMEBOY_ADVANCE option is set, but missing ARM toolchain.")
endif()


if(GAMEBOY_ADVANCE AND WIN32)
  message(FATAL_ERROR "Gameboy Advance builds not supported in windows")
endif()


include_directories(${SOURCE_DIR})
include_directories(${ROOT_DIR}/external/)


if(APPLE)
  set(EMACS_FLYCHECK_FRONTEND clang)
else()
  set(EMACS_FLYCHECK_FRONTEND gcc)
endif()

# For emacs flycheck users
configure_file(dirlocals.in ${SOURCE_DIR}/.dir-locals.el)



set(SOURCES
  ${SOURCE_DIR}/graphics/overlay.cpp
  ${SOURCE_DIR}/graphics/sprite.cpp
  ${SOURCE_DIR}/number/numeric.cpp
  ${SOURCE_DIR}/number/random.cpp
  ${SOURCE_DIR}/graphics/view.cpp
  ${SOURCE_DIR}/localization.cpp
  ${SOURCE_DIR}/collision.cpp
  ${SOURCE_DIR}/tileMap.cpp
  ${SOURCE_DIR}/globals.cpp
  ${SOURCE_DIR}/camera.cpp
  ${SOURCE_DIR}/string.cpp
  ${SOURCE_DIR}/start.cpp
  ${SOURCE_DIR}/path.cpp)


set(SOURCES
  ${SOURCES}
  ${SOURCE_DIR}/skyland/room.cpp
  ${SOURCE_DIR}/skyland/path.cpp
  ${SOURCE_DIR}/skyland/save.cpp
  ${SOURCE_DIR}/skyland/island.cpp
  ${SOURCE_DIR}/skyland/camera.cpp
  ${SOURCE_DIR}/skyland/serial.cpp
  ${SOURCE_DIR}/skyland/skyland.cpp
  ${SOURCE_DIR}/skyland/network.cpp
  ${SOURCE_DIR}/skyland/roomPool.cpp
  ${SOURCE_DIR}/skyland/script_defs.cpp
  ${SOURCE_DIR}/skyland/configure_island.cpp
  ${SOURCE_DIR}/skyland/opponent/enemyAI.cpp
  ${SOURCE_DIR}/skyland/opponent/multiplayerPeer.cpp
  ${SOURCE_DIR}/skyland/scene_pool.cpp
  ${SOURCE_DIR}/skyland/room_metatable.cpp
  ${SOURCE_DIR}/skyland/entity/explosion/explosion.cpp
  ${SOURCE_DIR}/skyland/entity/projectile/missile.cpp
  ${SOURCE_DIR}/skyland/entity/projectile/cannonball.cpp
  ${SOURCE_DIR}/skyland/entity/projectile/ionBurst.cpp
  ${SOURCE_DIR}/skyland/entity/character/basicCharacter.cpp
  ${SOURCE_DIR}/skyland/rooms/core.cpp
  ${SOURCE_DIR}/skyland/rooms/hull.cpp
  ${SOURCE_DIR}/skyland/rooms/radar.cpp
  ${SOURCE_DIR}/skyland/rooms/cannon.cpp
  ${SOURCE_DIR}/skyland/rooms/bulkhead.cpp
  ${SOURCE_DIR}/skyland/rooms/workshop.cpp
  ${SOURCE_DIR}/skyland/rooms/infirmary.cpp
  ${SOURCE_DIR}/skyland/rooms/stairwell.cpp
  ${SOURCE_DIR}/skyland/rooms/ionCannon.cpp
  ${SOURCE_DIR}/skyland/rooms/replicator.cpp
  ${SOURCE_DIR}/skyland/rooms/forcefield.cpp
  ${SOURCE_DIR}/skyland/rooms/missileSilo.cpp
  ${SOURCE_DIR}/skyland/rooms/transporter.cpp
  ${SOURCE_DIR}/skyland/rooms/plunderedRoom.cpp
  ${SOURCE_DIR}/skyland/scene/hintScene.cpp
  ${SOURCE_DIR}/skyland/scene/worldScene.cpp
  ${SOURCE_DIR}/skyland/scene/readyScene.cpp
  ${SOURCE_DIR}/skyland/scene/fadeInScene.cpp
  ${SOURCE_DIR}/skyland/scene/fadeOutScene.cpp
  ${SOURCE_DIR}/skyland/scene/newgameScene.cpp
  ${SOURCE_DIR}/skyland/scene/worldMapScene.cpp
  ${SOURCE_DIR}/skyland/scene/inspectP2Scene.cpp
  ${SOURCE_DIR}/skyland/scene/loadLevelScene.cpp
  ${SOURCE_DIR}/skyland/scene/zoneImageScene.cpp
  ${SOURCE_DIR}/skyland/scene/scriptHookScene.cpp
  ${SOURCE_DIR}/skyland/scene/highscoresScene.cpp
  ${SOURCE_DIR}/skyland/scene/boxedDialogScene.cpp
  ${SOURCE_DIR}/skyland/scene/salvageRoomScene.cpp
  ${SOURCE_DIR}/skyland/scene/titleScreenScene.cpp
  ${SOURCE_DIR}/skyland/scene/introCreditsScene.cpp
  ${SOURCE_DIR}/skyland/scene/constructionScene.cpp
  ${SOURCE_DIR}/skyland/scene/moveCharacterScene.cpp
  ${SOURCE_DIR}/skyland/scene/selectChallengeScene.cpp
  ${SOURCE_DIR}/skyland/scene/weaponSetTargetScene.cpp
  ${SOURCE_DIR}/skyland/scene/fullscreenDialogScene.cpp
  ${SOURCE_DIR}/skyland/scene/recoverCharacterScene.cpp
  ${SOURCE_DIR}/skyland/scene/transportCharacterScene.cpp
  ${SOURCE_DIR}/skyland/scene/multiplayerConnectScene.cpp
  ${SOURCE_DIR}/skyland/scene/replicatorSelectionScene.cpp
  ${SOURCE_DIR}/skyland/scene/playerIslandDestroyedScene.cpp)


set(SOURCES
  ${SOURCES}
  ${SOURCE_DIR}/script/lisp.cpp
  ${SOURCE_DIR}/script/compiler.cpp
  ${SOURCE_DIR}/script/vm.cpp)


set(SOURCES
  ${SOURCES})



# Ok, I should explain... For each call to add_file, the cmake script creates a
# build dependency for the executable, converts the file to an array of bytes
# with xxd, outputs a .cpp file, and adds the name of the .cpp file to a list of
# files, which are then compiled into a static library and linked with the
# executable. Maybe this sounds complicated, but now adding files to the project
# is as easy as `add_file(...)` (see below).
set(FILES_DIR ${SOURCE_DIR}/../)
set(FILES "")
set(FILE_DECLS "")
set(FILES_SOURCE_LIST)



macro(add_file prefix file suffix)

  set(FILES ${FILES} "\n    {\"${prefix}\", \"${file}.${suffix}\", file_${file}},\n//")
  set(FILE_DECLS ${FILE_DECLS}
    "\nextern const unsigned char file_${file}[];\n//")

  if(GBA_AUTOBUILD_CONF)

    set(FILE_IN ${FILES_DIR}/${prefix}/${file}.${suffix})
    set(FILE_OUT ${SOURCE_DIR}/data/file_${file}.cpp)

    add_custom_command(OUTPUT ${FILE_OUT}
      COMMAND echo "extern const unsigned char file_${file}[] = {" > ${FILE_OUT}
      COMMAND cat ${FILE_IN} | xxd -i | tee -a ${FILE_OUT} > /dev/null
      COMMAND echo ", 0x00}\;" | tee -a ${FILE_OUT}
      DEPENDS ${FILE_IN})

    add_custom_target(convert_${file}_file DEPENDS ${FILE_OUT})

    add_dependencies(SkylandEngine convert_${file}_file)

    list(APPEND FILES_SOURCE_LIST "${DATA_DIR}file_${file}.cpp")

  endif()
endmacro()


if(GAMEBOY_ADVANCE)

  set(DATA_DIR ${SOURCE_DIR}/data/)

  set(SOURCES
    ${SOURCES}
    ${SOURCE_DIR}/platform/gba/gba_platform.cpp
    ${SOURCE_DIR}/platform/gba/gbp_logo.cpp
    ${SOURCE_DIR}/platform/gba/memcpy.s
    ${SOURCE_DIR}/platform/gba/filesystem.cpp

    ${ROOT_DIR}/external/libgba/interrupt.c
    ${ROOT_DIR}/external/libgba/interrupt_dispatch.s
    ${ROOT_DIR}/external/libgba/bios_math.s
    ${ROOT_DIR}/external/libgba/reset.s

    ${SOURCE_DIR}/platform/gba/rumble.c

    # Data files below. Because the gameboy advance does not have a filesystem,
    # all of the sounds and images need to be compiled into the ROM.
    ${DATA_DIR}/music_chantiers_navals_412.s
    ${DATA_DIR}/overlay.s
    ${DATA_DIR}/overlay_dialog.s
    ${DATA_DIR}/overlay_world_map.s
    ${DATA_DIR}/overlay_island_destroyed.s
    ${DATA_DIR}/overlay_challenges.s
    ${DATA_DIR}/hint_infirmary.s
    ${DATA_DIR}/hint_goblin.s
    ${DATA_DIR}/hint_plunder.s
    ${DATA_DIR}/hint_damaged_core.s
    ${DATA_DIR}/charset.s
    ${DATA_DIR}/tilesheet.s
    ${DATA_DIR}/tilesheet_enemy_0.s
    ${DATA_DIR}/tilesheet_enemy_0_interior.s
    ${DATA_DIR}/tilesheet_interior.s
    ${DATA_DIR}/tilesheet_top.s
    ${DATA_DIR}/skyland_title_2_flattened.s
    ${DATA_DIR}/skyland_title_1_flattened.s
    ${DATA_DIR}/skyland_title_0_flattened.s
    ${DATA_DIR}/zone_image_0_flattened.s
    ${DATA_DIR}/zone_image_1_flattened.s
    ${DATA_DIR}/zone_image_2_flattened.s
    ${DATA_DIR}/zone_image_3_flattened.s
    ${DATA_DIR}/spritesheet.s
    ${DATA_DIR}/spritesheet_title_screen.s
    #${DATA_DIR}/shadows.s
    ${DATA_DIR}/sound_msg.s
    ${DATA_DIR}/sound_pop.s
    ${DATA_DIR}/sound_coin.s
    ${DATA_DIR}/sound_bell.s
    ${DATA_DIR}/sound_thud.s
    ${DATA_DIR}/sound_click.s
    ${DATA_DIR}/sound_heart.s
    ${DATA_DIR}/sound_dodge.s
    ${DATA_DIR}/sound_creak.s
    ${DATA_DIR}/sound_dropitem.s
    ${DATA_DIR}/sound_scroll.s
    ${DATA_DIR}/sound_laser1.s
    ${DATA_DIR}/sound_select.s
    ${DATA_DIR}/sound_openbag.s
    ${DATA_DIR}/sound_blaster.s
    ${DATA_DIR}/sound_tw_bell.s
    ${DATA_DIR}/sound_footstep1.s
    ${DATA_DIR}/sound_footstep2.s
    ${DATA_DIR}/sound_footstep3.s
    ${DATA_DIR}/sound_footstep4.s
    ${DATA_DIR}/sound_open_book.s
    ${DATA_DIR}/sound_explosion1.s
    ${DATA_DIR}/sound_typewriter.s
    ${DATA_DIR}/sound_explosion2.s)
else()
  set(SOURCES
    ${SOURCES}
    ${SOURCE_DIR}/platform/desktop/desktop_platform.cpp
    ${SOURCE_DIR}/platform/desktop/resource_path.cpp)
endif()


if(APPLE)
  add_executable(SkylandEngine MACOSX_BUNDLE ${SOURCES})


else()
  add_executable(SkylandEngine
    ${SOURCES})
endif()


set(IMAGE_INCLUDES "")
set(IMAGE_SPR_STUBS "")
set(IMAGE_TILE_STUBS "")
set(IMAGE_OVERLAY_STUBS "")

macro(add_spritesheet filename mw mh flatten)

  set(IMAGE_INCLUDES ${IMAGE_INCLUDES}
    "\n#include \"data/${filename}.h\"\n//")

  set(IMAGE_SPR_STUBS ${IMAGE_SPR_STUBS}
    "\n    TEXTURE_INFO(${filename}),\n//")

  compile_image(${filename} ${mw} ${mh} ${flatten} 4 NO)
endmacro()


macro(add_tilesheet filename mw mh flatten compr)
  if(${flatten} STREQUAL "YES")
    set(FLATTENED_SUFFIX "_flattened")
  else()
    set(FLATTENED_SUFFIX "")
  endif()

  set(IMAGE_INCLUDES ${IMAGE_INCLUDES}
    "\n#include \"data/${filename}${FLATTENED_SUFFIX}.h\"\n//")

  set(IMAGE_TILE_STUBS ${IMAGE_TILE_STUBS}
    "\n    TEXTURE_INFO(${filename}${FLATTENED_SUFFIX}),\n//")

  compile_image(${filename} ${mw} ${mh} ${flatten} 4 ${compr})
endmacro()


macro(add_overlay filename mw mh flatten)
  if(${flatten} STREQUAL "YES")
    set(FLATTENED_SUFFIX "_flattened")
  else()
    set(FLATTENED_SUFFIX "")
  endif()

  set(IMAGE_INCLUDES ${IMAGE_INCLUDES}
    "\n#include \"data/${filename}${FLATTENED_SUFFIX}.h\"\n//")

  set(IMAGE_OVERLAY_STUBS ${IMAGE_OVERLAY_STUBS}
    "\n    TEXTURE_INFO(${filename}${FLATTENED_SUFFIX}),\n//")

  compile_image(${filename} ${mw} ${mh} ${flatten} 4 NO)
endmacro()


function(compile_image filename mw mh flatten bpp compr)
  if(${compr} STREQUAL "YES")
    set(COMPRESSION "-Zlz77")
    message("${COMPRESSION}")
  else()
    set(${COMPRESSION} "")
  endif()
  if(${flatten} STREQUAL "YES")
    add_custom_command(OUTPUT ${SOURCE_DIR}/data/${filename}_flattened.s
      COMMAND python3 prep_image.py ${ROOT_DIR} ${filename}.png yes
      COMMAND ${DEVKITPRO}/tools/bin/grit tmp/${filename}_flattened.png -gB${bpp} -gTFF00FF ${COMPRESSION}
      COMMAND mv ${filename}_flattened.s ${SOURCE_DIR}/data/${filename}_flattened.s
      COMMAND mv ${filename}_flattened.h ${SOURCE_DIR}/data/${filename}_flattened.h
      DEPENDS ${IMAGE_DIR}/${filename}.png)

    add_custom_target(compile_image_${filename} DEPENDS ${SOURCE_DIR}/data/${filename}_flattened.s)

  elseif(${mw} STREQUAL "0" AND ${mh} STREQUAL "0")
    add_custom_command(OUTPUT ${SOURCE_DIR}/data/${filename}.s
      COMMAND python3 prep_image.py ${ROOT_DIR} ${filename}.png no
      COMMAND ${DEVKITPRO}/tools/bin/grit  tmp/${filename}.png -gB${bpp} -gTFF00FF ${COMPRESSION}
      COMMAND mv ${filename}.s ${SOURCE_DIR}/data/${filename}.s
      COMMAND mv ${filename}.h ${SOURCE_DIR}/data/${filename}.h
      DEPENDS ${IMAGE_DIR}/${filename}.png)

    add_custom_target(compile_image_${filename} DEPENDS ${SOURCE_DIR}/data/${filename}.s)

  else()
    add_custom_command(OUTPUT ${SOURCE_DIR}/data/${filename}.s
      COMMAND python3 prep_image.py ${ROOT_DIR} ${filename}.png no
      COMMAND ${DEVKITPRO}/tools/bin/grit tmp/${filename}.png -gB${bpp} -Mw ${mw} -Mh ${mh} -gTFF00FF ${COMPRESSION}
      COMMAND mv ${filename}.s ${SOURCE_DIR}/data/${filename}.s
      COMMAND mv ${filename}.h ${SOURCE_DIR}/data/${filename}.h
      DEPENDS ${IMAGE_DIR}/${filename}.png)

    add_custom_target(compile_image_${filename} DEPENDS ${SOURCE_DIR}/data/${filename}.s)

  endif()

  add_dependencies(SkylandEngine compile_image_${filename})
endfunction()


if(APPLE OR UNIX OR GAMEBOY_ADVANCE)

  set(SHARED_COMPILE_OPTIONS
    -g
    -Os
    -Wall
    -Werror
    -pedantic
    -nostdlib
    -ffast-math
    -fno-math-errno
    -Wdouble-promotion
    -fomit-frame-pointer
    -D__BLINDJUMP_ENABLE_LOGS)

  if(GAMEBOY_ADVANCE)
    set(SHARED_COMPILE_OPTIONS
      ${SHARED_COMPILE_OPTIONS}
      -fno-exceptions)
  endif()

elseif(WIN32)

  set(SHARED_COMPILE_OPTIONS
    /W4)

  include_directories(${ROOT_DIR}/external/SFML-2.5.1/include/)

  set(SFML_LIB_DIR ${ROOT_DIR}/external/SFML-2.5.1/lib/Release)

  target_link_libraries(SkylandEngine
    ${SFML_LIB_DIR}/sfml-graphics.lib
    ${SFML_LIB_DIR}/sfml-window.lib
    ${SFML_LIB_DIR}/sfml-system.lib
    ${SFML_LIB_DIR}/sfml-audio.lib
    ${SFML_LIB_DIR}/sfml-network.lib)

  add_custom_command(TARGET SkylandEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SFML_LIB_DIR}/sfml-graphics-2.dll"
        $<TARGET_FILE_DIR:SkylandEngine>)

  add_custom_command(TARGET SkylandEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SFML_LIB_DIR}/sfml-audio-2.dll"
        $<TARGET_FILE_DIR:SkylandEngine>)

  add_custom_command(TARGET SkylandEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SFML_LIB_DIR}/sfml-window-2.dll"
        $<TARGET_FILE_DIR:SkylandEngine>)

  add_custom_command(TARGET SkylandEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SFML_LIB_DIR}/sfml-network-2.dll"
        $<TARGET_FILE_DIR:SkylandEngine>)

  add_custom_command(TARGET SkylandEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SFML_LIB_DIR}/sfml-system-2.dll"
        $<TARGET_FILE_DIR:SkylandEngine>)

   add_custom_command(TARGET SkylandEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ROOT_DIR}/external/SFML-2.5.1/extlibs/bin/x64/openal32.dll"
        $<TARGET_FILE_DIR:SkylandEngine>)
endif()


if(GAMEBOY_ADVANCE)

  # I am setting CMAKE_AR in the toolchain file, but for some reason, the
  # variable no longer exists within the cmakelists... which is strange, because
  # other variables set in the toolchain file seem to be persistent...
  set(CMAKE_AR $ENV{DEVKITARM}/bin/arm-none-eabi-ar)

  add_library(ARM_ROUTINES ${SOURCE_DIR}/platform/gba/gba_arm_routines.cpp)

  target_compile_options(ARM_ROUTINES PRIVATE
    ${SHARED_COMPILE_OPTIONS})

  target_compile_options(ARM_ROUTINES PRIVATE
    -marm
    -mlong-calls
    -mthumb-interwork
    -mcpu=arm7tdmi
    -mtune=arm7tdmi)



  if(GBA_AUTOBUILD_CONF)
    configure_file(files.cpp.in ${SOURCE_DIR}/platform/gba/files.cpp)
  endif()

  if(GBA_AUTOBUILD_IMG)
    add_spritesheet(spritesheet 2 4 NO)
    add_spritesheet(spritesheet_title_screen 2 4 NO)
    add_tilesheet(tilesheet 2 2 NO NO)
    add_tilesheet(tilesheet_enemy_0 2 2 NO NO)
    add_tilesheet(tilesheet_enemy_0_interior 2 2 NO NO)
    add_tilesheet(tilesheet_interior 2 2 NO NO)
    add_tilesheet(tilesheet_top 4 3 NO NO)
    add_tilesheet(skyland_title_2 0 0 YES NO)
    add_tilesheet(skyland_title_1 0 0 YES NO)
    add_tilesheet(skyland_title_0 0 0 YES NO)
    add_tilesheet(zone_image_0 0 0 YES NO)
    add_tilesheet(zone_image_1 0 0 YES NO)
    add_tilesheet(zone_image_2 0 0 YES NO)
    add_tilesheet(zone_image_3 0 0 YES NO)
    add_overlay(overlay 0 0 NO NO)
    add_overlay(overlay_dialog 0 0 NO NO)
    add_overlay(charset 0 0 NO NO)
    add_overlay(overlay_world_map 0 0 NO NO)
    add_overlay(overlay_island_destroyed 0 0 NO NO)
    add_overlay(overlay_challenges 0 0 NO NO)
    add_overlay(hint_infirmary 0 0 NO NO)
    add_overlay(hint_goblin 0 0 NO NO)
    add_overlay(hint_plunder 0 0 NO NO)
    add_overlay(hint_damaged_core 0 0 NO NO)


    configure_file(images.cpp.in ${SOURCE_DIR}/platform/gba/images.cpp)
  endif()

  set(CMAKE_EXE_LINKER_FLAGS
    "-mthumb -mthumb-interwork -Wl,-Map,SkylandEngine.elf.map -specs=gba.specs"
    CACHE INTERNAL "" FORCE)

  target_link_libraries(SkylandEngine PRIVATE
    # ${DEVKITPRO}/libgba/lib/libgba.a
    ARM_ROUTINES)

  target_compile_options(SkylandEngine PRIVATE
    -mthumb
    -mthumb-interwork
    -mcpu=arm7tdmi
    -mtune=arm7tdmi)

  add_custom_command(TARGET SkylandEngine POST_BUILD
    COMMENT "building cartridge"
    COMMAND ${DEVKITARM}/bin/arm-none-eabi-objcopy -O binary SkylandEngine SkylandEngine.gba
    # NOTE: using DrillDozer's ROM id code for testing purposes, because many
    # emulator developers do not give you the option to manually override which
    # gamepacks support gpio rumble. SkylandEngine's actual ROM code was originally CBJE.
    COMMAND ${DEVKITPRO}/tools/bin/gbafix -tSkylandEngine -cV49E -r0 -mEB SkylandEngine.gba)

  target_compile_options(SkylandEngine PRIVATE
    -D__GBA__)

elseif(APPLE)
  target_link_libraries(SkylandEngine
    "-framework sfml-window -framework sfml-graphics -framework sfml-system -framework sfml-audio -framework sfml-network -framework Cocoa")
  set_target_properties(SkylandEngine
    PROPERTIES LINK_FLAGS "-Wl,-F/Library/Frameworks")

  include_directories("/usr/local/include/")

  add_custom_target(pkg ALL mkdir -p SkylandEngine.app/Contents/Resources
    COMMAND mkdir -p SkylandEngine.app/Contents/Frameworks
    COMMAND cp -r ${ROOT_DIR}/images/ SkylandEngine.app/Contents/images/
    COMMAND cp -r ${ROOT_DIR}/shaders/ SkylandEngine.app/Contents/shaders/
    COMMAND cp -r ${ROOT_DIR}/sounds/ SkylandEngine.app/Contents/sounds/
    COMMAND cp -r ${ROOT_DIR}/scripts/ SkylandEngine.app/Contents/scripts/
    COMMAND cp -r ${ROOT_DIR}/strings/ SkylandEngine.app/Contents/strings/
    # COMMAND cp macOS/icon.icns SkylandEngine.app/Contents/Resources
    # COMMAND cp -r ${SFML_DIR}/lib/* SkylandEngine.app/Contents/Frameworks
    # COMMAND cp -r ${SFML_DIR}/extlibs/libs-osx/Frameworks/* SkylandEngine.app/Contents/Frameworks
    )

elseif(UNIX)

  target_link_libraries(SkylandEngine
    -lpthread
    -lsfml-window
    -lsfml-graphics
    -lsfml-system
    -lsfml-audio
    -lsfml-network)

endif()



target_compile_options(SkylandEngine PRIVATE
  ${SHARED_COMPILE_OPTIONS})



file(GLOB_RECURSE SOURCES "${SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE HEADERS "${SOURCE_DIR}/*.hpp")

add_custom_target(clang-format
  COMMAND clang-format --style=file -i ${SOURCES} ${HEADERS}
  COMMENT "Running clang-format"
  VERBATIM)
