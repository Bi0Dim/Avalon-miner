# Мониторинг и метрики

Quaxis Solo Miner предоставляет HTTP endpoints для мониторинга и сбора метрик в формате Prometheus.

## HTTP Endpoints

### /health

Возвращает статус здоровья системы в формате JSON.

```bash
curl http://localhost:9090/health
```

**Пример ответа (здоровая система):**
```json
{
  "status": "healthy",
  "uptime_seconds": 86400,
  "mode": "primary_shm",
  "bitcoin_core": "connected",
  "asic_connections": 3,
  "last_job_age_ms": 150
}
```

**HTTP статусы:**
- `200 OK` — система здорова
- `503 Service Unavailable` — система нездорова

### /metrics

Возвращает метрики в формате Prometheus exposition format.

```bash
curl http://localhost:9090/metrics
```

**Пример ответа:**
```text
# HELP quaxis_hashrate_ths Current hashrate in TH/s
# TYPE quaxis_hashrate_ths gauge
quaxis_hashrate_ths 90.5

# HELP quaxis_jobs_sent_total Total jobs sent to ASIC
# TYPE quaxis_jobs_sent_total counter
quaxis_jobs_sent_total 12345

# HELP quaxis_shares_found_total Total shares found
# TYPE quaxis_shares_found_total counter
quaxis_shares_found_total 42

# HELP quaxis_blocks_found_total Total blocks found
# TYPE quaxis_blocks_found_total counter
quaxis_blocks_found_total 1

# HELP quaxis_uptime_seconds Server uptime
# TYPE quaxis_uptime_seconds counter
quaxis_uptime_seconds 86400

# HELP quaxis_mode Current operating mode (0=shm, 1=zmq, 2=stratum)
# TYPE quaxis_mode gauge
quaxis_mode 0

# HELP quaxis_bitcoin_core_connected Bitcoin Core connection status
# TYPE quaxis_bitcoin_core_connected gauge
quaxis_bitcoin_core_connected 1

# HELP quaxis_asic_connections Number of connected ASIC devices
# TYPE quaxis_asic_connections gauge
quaxis_asic_connections 3

# HELP quaxis_merged_chains_active Active merged mining chains
# TYPE quaxis_merged_chains_active gauge
quaxis_merged_chains_active 11

# HELP quaxis_latency_ms Job latency in milliseconds
# TYPE quaxis_latency_ms histogram
quaxis_latency_ms_bucket{le="1"} 100
quaxis_latency_ms_bucket{le="5"} 500
quaxis_latency_ms_bucket{le="10"} 900
quaxis_latency_ms_bucket{le="+Inf"} 1000
quaxis_latency_ms_sum 2500.5
quaxis_latency_ms_count 1000
```

### /ready (опционально)

Эндпоинт готовности для Kubernetes/Docker orchestration.

## Доступные метрики

### Counters (всегда возрастающие)

| Метрика | Описание |
|---------|----------|
| `quaxis_jobs_sent_total` | Всего отправлено заданий ASIC |
| `quaxis_shares_found_total` | Всего найдено shares |
| `quaxis_blocks_found_total` | Всего найдено блоков Bitcoin |
| `quaxis_merged_blocks_total{chain="..."}` | Найдено блоков по AuxPoW chain |
| `quaxis_uptime_seconds` | Время работы сервера |
| `quaxis_fallback_switches_total` | Переключений на fallback |
| `quaxis_errors_total` | Количество ошибок |

### Gauges (текущее значение)

| Метрика | Описание |
|---------|----------|
| `quaxis_hashrate_ths` | Текущий хешрейт (TH/s) |
| `quaxis_mode` | Режим работы: 0=SHM, 1=ZMQ, 2=Stratum |
| `quaxis_bitcoin_core_connected` | 1=подключён, 0=отключён |
| `quaxis_asic_connections` | Количество подключённых ASIC |
| `quaxis_merged_chains_active` | Активных merged mining chains |
| `quaxis_block_height` | Текущая высота блока |
| `quaxis_difficulty` | Текущая сложность сети |
| `quaxis_fallback_active` | 1=fallback активен, 0=primary |
| `quaxis_block_height{chain="namecoin"}` | Высота блока по chain |
| `quaxis_job_queue_depth` | Глубина очереди заданий |

