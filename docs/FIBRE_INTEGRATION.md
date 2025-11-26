# Интеграция FIBRE UDP Relay

## Что такое FIBRE?

FIBRE (Fast Internet Bitcoin Relay Engine) — это протокол для сверхбыстрого распространения Bitcoin блоков через сеть relay серверов. В отличие от стандартного Bitcoin P2P протокола, FIBRE использует:

- **UDP** вместо TCP для минимизации латентности
- **Forward Error Correction (FEC)** для надёжной передачи данных
- **Глобальную сеть relay серверов** для быстрого распространения

### Преимущества FIBRE

| Характеристика | Bitcoin P2P | FIBRE UDP |
|----------------|-------------|-----------|
| Латентность | 500-2000 мс | 100-300 мс |
| Протокол | TCP | UDP |
| Надёжность | Ретрансмиссии | FEC |
| Экономия времени | - | 400-1700 мс |
| Прирост эффективности | - | +0.5-1.5% |

## Архитектура

```
                      BITCOIN NETWORK
                            │
            ┌───────────────┴───────────────┐
            │                               │
            ▼                               ▼
 ┌─────────────────────┐         ┌─────────────────────┐
 │   FIBRE UDP PEERS   │         │   BITCOIN P2P       │
 │   (100-300 мс)      │         │   (500-2000 мс)     │
 └──────────┬──────────┘         └──────────┬──────────┘
            │                               │
            ▼                               ▼
 ┌─────────────────────┐         ┌─────────────────────┐
 │ QUAXIS UDP RELAY    │         │   BITCOIN CORE      │
 │ (RelayManager)      │         │   (с нашими патчами)│
 └──────────┬──────────┘         └──────────┬──────────┘
            │                               │
            │ Callback                      │ Shared Memory
            │ (первый!)                     │ (подтверждение)
            │                               │
            └───────────────┬───────────────┘
                            │
                            ▼
            ┌───────────────────────────────┐
            │      QUAXIS MINER SERVER      │
            │                               │
            │  Получает header из ПЕРВОГО   │
            │  источника — начинает майнить │
            └───────────────────────────────┘
```

## Компоненты модуля

### Структура файлов

```
src/relay/
├── CMakeLists.txt           # Конфигурация сборки
├── fec_decoder.hpp/cpp      # FEC декодер (Forward Error Correction)
├── udp_socket.hpp/cpp       # Асинхронный UDP сокет
├── fibre_protocol.hpp/cpp   # Парсер FIBRE протокола
├── block_reconstructor.hpp/cpp  # Реконструкция блока из чанков
├── relay_peer.hpp/cpp       # Управление одним FIBRE пиром
└── relay_manager.hpp/cpp    # Менеджер всех relay источников
```

### FEC Decoder

Реализует Forward Error Correction для восстановления данных при потере пакетов:

- Блок разбивается на N data chunks
- Генерируются M FEC (parity) chunks
- Для восстановления нужны любые N из (N+M) чанков
- Типичное соотношение: N=100, M=50 (можно потерять до 33% пакетов)

### FIBRE Protocol

Структура UDP пакета:

```cpp
struct FibreHeader {
    uint32_t magic;           // 0xF1B3E001
    uint8_t  version;         // Версия протокола (1)
    uint8_t  flags;           // Флаги пакета
    uint16_t chunk_id;        // ID чанка в блоке
    uint32_t block_height;    // Высота блока
    uint8_t  block_hash[32];  // Хеш блока
    uint16_t total_chunks;    // Всего чанков
    uint16_t data_chunks;     // Чанков с данными
    uint16_t payload_size;    // Размер payload
    // + payload данные
};
```

### Block Reconstructor

Ключевая оптимизация — **Early Header Extraction**:

```cpp
void BlockReconstructor::on_chunk_received(const FibreChunk& chunk) {
    // Если это первый чанк — извлекаем header сразу!
    if (chunk.chunk_id == 0 && chunk.payload_size >= 80) {
        // Block header всегда в начале (первые 80 байт)
        // Начинаем Spy Mining не дожидаясь полного блока!
        if (header_callback_) {
            header_callback_(header);
        }
    }
}
```

