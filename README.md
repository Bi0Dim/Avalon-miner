# Quaxis Solo Miner

Высокооптимизированный соло-майнер Bitcoin для ASIC Avalon 1126 Pro с интеграцией в модифицированный Bitcoin Core.

**Язык: C++23** — использует все современные возможности: std::expected, std::format, concepts, ranges.

**Все комментарии в коде на русском языке.**

## Архитектура

```
┌──────────────────────────────────────────────────────────────────────┐
│                         BITCOIN NETWORK                               │
└────────────────────────────────┬─────────────────────────────────────┘
                                 │
                                 ▼
┌──────────────────────────────────────────────────────────────────────┐
│                      МОДИФИЦИРОВАННЫЙ BITCOIN CORE                    │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────┐  │
│  │  Validation     │  │  Spy Mining     │  │  Shared Memory      │  │
│  │                 │──│  Callback       │──│  Bridge             │  │
│  │  (+ priority)   │  │                 │  │  /quaxis_block      │  │
│  └─────────────────┘  └─────────────────┘  └──────────┬──────────┘  │
└───────────────────────────────────────────────────────┼──────────────┘
                                                        │ ~100 нс
                                                        ▼
┌──────────────────────────────────────────────────────────────────────┐
│                      QUAXIS SOLO MINER SERVER                         │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────┐  │
│  │  SHM Subscriber │  │  Job Manager    │  │  Template Cache     │  │
│  │  (spin-wait)    │──│  (extranonce)   │──│  (precompute N+1)   │  │
│  └─────────────────┘  └────────┬────────┘  └─────────────────────┘  │
│                                │                                      │
│  ┌─────────────────────────────▼──────────────────────────────────┐  │
│  │                      TCP SERVER (port 3333)                     │  │
│  │              Бинарный протокол 48 байт / 8 байт                 │  │
│  └──────────────────────────────┬─────────────────────────────────┘  │
└─────────────────────────────────┼────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────┐
│                      AVALON 1126 PRO (114 чипов)                      │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────┐  │
│  │  Network Client │──│  SPI Controller │──│  A1126 ASIC Chips   │  │
│  │  (TCP)          │  │                 │  │  (SHA256 x 114)     │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────────┘  │
└──────────────────────────────────────────────────────────────────────┘
```

## Оптимизации (17 штук)

### Вычислительные (+3.8%)
1. **Готовый midstate** — ASIC делает 1 SHA256 вместо 2 (+3.3%)
2. **Пустые блоки** — только coinbase, нет merkle tree (+0.5%)
3. **SHA-NI ускорение** — аппаратные SHA256 инструкции (~100 нс)
4. **Предвычисление coinbase midstate** — кешируется при новом блоке

### Уникальность хешей
5. **Coinbase tag "quaxis"** — 6-байтная метка в scriptsig
6. **MTP+1 timestamp** — минимально допустимый timestamp
7. **Уникальный payout address** — P2WPKH формат (bc1q...)
8. **Extranonce 6 байт** — 2^48 вариантов = 589 лет

### Минимизация латентности (-150-1500 мс)
9. **Shared Memory** — латентность ~100 нс вместо ZMQ 1-3 мс
10. **Spy Mining** — майнинг до полной валидации блока
11. **Callback в ProcessNewBlockHeaders** — мгновенное уведомление
12. **Приоритет block messages** — блоки обрабатываются первыми
13. **Предвычисление шаблонов** — готовим N+1 пока майним N

### Протокол связи
14. **Бинарный протокол 48 байт** — вместо Stratum JSON 500-1000 байт
15. **Минимальная логика на ASIC** — только SHA256 + compare
16. **Маленькая очередь** — 50-100 заданий

### Сетевые
17. **Оптимизированный bitcoin.conf** — maxconnections, dbcache, compact blocks

## Быстрый старт

```bash
# Установка зависимостей
./scripts/install-dependencies.sh

# Сборка
./scripts/build.sh

# Настройка Bitcoin Core с патчами
./scripts/setup-bitcoin-core.sh --clone /opt/bitcoin

# Настройка конфигурации
cp config/quaxis.toml.example ~/.config/quaxis/quaxis.toml
# Отредактируйте: payout_address, rpc_password

# Запуск
./scripts/run-server.sh
```

## Требования

