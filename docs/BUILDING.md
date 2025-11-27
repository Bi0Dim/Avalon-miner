# Сборка Quaxis Solo Miner

## Требования

### Операционная система

- Linux (Ubuntu 22.04+, Debian 12+, Fedora 38+)
- Поддержка POSIX shared memory

### Компилятор

- GCC 13+ или Clang 17+
- Поддержка C++23

### Зависимости

- CMake 3.25+
- libcurl
- POSIX threads

### Опционально

- CPU с SHA-NI (Intel Skylake+, AMD Zen+) для ускорения

## Установка зависимостей

### Ubuntu/Debian

```bash
# Компилятор и инструменты
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    git

# GCC 13 (если не установлен)
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install -y gcc-13 g++-13

# Зависимости
sudo apt install -y \
    libcurl4-openssl-dev \
    libssl-dev

# Опционально: для документации
sudo apt install -y doxygen graphviz
```

### Fedora

```bash
sudo dnf install -y \
    gcc \
    g++ \
    cmake \
    ninja-build \
    git \
    libcurl-devel \
    openssl-devel
```

### Arch Linux

```bash
sudo pacman -S \
    base-devel \
    cmake \
    ninja \
    git \
    curl \
    openssl
```

## Клонирование репозитория

```bash
git clone https://github.com/Bi0Dim/Avalon-miner.git
cd Avalon-miner
```

## Сборка

### Быстрая сборка (Release)

```bash
mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja
```

### Отладочная сборка

```bash
mkdir build-debug && cd build-debug
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..
ninja
```

### Опции сборки

| Опция | По умолчанию | Описание |
|-------|--------------|----------|
| `QUAXIS_ENABLE_SHANI` | ON | Включить SHA-NI оптимизации |
| `QUAXIS_ENABLE_TESTS` | ON | Сборка тестов |
| `QUAXIS_ENABLE_BENCHMARKS` | ON | Сборка бенчмарков |

Пример с опциями:

```bash
cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DQUAXIS_ENABLE_TESTS=OFF \
    -DQUAXIS_ENABLE_SHANI=ON \
    ..
```

## Запуск тестов

```bash
cd build
ctest --output-on-failure
```

Или напрямую:

```bash
./build/tests/quaxis_tests
```

## Установка

```bash
sudo ninja install
```

По умолчанию устанавливается в `/usr/local/bin`.

Для установки в другой каталог:

```bash
cmake -DCMAKE_INSTALL_PREFIX=/opt/quaxis ..
ninja install
```

## Сборка прошивки ASIC

Прошивка ASIC собирается отдельно с использованием ARM toolchain.

### Требования

- ARM GCC toolchain (arm-none-eabi-gcc)

### Установка toolchain

```bash
# Ubuntu/Debian
sudo apt install gcc-arm-none-eabi

# Fedora
sudo dnf install arm-none-eabi-gcc arm-none-eabi-newlib
```

### Сборка

```bash
cd firmware
make
```

Результат: `firmware/build/quaxis_firmware.bin`

### Прошивка ASIC

**ВНИМАНИЕ**: Неправильная прошивка может сделать устройство неработоспособным!

```bash
# Резервное копирование оригинальной прошивки
cgminer-avalon --read-firmware --output original.bin

# Загрузка новой прошивки
cgminer-avalon --flash-firmware quaxis_firmware.bin
```

## Проверка установки

### Проверка сервера

```bash
quaxis-server --version
quaxis-server --check-shani
```

### Проверка SHA-NI

```bash
# Проверка поддержки CPU
grep -o 'sha_ni' /proc/cpuinfo | head -1
```

## Типичные проблемы

### CMake не находит C++23 компилятор

```bash
# Укажите компилятор явно
cmake -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 ..
```

### Ошибка "sha_ni not supported"

CPU не поддерживает SHA-NI. Используется generic реализация (медленнее в ~5 раз).

Проверьте CPU:
```bash
cat /proc/cpuinfo | grep -E "(model name|sha_ni)"
```

### Ошибка линковки с libcurl

```bash
sudo apt install libcurl4-openssl-dev
# или
cmake -DCURL_LIBRARY=/usr/lib/x86_64-linux-gnu/libcurl.so ..
```

### Shared memory permission denied

```bash
# Добавьте пользователя в группу
sudo usermod -a -G shm $USER
# Или настройте права
sudo chmod 666 /dev/shm/quaxis_block
```

## Структура build каталога

После успешной сборки:

```
build/
├── quaxis-server           # Основной исполняемый файл
├── tests/
│   └── quaxis_tests        # Тесты
├── compile_commands.json   # Для IDE
└── src/
    ├── core/
    ├── crypto/
    ├── bitcoin/
    ├── mining/
    ├── network/
    └── monitoring/
```
