# =============================================================================
# Quaxis Solo Miner - Core Primitives модуль
# =============================================================================
# Примитивы Bitcoin с зависимостью от crypto
# =============================================================================

add_library(quaxis_primitives STATIC
    block_header.cpp
    merkle.cpp
    auxpow.cpp
    ../template_generator.cpp
)

target_include_directories(quaxis_primitives PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(quaxis_primitives PUBLIC
    quaxis::core
    quaxis::crypto
)

# Алиас для удобства использования
add_library(quaxis::primitives ALIAS quaxis_primitives)