## Настройка

### Конфигурация в quaxis.toml

```toml
[relay]
# Включить UDP relay
enabled = true

# Локальный UDP порт
local_port = 8336

# Bandwidth лимит (Mbps)
bandwidth_limit = 100

# Таймаут реконструкции блока (мс)
reconstruction_timeout = 5000

# Forward Error Correction
fec_enabled = true
fec_overhead = 0.5  # 50% избыточности

# FIBRE пиры
[[relay.peers]]
host = "fibre.asia.bitcoinfibre.org"
port = 8336
trusted = true

[[relay.peers]]
host = "fibre.eu.bitcoinfibre.org"
port = 8336
trusted = true

[[relay.peers]]
host = "fibre.us.bitcoinfibre.org"
port = 8336
trusted = true
```

### Публичные FIBRE ноды

| Регион | Хост | Порт |
|--------|------|------|
| Азия | fibre.asia.bitcoinfibre.org | 8336 |
| Европа | fibre.eu.bitcoinfibre.org | 8336 |
| США | fibre.us.bitcoinfibre.org | 8336 |

## Интеграция с Quaxis Miner

### Инициализация

```cpp
#include "relay/relay_manager.hpp"

// Создаём менеджер relay
auto relay_manager = std::make_unique<relay::RelayManager>(config.relay);

// Устанавливаем callback для header
relay_manager->set_header_callback([&](
    const bitcoin::BlockHeader& header,
    relay::BlockSource source
) {
    // Получили header через UDP — сразу начинаем майнить!
    job_manager->on_new_block_header(header, source);
    std::cout << "[RELAY] Header получен за " 
              << relay_manager->stats().avg_header_latency_ms << " мс\n";
});

// Запускаем
relay_manager->start();
```

### Graceful Degradation

Если UDP relay недоступен, система автоматически использует данные от Bitcoin P2P через Shared Memory:

1. **Приоритет 1**: UDP Relay (100-300 мс)
2. **Приоритет 2**: Shared Memory от Bitcoin Core (500-2000 мс)
3. **Резерв**: Polling RPC getblocktemplate

## Мониторинг

### Статистика

```cpp
auto stats = relay_manager->stats();

std::cout << "Активных пиров: " << stats.active_peers << "\n";
std::cout << "Подключенных: " << stats.connected_peers << "\n";
std::cout << "Получено блоков: " << stats.blocks_received << "\n";
std::cout << "Средняя латентность header: " << stats.avg_header_latency_ms << " мс\n";
std::cout << "Таймауты: " << stats.reconstruction_timeouts << "\n";
```

### Логирование

При включённом debug режиме выводится подробная информация:

```
[RELAY] Подключение к fibre.eu.bitcoinfibre.org:8336...
[RELAY] Подключено к 3 пирам
[RELAY] Новый блок 823456, получено 42/150 чанков
[RELAY] Header извлечён за 127 мс (до полной реконструкции)
[RELAY] Блок 823456 реконструирован за 234 мс
```

## Отладка

### Проверка подключения

```bash
# Проверить доступность FIBRE сервера
nc -vzu fibre.eu.bitcoinfibre.org 8336
```

### Частые проблемы

1. **Firewall блокирует UDP**
   - Откройте порт 8336/udp

2. **Высокая потеря пакетов**
   - Увеличьте `fec_overhead` до 0.7-1.0

3. **Таймауты реконструкции**
   - Увеличьте `reconstruction_timeout`
   - Проверьте качество сети

## Безопасность

### Trusted Peers

Только пиры с `trusted = true` используются для Spy Mining. Это защищает от атак с поддельными block headers.

### Валидация

Все полученные headers проходят базовую проверку PoW перед использованием для Spy Mining:

```cpp
if (verify_pow(header)) {
    // Используем для майнинга
} else {
    // Игнорируем подозрительный header
}
```

## Ссылки

- [FIBRE Protocol Specification](http://bitcoinfibre.org/)
- [Forward Error Correction](https://en.wikipedia.org/wiki/Forward_error_correction)
- [Reed-Solomon codes](https://en.wikipedia.org/wiki/Reed%E2%80%93Solomon_error_correction)
