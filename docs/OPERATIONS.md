# Операционное руководство

Руководство по эксплуатации Quaxis Solo Miner в production среде.

## Системные требования

### Hardware

- **CPU**: Intel/AMD с поддержкой SHA-NI (Skylake+, Zen+)
- **RAM**: 4+ GB
- **Disk**: SSD рекомендуется для Bitcoin Core
- **Network**: 1 Gbps Ethernet

### Software

- Linux (Ubuntu 22.04+, Debian 12+)
- GCC 13+ или Clang 17+ (для сборки)
- systemd (для управления сервисом)

## Оптимизация системы

### sysctl настройки

Добавьте в `/etc/sysctl.d/99-quaxis.conf`:

```bash
# Увеличить буферы сети
net.core.rmem_max = 134217728
net.core.wmem_max = 134217728
net.core.netdev_max_backlog = 5000

# TCP оптимизации
net.ipv4.tcp_rmem = 4096 87380 134217728
net.ipv4.tcp_wmem = 4096 65536 134217728
net.ipv4.tcp_max_syn_backlog = 8192
net.ipv4.tcp_slow_start_after_idle = 0
net.ipv4.tcp_tw_reuse = 1

# Уменьшить latency
net.ipv4.tcp_low_latency = 1
net.core.busy_poll = 50
net.core.busy_read = 50

# Shared memory
kernel.shmmax = 268435456
kernel.shmall = 2097152
```

Применить: `sudo sysctl -p /etc/sysctl.d/99-quaxis.conf`

### CPU Affinity (опционально)

Для минимальной латентности можно привязать потоки к конкретным CPU:

```bash
# Показать доступные CPU
lscpu | grep -E "^CPU\(s\)|^Thread"

# Запуск с привязкой (пример)
taskset -c 0-3 ./quaxis-miner --config /etc/quaxis/quaxis.toml
```

Рекомендуемое распределение:
- **CPU 0**: SHM subscriber (критичная латентность)
- **CPU 1**: Job Manager
- **CPU 2-3**: Network workers

### Huge Pages (опционально)

```bash
# Включить huge pages
echo 128 | sudo tee /proc/sys/vm/nr_hugepages

# Проверить
cat /proc/meminfo | grep -i huge
```

## Запуск

### Systemd сервис

Создайте `/etc/systemd/system/quaxis.service`:

```ini
[Unit]
Description=Quaxis Solo Miner
After=network.target bitcoind.service
Wants=bitcoind.service

[Service]
Type=simple
User=quaxis
Group=quaxis
ExecStart=/opt/quaxis/quaxis-miner --config /etc/quaxis/quaxis.toml
Restart=always
RestartSec=10

# Ресурсы
LimitNOFILE=65535
LimitMEMLOCK=infinity

# CPU affinity (опционально)
# CPUAffinity=0-3

# Безопасность
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/log/quaxis /run/quaxis

[Install]
WantedBy=multi-user.target
```

```bash
# Активировать
sudo systemctl daemon-reload
sudo systemctl enable quaxis
sudo systemctl start quaxis

# Проверить статус
sudo systemctl status quaxis
journalctl -u quaxis -f
```

### Docker

```dockerfile
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    libcurl4 \
    && rm -rf /var/lib/apt/lists/*

COPY quaxis-miner /usr/local/bin/
COPY quaxis.toml /etc/quaxis/

EXPOSE 3333 9090

CMD ["quaxis-miner", "--config", "/etc/quaxis/quaxis.toml"]
```

```bash
docker run -d \
  --name quaxis \
  -p 3333:3333 \
  -p 9090:9090 \
  -v /path/to/quaxis.toml:/etc/quaxis/quaxis.toml:ro \
  --ipc=host \
  quaxis-miner
```

## Fallback System

### Архитектура

```
Primary (SHM) ──► ZMQ Fallback ──► Stratum Fallback
       │               │                  │
       ▼               ▼                  ▼
  Bitcoin Core    Bitcoin Core       Pool Server
  (Shared Mem)       (ZMQ)          (ckpool.org)
```

### Конфигурация

```toml
[fallback]
enabled = true

[fallback.zmq]
enabled = true
endpoint = "tcp://127.0.0.1:28332"

[fallback.stratum]
enabled = true
url = "stratum+tcp://solo.ckpool.org:3333"
user = "YOUR_BITCOIN_ADDRESS"
password = "x"

[fallback.timeouts]
primary_health_check_ms = 1000
primary_timeout_ms = 5000
reconnect_delay_ms = 1000
```

### Логика переключения

1. **SHM → ZMQ**: Если SHM не обновляется > 5 секунд
2. **ZMQ → Stratum**: Если Bitcoin Core RPC недоступен > 30 секунд
3. **Восстановление**: Автоматически при восстановлении primary

