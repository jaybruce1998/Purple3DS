cmake_minimum_required(VERSION 3.10)

# Force cross-compilation before project()
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(DEVKITARM $ENV{DEVKITARM})
if(NOT DEVKITARM)
    set(DEVKITARM "C:/devkitPro/devkitARM")
endif()

# Force the cross-compilers
set(CMAKE_C_COMPILER "${DEVKITARM}/bin/arm-none-eabi-gcc.exe")
set(CMAKE_CXX_COMPILER "${DEVKITARM}/bin/arm-none-eabi-g++.exe")
set(CMAKE_OBJCOPY "${DEVKITARM}/bin/arm-none-eabi-objcopy.exe")

# Don't let CMake test the compiler
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)

# Bypass compiler tests
set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_CXX_COMPILER_WORKS TRUE)

project(PokemonPurple3DS C CXX)

# Force include paths after project (Windows paths)
include_directories("C:/devkitPro/libctru/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

set(CMAKE_C_FLAGS "-Wall -O3 -mword-relocations -fomit-frame-pointer -ffast-math -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fexceptions -std=gnu++17")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DARM11 -D__3DS__ -IC:/devkitPro/libctru/include -I${CMAKE_CURRENT_SOURCE_DIR}/include")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DARM11 -D__3DS__ -IC:/devkitPro/libctru/include -I${CMAKE_CURRENT_SOURCE_DIR}/include")

set(CMAKE_EXE_LINKER_FLAGS "-specs=C:/devkitPro/devkitARM/arm-none-eabi/lib/3dsx.specs -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lctru -lm -lcitro2d -lcitro3d -LC:/devkitPro/libctru/lib")

# Collect only the files needed for main_3ds.cpp
set(SOURCES
    source/main_3ds.cpp
    source/map_data.cpp
    source/map_definitions.cpp
    source/MAP_3DS_bgr.c
    source/all_glyphs_data.c
    source/all_tiles_data.c
    source/all_sprites_complete.cpp
    source/initialize_sprites.cpp
    source/sprites_map.cpp
    source/global_gui.cpp
    source/all_battlers_proper.c
    source/BattleState.cpp
    source/Blocker.cpp
    source/Battler.cpp
    source/Effect.cpp
    source/Encounter.cpp
    source/Evolution.cpp
    source/FlyLocation.cpp
    source/Giver.cpp
    source/Item.cpp
    source/LevelUpMove.cpp
    source/MartItem.cpp
    source/Monster.cpp
    source/Move.cpp
    source/Npc.cpp
    source/NpcStrings.cpp
    source/Gui.cpp
    source/Player.cpp
    source/PokeMap.cpp
    source/TmLearnsets.cpp
    source/Trader.cpp
    source/Trainer.cpp
    source/Types.cpp
    source/Warp.cpp
    source/WorldObject.cpp
)

# Create executable
add_executable(main_3ds.elf ${SOURCES})

# Link libraries
target_link_libraries(main_3ds.elf ctru citro2d citro3d)

# Create 3DSX file
add_custom_command(TARGET main_3ds.elf POST_BUILD
    COMMAND C:/devkitPro/tools/bin/3dsxtool.exe ${CMAKE_BINARY_DIR}/main_3ds.elf ${CMAKE_BINARY_DIR}/main_3ds.3dsx
    COMMENT "Creating 3DSX file"
)
