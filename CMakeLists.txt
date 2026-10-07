cmake_minimum_required(VERSION 3.16)
project(PET_simulation)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Путь к Geant4: на Windows по умолчанию локальная установка,
# переопределяется через -DGeant4_DIR=... (на Linux обычно не нужен)
if(WIN32 AND NOT Geant4_DIR AND NOT DEFINED ENV{Geant4_DIR})
    set(Geant4_DIR "C:/Geant4/geant4-v11.3.0-inst/lib/cmake/Geant4")
endif()
find_package(Geant4 REQUIRED ui_all vis_all)

# src/ и include/ — кольцевой режим, src/tof/ и include/tof/ — режим tof
file(GLOB_RECURSE sources CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.cc)
file(GLOB_RECURSE headers CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/include/*.hh)

add_executable(PET_simulation ${sources} ${headers})
target_include_directories(PET_simulation PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/tof)
target_link_libraries(PET_simulation PRIVATE ${Geant4_LIBRARIES})

# Копирование макросов рядом с исполняемым файлом (build/Release/ для MSVC,
# build/ для Makefile/Ninja) — main.cc ищет run.mac в текущей папке,
# макросы режима tof лежат в подпапке tof/
file(GLOB macros ${PROJECT_SOURCE_DIR}/macros/*.mac)
add_custom_command(TARGET PET_simulation POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${macros} $<TARGET_FILE_DIR:PET_simulation>
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/macros/tof $<TARGET_FILE_DIR:PET_simulation>/tof
    COMMENT "Copying macros to output directory")

message(STATUS " PET Simulation configured successfully!")
