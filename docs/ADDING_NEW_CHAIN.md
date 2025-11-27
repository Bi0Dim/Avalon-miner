# Добавление новой AuxPoW монеты

Пошаговое руководство по добавлению поддержки новой merged mining монеты.

## Шаг 1: Исследование параметров

Соберите следующую информацию о монете:

| Параметр | Описание | Где найти |
|----------|----------|-----------|
| `name` | Название (lowercase) | Официальный сайт |
| `ticker` | Тикер валюты | CoinMarketCap, CoinGecko |
| `chain_id` | ID для AuxPoW | Исходный код (chainparams.cpp) |
| `start_height` | Высота активации AuxPoW | Исходный код или документация |
| `network_magic` | 4 байта магии сети | chainparams.cpp |
| `default_port` | P2P порт | chainparams.cpp |
| `rpc_port` | RPC порт | Документация |
| `target_spacing` | Время блока (секунды) | chainparams.cpp |
| `adjustment_interval` | Интервал пересчёта сложности | chainparams.cpp |
| `pow_limit_bits` | Минимальная сложность (nBits) | chainparams.cpp |
| `consensus_type` | Тип консенсуса | Анализ кода |

## Шаг 2: Определение типа консенсуса

Выберите подходящий тип:

```cpp
enum class ConsensusType {
    PURE_AUXPOW,        // Стандартный AuxPoW
    AUXPOW_CHAINLOCK,   // + ChainLock (Syscoin)
    AUXPOW_HYBRID_POS,  // + Proof-of-Stake (Emercoin)
    AUXPOW_HYBRID_BPOS, // + Bonded PoS (Elastos)
    AUXPOW_DECOR,       // + DECOR+ (RSK)
    AUXPOW_DAG,         // + DAG (Hathor)
};
```

Если монета не подходит ни под один тип:
1. Добавьте новый тип в `consensus_type.hpp`
2. Обновите `to_string()` и `supports_standard_auxpow()`
3. При необходимости расширьте валидатор

## Шаг 3: Добавление в реестр

Откройте `src/core/chain/chain_registry.cpp` и добавьте параметры в функцию `init_builtin_chains()`:

```cpp
// NewCoin (chain_id = X)
{
    ChainParams params;
    params.name = "newcoin";
    params.ticker = "NEW";
    params.consensus_type = ConsensusType::PURE_AUXPOW;
    
    // AuxPoW параметры
    params.auxpow.chain_id = 42;
    params.auxpow.start_height = 100000;
    params.auxpow.magic_bytes = {0xfa, 0xbe, 0x6d, 0x6d};  // Стандартный
    params.auxpow.version_flag = 0x00620102;
    
    // Сложность
    params.difficulty.target_spacing = 120;  // 2 минуты
    params.difficulty.adjustment_interval = 2016;
    params.difficulty.pow_limit_bits = 0x1e00ffff;
    
    // Награды
    params.rewards.initial_reward = 5000000000;  // 50 монет
    params.rewards.halving_interval = 210000;
    params.rewards.miner_share = 1.0;
    params.rewards.coinbase_maturity = 100;
    
    // Сеть
    params.mainnet.magic = {0x12, 0x34, 0x56, 0x78};
    params.mainnet.default_port = 12345;
    params.mainnet.rpc_port = 12346;
    params.mainnet.dns_seeds = {"seed.newcoin.org"};
    
    register_chain(std::move(params));
}
```

## Шаг 4: Добавление удобной функции

В `chain_registry.hpp` добавьте:

```cpp
[[nodiscard]] const ChainParams& newcoin_params();
```

В `chain_registry.cpp` реализуйте:

```cpp
const ChainParams& newcoin_params() {
    const auto* params = ChainRegistry::instance().get_by_name("newcoin");
    if (!params) {
        throw std::runtime_error("NewCoin params not found");
    }
    return *params;
}
```

## Шаг 5: Добавление тестов

Создайте или обновите тесты в `tests/core/test_chain_params.cpp`:

```cpp
TEST(ChainParamsTest, NewCoinParams) {
    const auto& coin = newcoin_params();
    
    EXPECT_EQ(coin.name, "newcoin");
    EXPECT_EQ(coin.ticker, "NEW");
    EXPECT_EQ(coin.auxpow.chain_id, 42);
    EXPECT_EQ(coin.difficulty.target_spacing, 120);
    EXPECT_EQ(coin.mainnet.default_port, 12345);
}

TEST(ChainParamsTest, NewCoinAuxPowActivation) {
    const auto& coin = newcoin_params();
    
    EXPECT_FALSE(coin.is_auxpow_active(99999));
    EXPECT_TRUE(coin.is_auxpow_active(100000));
}
```

## Шаг 6: Обновление документации

Добавьте монету в таблицу в `docs/UNIVERSAL_CORE.md`:

```markdown
| NewCoin | 42 | 12345 | 120s | PURE_AUXPOW |
```

## Шаг 7: Сборка и тестирование

```bash
cd build
cmake ..
make -j$(nproc)
./tests/quaxis_tests --gtest_filter="*NewCoin*"
```

## Пример: Добавление Dogecoin (если бы поддерживал AuxPoW)

```cpp
// Dogecoin (гипотетический пример)
{
    ChainParams params;
    params.name = "dogecoin";
    params.ticker = "DOGE";
    params.consensus_type = ConsensusType::PURE_AUXPOW;
    
    params.auxpow.chain_id = 99;
    params.auxpow.start_height = 371337;
    
    params.difficulty.target_spacing = 60;  // 1 минута
    params.difficulty.adjustment_interval = 240;
    params.difficulty.pow_limit_bits = 0x1e0ffff0;
    
    params.rewards.initial_reward = 1000000000000;  // 10000 DOGE
    params.rewards.halving_interval = 0;  // Нет halving
    params.rewards.miner_share = 1.0;
    
    params.mainnet.magic = {0xc0, 0xc0, 0xc0, 0xc0};
    params.mainnet.default_port = 22556;
    params.mainnet.rpc_port = 22555;
    
    register_chain(std::move(params));
}
```

## Проверка параметров

### Где найти chain_id

В исходном коде монеты ищите:

```cpp
// Bitcoin Core форки
nAuxpowChainId = X;
// или
static const int AUXPOW_CHAIN_ID = X;
```

### Где найти magic bytes

```cpp
// chainparams.cpp
pchMessageStart[0] = 0xXX;
pchMessageStart[1] = 0xXX;
pchMessageStart[2] = 0xXX;
pchMessageStart[3] = 0xXX;
```

### Проверка AuxPoW совместимости

Монета должна:
1. Использовать SHA256d (не scrypt, не X11)
2. Иметь стандартный формат coinbase commitment
3. Поддерживать `getauxblock` или `createauxblock` RPC

## Типичные ошибки

1. **Неверный chain_id** — блоки будут отклоняться другими нодами
2. **Неверная высота активации** — AuxPoW не будет работать до этой высоты
3. **Забытый тип консенсуса** — использование `PURE_AUXPOW` для сложных монет

## Поддержка

При возникновении вопросов:
1. Изучите исходный код референсной ноды монеты
2. Проверьте документацию merged mining
3. Откройте issue в репозитории
