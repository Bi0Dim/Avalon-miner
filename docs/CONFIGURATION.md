# Настройка Quaxis Solo Miner

## Файлы конфигурации

- `quaxis.toml` - Конфигурация сервера Quaxis
- `bitcoin.conf` - Конфигурация Bitcoin Core

## Конфигурация Quaxis Server

### Расположение файла

```bash
# По умолчанию
~/.config/quaxis/quaxis.toml

# Или указать явно
quaxis-server --config /path/to/quaxis.toml
```

### Пример конфигурации

```toml
# =============================================================================
# Quaxis Solo Miner - Конфигурация сервера
# Universal AuxPoW Core - автономный режим
# =============================================================================

[server]
# Адрес для прослушивания подключений ASIC
bind_address = "0.0.0.0"
# Порт для подключений ASIC
port = 3333
# Максимальное количество подключений
max_connections = 10
# Размер буфера сокета (байт)
socket_buffer_size = 65536

[parent_chain]
# Источник заголовков: "p2p", "fibre" или "trusted"
headers_source = "p2p"
# Seed ноды для P2P подключения
seed_nodes = ["seed.bitcoin.sipa.be", "dnsseed.bluematt.me"]
# Интервал обновления MTP (секунды)
mtp_refresh_seconds = 60
# Адрес для получения награды (P2WPKH - bc1q...)
payout_address = "bc1qrpamkfhuragxrfx8a9c28drcwygdwsgkzk2ykq"

[mining]
# Тег в coinbase транзакции (до 20 символов ASCII)
coinbase_tag = "quaxis"
# Размер extranonce (байт, 4-8)
extranonce_size = 6
# Размер очереди заданий
job_queue_size = 100
# Использовать spy mining (начинать до валидации)
use_spy_mining = true
# Использовать MTP+1 timestamp
use_mtp_timestamp = true
# Пустые блоки (только coinbase)
empty_blocks = true

[shm]
# Использовать Shared Memory для уведомлений
enabled = true
# Путь к shared memory
path = "/quaxis_block"
# Использовать адаптивный spin-wait
adaptive_spin_enabled = true
# Количество итераций фазы 1 (spin)
spin_phase1_iterations = 2000
# Количество итераций фазы 2 (yield)
spin_phase2_iterations = 2000
# Время sleep в микросекундах (фаза 3)
sleep_us = 200

[logging]
# Интервал обновления экрана (миллисекунды)
refresh_interval_ms = 1000
# Уровень логирования: error, warn, info, debug
level = "info"
# Размер истории событий
event_history = 200
# Использовать цветной вывод
color = true
# Показывать хешрейт
show_hashrate = true
# Подсвечивать найденные блоки
highlight_found_blocks = true
# Показывать счётчики блоков по chains
show_chain_block_counts = true
```

### Параметры секции [server]

| Параметр | Тип | По умолчанию | Описание |
|----------|-----|--------------|----------|
| bind_address | string | "0.0.0.0" | IP адрес для прослушивания |
| port | int | 3333 | TCP порт |
| max_connections | int | 10 | Макс. подключений ASIC |
| socket_buffer_size | int | 65536 | Размер буфера сокета |

### Параметры секции [parent_chain]

| Параметр | Тип | По умолчанию | Описание |
|----------|-----|--------------|----------|
| headers_source | string | "p2p" | Источник заголовков: p2p, fibre, trusted |
| seed_nodes | array | [...] | Seed ноды для P2P |
| mtp_refresh_seconds | int | 60 | Интервал обновления MTP |
| payout_address | string | - | Адрес для награды |

### Параметры секции [mining]

| Параметр | Тип | По умолчанию | Описание |
|----------|-----|--------------|----------|
| coinbase_tag | string | "quaxis" | Тег в coinbase |
| extranonce_size | int | 6 | Размер extranonce |
| job_queue_size | int | 100 | Размер очереди заданий |
| use_spy_mining | bool | true | Spy mining |
| use_mtp_timestamp | bool | true | Использовать MTP+1 |
| empty_blocks | bool | true | Пустые блоки |

### Параметры секции [shm]

| Параметр | Тип | По умолчанию | Описание |
|----------|-----|--------------|----------|
| enabled | bool | true | Включить SHM |
| path | string | "/quaxis_block" | Путь к SHM |
| adaptive_spin_enabled | bool | true | Адаптивный spin-wait |
| spin_phase1_iterations | int | 2000 | Итерации фазы 1 (spin) |
| spin_phase2_iterations | int | 2000 | Итерации фазы 2 (yield) |
| sleep_us | int | 200 | Sleep в микросекундах |

### Параметры секции [logging]

| Параметр | Тип | По умолчанию | Описание |
|----------|-----|--------------|----------|
| refresh_interval_ms | int | 1000 | Интервал обновления экрана |
| level | string | "info" | Уровень логирования |
| event_history | int | 200 | Размер истории событий |
| color | bool | true | Цветной вывод |
| show_hashrate | bool | true | Показывать хешрейт |
| highlight_found_blocks | bool | true | Подсвечивать найденные блоки |
| show_chain_block_counts | bool | true | Показывать счётчики |
| poll_interval_us | int | 100 | Интервал polling |


## Проверка конфигурации

### Проверка quaxis.toml

```bash
quaxis-miner --test-config --config quaxis.toml
```

## Запуск

### Запуск Quaxis Miner

```bash
# Запуск с конфигурацией по умолчанию
./quaxis-miner

# Запуск с указанием файла конфигурации
./quaxis-miner -c /etc/quaxis/quaxis.toml
```
