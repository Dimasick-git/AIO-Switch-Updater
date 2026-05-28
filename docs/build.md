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
# → Ryazhenka_AIO.zip с layout switch/aio-switch-updater/...
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

- `APP_VERSION` в `Makefile` — обычный SemVer (например `2.23.3`).
  Совпадает с upstream-веткой AIO-Switch-Updater, на которой
  собрана наша сборка. Используется в самообновлении и в About.
- **Git-теги релизов** = `v${APP_VERSION}` (например `v2.23.3`).
  Создаются автоматически в `build.yml` только когда `APP_VERSION`
  в Makefile отличается от уже опубликованных тегов. Это значит:
  push в main без bump APP_VERSION только собирает артефакты, но
  не создаёт повторных релизов. Чтобы выпустить релиз — поднимите
  patch-цифру и запушьте. `releases/latest/download/Ryazhenka_AIO.zip`
  указывает на
  последнюю удачную сборку.
