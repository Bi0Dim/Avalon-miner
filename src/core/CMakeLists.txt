# =============================================================================
# Quaxis Solo Miner - Core модуль
# =============================================================================
# Базовые типы, константы и утилиты
# =============================================================================

add_library(quaxis_core STATIC
    byte_order.cpp
    config.cpp
)

target_include_directories(quaxis_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(quaxis_core PUBLIC
    tomlplusplus::tomlplusplus
)

# Алиас для удобства использования
add_library(quaxis::core ALIAS quaxis_core)