### Сервер
- Linux (Ubuntu 22.04+, Debian 12+)
- GCC 13+ или Clang 17+ (C++23)
- CPU с SHA-NI (Intel Skylake+, AMD Zen+)
- 4+ GB RAM
- SSD для Bitcoin Core

### ASIC
- Avalon 1126 Pro (90+ TH/s)
- Ethernet подключение

## Структура проекта

```
Avalon-miner/
├── CMakeLists.txt              # Главный CMake файл
├── src/
│   ├── core/                   # Базовые типы, константы, конфигурация
│   ├── crypto/                 # SHA256 с SHA-NI поддержкой
│   ├── bitcoin/                # Блоки, coinbase, RPC, Shared Memory
│   ├── mining/                 # Задания, валидация, кеш шаблонов
│   ├── network/                # TCP сервер, бинарный протокол
│   ├── fallback/               # Система fallback (SHM → ZMQ → Stratum)
│   ├── log/                    # Терминальный вывод статуса
│   ├── merged/                 # Merged mining (AuxPoW) для 12 chains
│   ├── relay/                  # FIBRE/UDP relay
│   └── main.cpp                # Точка входа
├── firmware/                   # Прошивка ASIC (C)
│   ├── include/                # Заголовочные файлы
│   └── src/                    # Реализация
├── bitcoin-core-patches/       # Патчи для Bitcoin Core
├── config/                     # Примеры конфигурации
├── docs/                       # Документация
├── scripts/                    # Скрипты установки и запуска
└── tests/                      # Тесты и бенчмарки
```

## Runtime Status Output

Quaxis Solo Miner предоставляет терминальный вывод статуса в реальном времени:

```
═══════════════════════════════════════════════════════════════════
                    QUAXIS SOLO MINER v1.0.0
═══════════════════════════════════════════════════════════════════

Uptime: 01:23:45

Bitcoin:
  Height: 850123 (tip age: 45s)
  Connection: CONNECTED

Hashrate:
  90.0 TH/s (rated)

ASIC:
  Connected: 3

Source: SHM (Primary)

SHM:
  Spin Wait: active
  Adaptive: enabled
  CPU Usage: 2.5%

Merged Mining Chains:
  • namecoin (5 blocks)
  • syscoin (3 blocks)
  • rsk (1 block)
  • fractal
  • elastos

Recent Events:
  10:15:30 [NEW_BLOCK] New Bitcoin block at height 850123
  10:14:22 [AUX_FOUND] Found block at height 600123 (namecoin)
  10:12:05 [SUBMIT_OK] Share/block submitted (bitcoin)
───────────────────────────────────────────────────────────────────
```

## Stacks (STX) Removal

**Note:** Stacks (STX) is NOT supported and has been explicitly removed from this project. Stacks uses Proof of Transfer (PoX), which is fundamentally different from Auxiliary Proof of Work (AuxPoW) used for merged mining. PoX requires STX stacking and is not compatible with traditional Bitcoin mining.

## Fallback System

The miner includes a robust fallback system:
- **Primary**: Shared Memory (SHM) - ~100ns latency
- **Fallback 1**: ZMQ - ~1-3ms latency  
- **Fallback 2**: Stratum pool - last resort

## Adaptive Spin Wait

SHM can use adaptive spin-wait to optimize CPU usage while maintaining low latency.

## Документация

- [ARCHITECTURE.md](docs/ARCHITECTURE.md) — описание архитектуры
- [PROTOCOL.md](docs/PROTOCOL.md) — спецификация бинарного протокола
- [OPTIMIZATIONS.md](docs/OPTIMIZATIONS.md) — описание всех 17 оптимизаций
- [BUILDING.md](docs/BUILDING.md) — инструкции по сборке
- [CONFIGURATION.md](docs/CONFIGURATION.md) — настройка системы

## Бинарный протокол

### Задание (Server → ASIC): 48 байт
```
midstate[32] + header_tail[12] + job_id[4]
```

### Ответ (ASIC → Server): 8 байт
```
job_id[4] + nonce[4]
```

## Структура Coinbase (110 байт)

```
ЧАСТЬ 1 (64 байта) — КОНСТАНТА ДЛЯ MIDSTATE:
[version][input_count][prev_tx][index][scriptsig_len][height]["quaxis"][padding]

ЧАСТЬ 2 (46 байт) — СОДЕРЖИТ EXTRANONCE:
[extranonce][sequence][outputs][locktime]
```

## Лицензия

MIT License. См. [LICENSE](LICENSE).
