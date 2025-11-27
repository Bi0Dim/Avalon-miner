# Universal AuxPoW Core — Архитектура

Универсальное ядро для merged mining всех AuxPoW монет.

## Обзор

Universal AuxPoW Core предоставляет единую кодовую базу для работы с любой монетой, поддерживающей AuxPoW (Auxiliary Proof of Work) — технологию merged mining с Bitcoin.

### Ключевые преимущества

- **80-90% переиспользования кода** между всеми 12+ AuxPoW монетами
- **Добавление новой монеты = ~50 строк** (только параметры)
- **0 дублирования** логики валидации и синхронизации
- **Type-safe C++23** с концептами и `std::expected`

## Структура модулей

```
src/core/
├── primitives/          # Базовые структуры Bitcoin
│   ├── uint256.hpp      # 256-битное целое число
│   ├── block_header.hpp # 80-байтный заголовок блока
│   ├── merkle.hpp       # Merkle tree функции
│   └── auxpow.hpp       # AuxPoW структура и функции
│
├── chain/               # Параметры блокчейнов
│   ├── consensus_type.hpp   # Типы консенсуса (enum)
│   ├── chain_params.hpp     # Структура параметров
│   └── chain_registry.hpp   # Реестр всех монет
│
├── serialization/       # Сериализация данных
│   └── stream.hpp       # ReadStream / WriteStream
│
├── validation/          # Валидация блоков
│   ├── pow_validator.hpp    # Проверка PoW
│   └── auxpow_validator.hpp # Проверка AuxPoW
│
└── sync/                # Синхронизация
    ├── headers_store.hpp    # Хранилище заголовков
    └── headers_sync.hpp     # Синхронизатор
```

## Типы консенсуса

| Тип | Описание | Монеты |
|-----|----------|--------|
| `PURE_AUXPOW` | Чистый AuxPoW | Namecoin, VCash, Fractal, Myriad, Huntercoin, Unobtanium, Terracoin |
| `AUXPOW_CHAINLOCK` | AuxPoW + мгновенная финализация | Syscoin |
| `AUXPOW_HYBRID_POS` | AuxPoW + Proof-of-Stake | Emercoin |
| `AUXPOW_HYBRID_BPOS` | AuxPoW + Bonded PoS (35% rewards) | Elastos |
| `AUXPOW_DECOR` | AuxPoW + DECOR+ протокол | RSK |
| `AUXPOW_DAG` | AuxPoW + DAG структура | Hathor |

## Параметры монет

### ChainParams структура

```cpp
struct ChainParams {
    // Идентификация
    std::string name;           // "namecoin"
    std::string ticker;         // "NMC"
    Hash256 genesis_hash;       // Genesis block hash
    
    // Консенсус
    ConsensusType consensus_type;
    AuxPowParams auxpow;
    DifficultyParams difficulty;
    RewardParams rewards;
    
    // Сеть
    NetworkParams mainnet;
    std::optional<NetworkParams> testnet;
};
```

### AuxPowParams

```cpp
struct AuxPowParams {
    uint32_t chain_id;                   // ID для Merkle tree slot
    std::array<uint8_t, 4> magic_bytes;  // 0xfabe6d6d ("mm")
    uint32_t start_height;               // Высота активации
    uint32_t version_flag;               // Флаг версии блока
};
```

## Таблица параметров

**Note:** Bitcoin is the parent chain and does not have a chain_id in the AuxPoW context.
Stacks (STX) is NOT supported — PoX is not AuxPoW.

| Монета | chain_id | port | block_time | consensus |
|--------|----------|------|------------|-----------|
| Bitcoin | - (parent) | 8333 | 600s | - |
| Namecoin | 1 | 8334 | 600s | PURE_AUXPOW |
| VCash | 2 | 5765 | 200s | PURE_AUXPOW |
| Myriad | 3 | 10888 | 60s | PURE_AUXPOW |
| Terracoin | 5 | 13332 | 120s | PURE_AUXPOW |
| Emercoin | 6 | 6661 | 600s | AUXPOW_HYBRID_POS |
| Unobtanium | 8 | 65534 | 180s | PURE_AUXPOW |
| Fractal | 10 | 8332 | 600s | PURE_AUXPOW |
| Hathor | 11 | 8000 | var | AUXPOW_DAG |
| Huntercoin | 12 | 8398 | 60s | PURE_AUXPOW |
| Elastos | 13 | 20866 | 120s | AUXPOW_HYBRID_BPOS |
| RSK | 30 | 4444 | 30s | AUXPOW_DECOR |
| Syscoin | 57 | 8369 | 150s | AUXPOW_CHAINLOCK |

**Important:** Each chain must have a unique chain_id to avoid Merkle tree slot collisions.

## Использование

### Получение параметров монеты

```cpp
#include "core/chain/chain_registry.hpp"

// Через реестр
auto& registry = ChainRegistry::instance();
const auto* nmc = registry.get_by_name("namecoin");
const auto* sys = registry.get_by_ticker("SYS");
const auto* id1 = registry.get_by_chain_id(1);

// Через удобные функции
const auto& btc = bitcoin_params();
const auto& nmc = namecoin_params();
```

### Валидация AuxPoW

```cpp
#include "core/validation/auxpow_validator.hpp"

const auto& params = namecoin_params();
AuxPowValidator validator(params);

auto result = validator.validate(auxpow, aux_hash, height);
if (result) {
    // AuxPoW валиден
} else {
    std::cerr << result.error_message << std::endl;
}
```

### Синхронизация заголовков

```cpp
#include "core/sync/headers_sync.hpp"

const auto& params = namecoin_params();
HeadersSync sync(params);

sync.on_new_block([](const BlockHeader& header, uint32_t height) {
    std::cout << "New block at height " << height << std::endl;
});

sync.start();
```

## Архитектурные решения

### Разделение типов и хеширования

Базовые типы (`Hash256`, `Bytes`, `Result<T>`) определены в `core/types.hpp` и не зависят от crypto модуля. Это позволяет избежать циклических зависимостей.

Примитивы, требующие хеширования (`BlockHeader`, `MerkleTree`, `AuxPow`), вынесены в отдельную библиотеку `quaxis_primitives`, которая зависит от `quaxis_crypto`.

### Порядок сборки

```
quaxis_core → quaxis_crypto → quaxis_primitives → quaxis_validation → quaxis_sync
```

### Thread-safety

Все публичные методы `HeadersStore` и `HeadersSync` потокобезопасны благодаря использованию `std::mutex`.

## Расширение

Для добавления поддержки нового консенсуса:

1. Добавьте новый тип в `ConsensusType` enum
2. Обновите функции `to_string()` и `supports_standard_auxpow()`
3. При необходимости расширьте `AuxPowValidator` для специфичной логики

## Ссылки

- [ADDING_NEW_CHAIN.md](ADDING_NEW_CHAIN.md) — как добавить новую монету
- [Bitcoin Wiki: Merged Mining](https://en.bitcoin.it/wiki/Merged_mining_specification)
- [Namecoin AuxPoW](https://www.namecoin.org/docs/auxpow/)