### Мониторинг fallback

```bash
# Метрика текущего режима
curl -s localhost:9090/metrics | grep quaxis_mode
# 0 = SHM (primary)
# 1 = ZMQ
# 2 = Stratum

# Количество переключений
curl -s localhost:9090/metrics | grep quaxis_fallback_switches
```

## Adaptive Spin-Wait

SHM subscriber использует многостадийный алгоритм ожидания:

```
[Spin] ──10000 iter──► [Yield] ──1000 iter──► [Sleep 100μs]
   ▲                                              │
   └──────────── on_change_detected ──────────────┘
```

### Конфигурация

```toml
[shm]
enabled = true
path = "/quaxis_block"
spin_wait = true

# Параметры adaptive spin (в коде)
# spin_iterations = 1000    # Итераций в spin stage
# yield_iterations = 100    # Итераций в yield stage
# sleep_us = 100            # Микросекунд в sleep stage
```

### Профили

| Профиль | Spin | Yield | Sleep | CPU | Latency |
|---------|------|-------|-------|-----|---------|
| High Performance | 10000 | 1000 | 50μs | Высокий | ~100ns |
| Balanced | 1000 | 100 | 100μs | Средний | ~1μs |
| Power Saving | 100 | 10 | 1000μs | Низкий | ~100μs |

## Merged Mining

### Проверка состояния chains

```bash
curl -s localhost:9090/metrics | grep merged
```

### Приоритеты chains

| Chain | Priority | Рекомендация |
|-------|----------|--------------|
| Fractal | 100 | Высший приоритет, максимальный доход |
| RSK | 90 | Второй приоритет |
| Syscoin | 80 | |
| Namecoin | 70 | |
| Elastos | 60 | |
| Hathor | 50 | |
| Emercoin | 50 | |
| Myriad | 40 | |
| Unobtanium | 35 | |
| Huntercoin | 30 | |
| Terracoin | 25 | |
| VCash | 10 | Низший приоритет |

### Отключение chain

```toml
[[merged_mining.chains]]
name = "vcash"
enabled = false  # Отключить
```

## Резервное копирование

### Важные файлы

```
/etc/quaxis/quaxis.toml   # Конфигурация
/var/log/quaxis/          # Логи
~/.bitcoin/wallet.dat     # Bitcoin кошелёк (если используется)
```

### Скрипт бэкапа

```bash
#!/bin/bash
BACKUP_DIR="/backup/quaxis/$(date +%Y%m%d)"
mkdir -p "$BACKUP_DIR"
cp /etc/quaxis/quaxis.toml "$BACKUP_DIR/"
cp -r /var/log/quaxis "$BACKUP_DIR/logs"
```

## Troubleshooting

### Высокое использование CPU

1. Переключитесь на balanced профиль spin-wait
2. Проверьте количество merged mining chains
3. Убедитесь, что SHM работает корректно

```bash
# Проверить использование CPU
top -p $(pgrep quaxis-miner)

# Проверить SHM
ls -la /dev/shm/quaxis*
```

### Частые переключения fallback

1. Проверьте стабильность Bitcoin Core:
   ```bash
   bitcoin-cli getblockchaininfo
   ```

2. Проверьте сеть:
   ```bash
   ping -c 10 8.8.8.8
   ```

3. Увеличьте таймауты:
   ```toml
   [fallback.timeouts]
   primary_timeout_ms = 10000
   ```

### ASIC не подключаются

1. Проверьте firewall:
   ```bash
   sudo ufw status
   sudo iptables -L -n | grep 3333
   ```

2. Проверьте, что порт слушается:
   ```bash
   ss -tlnp | grep 3333
   ```

3. Проверьте логи:
   ```bash
   journalctl -u quaxis -n 100 | grep -i error
   ```

### Блоки не находятся

1. Проверьте хешрейт ASIC
2. Убедитесь, что payout_address валидный
3. Проверьте сложность сети и ожидаемое время нахождения

```bash
# Ожидаемое время до блока (при 90 TH/s)
bitcoin-cli getdifficulty
# difficulty / (90 * 10^12) * 2^32 / 600 = дней
```

## Обновление

```bash
# Остановить сервис
sudo systemctl stop quaxis

# Бэкап конфигурации
cp /etc/quaxis/quaxis.toml /etc/quaxis/quaxis.toml.bak

# Обновить бинарник
cp new-quaxis-miner /opt/quaxis/quaxis-miner

# Запустить
sudo systemctl start quaxis

# Проверить
curl localhost:9090/health
```

## Контакты и поддержка

- GitHub Issues: https://github.com/Bi0Dim/Avalon-miner/issues
- Документация: https://github.com/Bi0Dim/Avalon-miner/docs
