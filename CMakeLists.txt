# =============================================================================
# Quaxis Solo Miner - Главный CMake файл
# =============================================================================
# Высокооптимизированный соло-майнер Bitcoin для ASIC Avalon 1126 Pro
# Язык: C++23
# =============================================================================

cmake_minimum_required(VERSION 3.25)

project(QuaxisSoloMiner
    VERSION 1.0.0
    DESCRIPTION "Высокооптимизированный соло-майнер Bitcoin для ASIC Avalon 1126 Pro"
    LANGUAGES CXX
)

# =============================================================================
# Настройки компилятора C++23
# =============================================================================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Оптимизации для Release сборки
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address,undefined")

# Предупреждения
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wconversion
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wformat=2
)

# =============================================================================
# Опции сборки
# =============================================================================
option(QUAXIS_ENABLE_SHANI "Включить SHA-NI оптимизации (Intel/AMD)" ON)
option(QUAXIS_ENABLE_TESTS "Включить сборку тестов" ON)
option(QUAXIS_ENABLE_BENCHMARKS "Включить сборку бенчмарков" ON)

# =============================================================================
# Проверка поддержки SHA-NI
# =============================================================================
if(QUAXIS_ENABLE_SHANI)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-msha" COMPILER_SUPPORTS_SHA)
    if(COMPILER_SUPPORTS_SHA)
        message(STATUS "SHA-NI поддерживается компилятором")
        add_compile_definitions(QUAXIS_HAS_SHANI)
    else()
        message(WARNING "SHA-NI не поддерживается компилятором, используется generic реализация")
    endif()
endif()

# =============================================================================
# Внешние зависимости
# =============================================================================
find_package(Threads REQUIRED)

# TOML++ для парсинга конфигурации (header-only)
include(FetchContent)
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

# libcurl для HTTP запросов к Bitcoin Core RPC
find_package(CURL REQUIRED)

# =============================================================================
# Подключение модулей проекта
# =============================================================================
add_subdirectory(src/core)
add_subdirectory(src/crypto)
add_subdirectory(src/bitcoin)
add_subdirectory(src/mining)
add_subdirectory(src/network)
add_subdirectory(src/monitoring)
add_subdirectory(src/relay)
add_subdirectory(src)

# =============================================================================
# Тесты
# =============================================================================
if(QUAXIS_ENABLE_TESTS)
    enable_testing()
    
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
    
    add_subdirectory(tests)
endif()

# =============================================================================
# Информация о сборке
# =============================================================================
message(STATUS "")
message(STATUS "=== Quaxis Solo Miner v${PROJECT_VERSION} ===")
message(STATUS "Компилятор: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Стандарт C++: ${CMAKE_CXX_STANDARD}")
message(STATUS "SHA-NI: ${QUAXIS_ENABLE_SHANI}")
message(STATUS "Тесты: ${QUAXIS_ENABLE_TESTS}")
message(STATUS "Тип сборки: ${CMAKE_BUILD_TYPE}")
message(STATUS "")
