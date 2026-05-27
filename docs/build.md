# Сборка из исходников

## Требования

- devkitPro + devkitARM + libnx
- C++23-совместимый toolchain (поставляется в составе devkitPro)
- `zip`, `python3`, GNU `make`

## Быстрый старт

```bash
git clone --recursive https://github.com/Dimasick-git/AIO-Switch-Updater
cd AIO-Switch-Updater

# Проверка что наши патчи на месте (должно показать "already applied" × 3)
bash scripts/ryazhenka/apply-patches.sh --check

# Сборка форвардера и основного NRO
make -C aiosu-forwarder
make -j"$(nproc)"

# Упаковка в релизный zip
bash scripts/ryazhenka/pack-release.sh
# → ryazhenka-updater.zip с layout switch/aio-switch-updater/...
```

## Через docker

CI использует образ `devkitpro/devkita64:latest`. Локальный аналог:

```bash
docker run --rm -it -v "$PWD:/work" -w /work devkitpro/devkita64:latest bash -c '
    git config --global --add safe.directory /work
    git config --global --add safe.directory /work/lib/borealis
    git config --global --add safe.directory /work/TegraExplorer
    make -C aiosu-forwarder
    make -j"$(nproc)"
    bash scripts/ryazhenka/pack-release.sh
'
```

## Релизный артефакт

`make` оставляет `ryazhenka-updater.nro` в корне (имя из `TARGET` в
`Makefile`). Скрипт `pack-release.sh` копирует его в
`switch/aio-switch-updater/aio-switch-updater.nro` (переименование!) и
зипует. Это сохраняет совместимость с in-app self-updater'ом, который
ожидает фиксированный путь по `NRO_PATH` константе.

## Версионирование

`APP_VERSION` в `Makefile` имеет формат
`<upstream>-ryazhenka.<patch>` (например `2.23.3-ryazhenka.1`). Когда
sync-workflow подтягивает новую upstream-версию через bump, патч-число
сбрасывается обратно на `.1`. Релизные git-теги — `vX.Y.Z-ryazhenka.N`.
