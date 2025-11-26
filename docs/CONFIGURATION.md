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

[bitcoin]
# Адрес RPC Bitcoin Core
rpc_host = "127.0.0.1"
# Порт RPC Bitcoin Core
rpc_port = 8332
# Имя пользователя RPC
rpc_user = "quaxis"
# Пароль RPC (ОБЯЗАТЕЛЬНО ИЗМЕНИТЬ!)
rpc_password = "your_secure_password_here"
# Таймаут RPC запросов (секунды)
rpc_timeout = 30

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
# Использовать spin-wait (низкая латентность, высокое CPU)
spin_wait = true
# Интервал polling (микросекунды, если spin_wait=false)
poll_interval_us = 100

[monitoring]
# Интервал вывода статистики (секунды)
stats_interval = 60
# Уровень логирования: trace, debug, info, warn, error
log_level = "info"
# Файл логов (пусто = stdout)
log_file = ""
# Prometheus endpoint
prometheus_enabled = false
prometheus_port = 9090
```

### Параметры секции [server]

| Параметр | Тип | По умолчанию | Описание |
|----------|-----|--------------|----------|
| bind_address | string | "0.0.0.0" | IP адрес для прослушивания |
| port | int | 3333 | TCP порт |
| max_connections | int | 10 | Макс. подключений ASIC |
| socket_buffer_size | int | 65536 | Размер буфера сокета |

### Параметры секции [bitcoin]

| Параметр | Тип | По умолчанию | Описание |
|----------|-----|--------------|----------|
| rpc_host | string | "127.0.0.1" | Адрес Bitcoin Core RPC |
| rpc_port | int | 8332 | Порт Bitcoin Core RPC |
| rpc_user | string | - | Имя пользователя RPC |
| rpc_password | string | - | Пароль RPC |
| rpc_timeout | int | 30 | Таймаут RPC (секунды) |
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
| spin_wait | bool | true | Spin-wait режим |
| poll_interval_us | int | 100 | Интервал polling |

## Конфигурация Bitcoin Core

### Расположение файла

```bash
# Linux
~/.bitcoin/bitcoin.conf

# Создание каталога
mkdir -p ~/.bitcoin
```

### Оптимизированная конфигурация

```ini
# =============================================================================
# Bitcoin Core - Оптимизированная конфигурация для Quaxis
# =============================================================================

# === ОСНОВНЫЕ ===
# Mainnet
#testnet=0
#regtest=0

# Отключение GUI (для сервера)
daemon=1

# === СЕТЬ ===
# Максимум соединений
maxconnections=256
# Исходящих соединений
maxoutboundconnections=16

# Быстрые seed ноды
addnode=seed.bitcoin.sipa.be
addnode=dnsseed.bluematt.me
addnode=seed.bitcoinstats.com
addnode=seed.bitcoin.jonasschnelli.ch
addnode=seed.btc.petertodd.org
addnode=seed.bitcoin.sprovoost.nl

# Прослушивание входящих
listen=1

# UPnP (если за NAT)
upnp=1

# === COMPACT BLOCKS ===
# Хранить транзакции для реконструкции блоков
blockreconstructionextratxn=100000

# === MEMPOOL ===
# Размер mempool (MB)
maxmempool=1000
# Время хранения транзакций (часы)
mempoolexpiry=336

# === ZMQ (запасной канал) ===
# Hash нового блока
zmqpubhashblock=tcp://127.0.0.1:28332
# Сырой блок
zmqpubrawblock=tcp://127.0.0.1:28333

# === ПРОИЗВОДИТЕЛЬНОСТЬ ===
# Потоки проверки подписей (0 = auto)
par=0
# Кеш UTXO (MB)
dbcache=4096
# Не хранить транзакции в txindex
txindex=0

# === WALLET ===
# Отключить wallet (не нужен для майнинга)
disablewallet=1

# === RPC ===
server=1
# Имя пользователя (ИЗМЕНИТЬ!)
rpcuser=quaxis
# Пароль (ИЗМЕНИТЬ!)
rpcpassword=CHANGE_THIS_PASSWORD
# Разрешённые IP
rpcallowip=127.0.0.1
# Порт
rpcport=8332
# Потоки RPC
rpcthreads=4
# Размер очереди RPC
rpcworkqueue=64

# === ЛОГИРОВАНИЕ ===
# Уровень отладки
debug=0
# Логировать timestamps
logtimestamps=1
# Файл логов
#debuglogfile=/var/log/bitcoin/debug.log

# === QUAXIS ПАТЧИ ===
# Включить Shared Memory bridge (требует патч)
quaxisshm=1
# Путь к SHM
quaxisshmpath=/quaxis_block
# Включить spy mining (требует патч)
quaxisspymining=1
# Приоритет block сообщений (требует патч)
quaxisblockpriority=1
```

## Генерация безопасного пароля

```bash
# Генерация случайного пароля
openssl rand -hex 32

# Или
head -c 32 /dev/urandom | xxd -p | tr -d '\n'
```

## Проверка конфигурации

### Проверка quaxis.toml

```bash
quaxis-server --check-config --config quaxis.toml
```

### Проверка bitcoin.conf

```bash
bitcoind -conf=/path/to/bitcoin.conf -debug=1 -printtoconsole
```

### Проверка RPC соединения

```bash
curl --user quaxis:password \
     --data-binary '{"jsonrpc": "1.0", "method": "getblockchaininfo"}' \
     -H 'content-type: text/plain;' \
     http://127.0.0.1:8332/
```

## Запуск

### Запуск Bitcoin Core

```bash
bitcoind -daemon
# Проверка
bitcoin-cli getblockchaininfo
```

### Запуск Quaxis Server

```bash
quaxis-server --config ~/.config/quaxis/quaxis.toml
```

### Systemd сервис

Создайте `/etc/systemd/system/quaxis.service`:

```ini
[Unit]
Description=Quaxis Solo Miner Server
After=network.target bitcoind.service
Wants=bitcoind.service

[Service]
Type=simple
User=quaxis
Group=quaxis
ExecStart=/usr/local/bin/quaxis-server --config /etc/quaxis/quaxis.toml
Restart=always
RestartSec=10

# Ограничения
LimitNOFILE=65536
MemoryMax=4G

# Безопасность
NoNewPrivileges=yes
ProtectSystem=strict
ProtectHome=yes
ReadWritePaths=/dev/shm /var/log/quaxis

[Install]
WantedBy=multi-user.target
```

Активация:

```bash
sudo systemctl daemon-reload
sudo systemctl enable quaxis
sudo systemctl start quaxis
sudo systemctl status quaxis
```

## Мониторинг

### Логи

```bash
# Systemd journal
journalctl -u quaxis -f

# Или файл логов
tail -f /var/log/quaxis/quaxis.log
```

### Статистика

```bash
# Curl к Prometheus endpoint
curl http://localhost:9090/metrics
```

### Проверка Shared Memory

```bash
# Проверка существования
ls -la /dev/shm/quaxis_block

# Мониторинг активности
watch -n 0.1 'cat /dev/shm/quaxis_block | xxd | head'
```