### Histograms (распределение)

| Метрика | Описание | Buckets (ms) |
|---------|----------|--------------|
| `quaxis_latency_ms` | Латентность заданий | 1, 5, 10, 25, 50, 100, 250, 500, 1000 |
| `quaxis_job_age_ms` | Возраст заданий | 1, 5, 10, 25, 50, 100, 250, 500, 1000 |
| `quaxis_block_latency_ms` | Латентность обнаружения блока | 1, 5, 10, 25, 50, 100, 250, 500, 1000 |

## Конфигурация

В файле `quaxis.toml`:

```toml
[monitoring]
# Интервал вывода статистики (секунды)
stats_interval = 60

# Уровень логирования: trace, debug, info, warn, error
log_level = "info"

# Файл для логов (пустая строка = stdout)
log_file = ""

# Включить Prometheus endpoint
prometheus_enabled = true
prometheus_port = 9090

[monitoring.http]
# Включить HTTP сервер
enabled = true
# Адрес для прослушивания
bind = "0.0.0.0"
# Порт HTTP сервера
port = 9090

[monitoring.health]
enabled = true
path = "/health"

[monitoring.prometheus]
enabled = true
path = "/metrics"

[monitoring.alerts]
# Минимальный уровень для логирования: info, warning, critical
log_level = "warning"
# Интервал дедупликации одинаковых алертов (секунды)
dedup_interval = 60
```

## Интеграция с Prometheus

### prometheus.yml

```yaml
scrape_configs:
  - job_name: 'quaxis'
    scrape_interval: 15s
    static_configs:
      - targets: ['localhost:9090']
    metrics_path: /metrics
```

### Алерты (prometheus-alerts.yml)

```yaml
groups:
  - name: quaxis
    rules:
      # Bitcoin Core отключён
      - alert: BitcoinCoreDisconnected
        expr: quaxis_bitcoin_core_connected == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Bitcoin Core disconnected"
          
      # Fallback активен
      - alert: FallbackActive
        expr: quaxis_mode > 0
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Running in fallback mode"
          
      # Высокая латентность
      - alert: HighLatency
        expr: histogram_quantile(0.99, rate(quaxis_latency_ms_bucket[5m])) > 100
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High job latency"
          
      # Нет подключённых ASIC
      - alert: NoAsicConnections
        expr: quaxis_asic_connections == 0
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "No ASIC devices connected"
```

## Интеграция с Grafana

Готовый dashboard доступен в `monitoring/grafana-dashboard.json`.

Импорт:
1. Откройте Grafana → Dashboards → Import
2. Загрузите файл или вставьте JSON
3. Выберите источник данных Prometheus

## Стресс-тестирование метрик

Для проверки производительности `/metrics` под нагрузкой:

```bash
# Компиляция симулятора
cd scripts/stress
g++ -std=c++23 -O2 -pthread fake_asic_simulator.cpp -o fake_asic_simulator

# Запуск с 500 клиентами
./fake_asic_simulator -n 500 -d 120 --host 127.0.0.1
```

Симулятор:
- Подключает N клиентов постепенно (ramp-up)
- Периодически проверяет `/metrics` endpoint
- Выводит статистику латентности

## Troubleshooting

### Метрики не обновляются

1. Проверьте, что HTTP сервер запущен: `curl localhost:9090/health`
2. Проверьте логи на наличие ошибок
3. Убедитесь, что `prometheus_enabled = true` в конфиге

### Высокая латентность /metrics

1. Частота опроса слишком высокая — увеличьте `scrape_interval`
2. Слишком много merged mining chains — отключите неактивные
3. Проверьте загрузку CPU

### Prometheus не получает метрики

1. Проверьте firewall: `telnet localhost 9090`
2. Проверьте формат: `curl -s localhost:9090/metrics | head`
3. Проверьте Prometheus targets: `http://prometheus:9090/targets`
