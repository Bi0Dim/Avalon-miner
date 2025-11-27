# =============================================================================
# Quaxis Solo Miner - Core Validation модуль
# =============================================================================
# Валидация PoW и AuxPoW
# =============================================================================

add_library(quaxis_validation STATIC
    pow_validator.cpp
    auxpow_validator.cpp
)

target_include_directories(quaxis_validation PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(quaxis_validation PUBLIC
    quaxis::core
    quaxis::primitives
)

# Алиас для удобства использования
add_library(quaxis::validation ALIAS quaxis_validation)
