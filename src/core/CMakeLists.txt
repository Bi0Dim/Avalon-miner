# =============================================================================
# Quaxis Solo Miner - Core модуль
# =============================================================================
# Базовые типы, константы, утилиты и универсальное ядро AuxPoW
# =============================================================================

# Базовые типы (без зависимости от crypto)
add_library(quaxis_core STATIC
    byte_order.cpp
    config.cpp
    mtp_calculator.cpp
    
    # Chain - параметры блокчейнов (не зависят от crypto)
    chain/chain_registry.cpp
    
    # Serialization - сериализация данных
    serialization/stream.cpp
    
    # Primitives - базовые структуры (без хеширования)
    primitives/uint256.cpp
)

target_include_directories(quaxis_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(quaxis_core PUBLIC
    tomlplusplus::tomlplusplus
)

# Алиас для удобства использования
add_library(quaxis::core ALIAS quaxis_core)
