# Ryazhenka Updater

[![Build](https://github.com/Dimasick-git/AIO-Switch-Updater/actions/workflows/build.yml/badge.svg)](https://github.com/Dimasick-git/AIO-Switch-Updater/actions/workflows/build.yml)
[![Sync upstream](https://github.com/Dimasick-git/AIO-Switch-Updater/actions/workflows/sync-upstream.yml/badge.svg)](https://github.com/Dimasick-git/AIO-Switch-Updater/actions/workflows/sync-upstream.yml)
[![Release](https://img.shields.io/github/v/release/Dimasick-git/AIO-Switch-Updater)](https://github.com/Dimasick-git/AIO-Switch-Updater/releases)
[![License](https://img.shields.io/github/license/Dimasick-git/AIO-Switch-Updater)](LICENSE)

> Homebrew updater for the **Ряженка** (Ryazhenka) custom-firmware pack for Nintendo Switch.

---

## English (short)

Ryazhenka Updater is a Russian-localized fork of
[HamletDuFromage/aio-switch-updater](https://github.com/HamletDuFromage/aio-switch-updater)
adapted for the Ryazhenka CFW ecosystem under
[github.com/Dimasick-git](https://github.com/Dimasick-git). It downloads and
updates Atmosphère, Hekate, sigpatches, payloads, firmware images, and the
full Ryazhenka pack directly on the console.

**Differences from upstream**

- Default interface language is **Russian**; English and 11 other locales
  remain selectable.
- Source URLs point at Dimasick-git infrastructure
  (`Dimasick-git/nx-links` for the JSON catalogue, `Dimasick-git/Ryzhenka` for
  the full pack archive).
- Self-update target is this fork's GitHub Releases.
- New "Install Ryazhenka pack" entry downloads the latest pack as
  `sd_card.zip` (rolled out in upcoming releases).
- A daily GitHub Actions job pulls upstream changes and replays the
  Ryazhenka-specific patches so we stay close to upstream master.

**Build (devkitPro required)**

```bash
git clone --recursive https://github.com/Dimasick-git/AIO-Switch-Updater
cd AIO-Switch-Updater
make -C aiosu-forwarder
make -j"$(nproc)"
bash scripts/ryazhenka/pack-release.sh   # produces ryazhenka-updater.zip
```

Licensed under **GPL-3.0**. All upstream attributions are preserved — see
[LICENSE](LICENSE) and the *Special thanks* section below.

---

## Русский

**Ряженка Updater** — это форк
[AIO-Switch-Updater](https://github.com/HamletDuFromage/aio-switch-updater)
адаптированный под кастомную прошивку **Ряженка** и под русскоязычное
сообщество Nintendo Switch.

### Что внутри

- 🇷🇺 Интерфейс на русском по умолчанию (другие 12 языков доступны через меню).
- 🥛 Источники прошивок и CFW указывают на инфраструктуру
  [Dimasick-git](https://github.com/Dimasick-git): свой каталог
  `nx-links`, релизы пака `Ryzhenka`, форки Atmosphere, Sys-clk, EdiZon,
  FPSLocker, Fizeau и тд.
- 🔄 Ежедневная авто-синхронизация с апстримом: новые версии и багфиксы
  HamletDuFromage подтягиваются без участия человека, наши правки
  переприменяются через `git apply` (см. `scripts/ryazhenka/`).
- 🛠 Самообновление приложения через GitHub Releases этого репозитория.

### Установка

1. Скачайте `ryazhenka-updater.zip` со страницы
   [Releases](https://github.com/Dimasick-git/AIO-Switch-Updater/releases).
2. Распакуйте архив в корень SD-карты — внутри лежит структура
   `switch/aio-switch-updater/aio-switch-updater.nro`.
3. Запустите через hbmenu. При первом запуске откроется предупреждение,
   подтвердите чтобы продолжить.

### Возможности (из апстрима, перевод сохранён)

- **Обновить Atmosphere** — скачивает свежий релиз и финализирует установку
  через специальный RCM payload.
- **Hekate / Payloads** — обновляет загрузчик, скачивает RCM payload'ы.
- **Сторонние загрузки (Custom Downloads)** — добавляйте свои ссылки в
  `/config/aio-switch-updater/custom_packs.json`.
- **Прошивки** — скачивает архивы firmware, которые потом ставятся через
  Daybreak.
- **Читы** — читы из GBAtemp и Cheat Slips, обновляются ежедневно.
- **Инструменты**: смена цвета Joy-Con / Pro Controller, настройки сети,
  скрытие вкладок, веб-браузер, перезагрузка в payload.

### Установка пака Ряженки

Доступно через одноимённую вкладку (раскатывается в ближайших релизах).
Скачивается полный архив пака с
[`Dimasick-git/Ryzhenka/releases/latest`](https://github.com/Dimasick-git/Ryzhenka/releases/latest)
и распаковывается в корень SD.

### Экосистема Ряженки

| Компонент | Репозиторий |
|---|---|
| Сам пак Ряженки | <https://github.com/Dimasick-git/Ryzhenka> |
| Atmosphere (форк) | <https://github.com/Dimasick-git/Atmosphere> |
| Sys-clk с русским переводом | <https://github.com/Dimasick-git/Sys-clk> |
| Ryazhahand-Overlay | <https://github.com/Dimasick-git/Ryazhahand-Overlay> |
| FPSLocker | <https://github.com/Dimasick-git/FPSLocker> |
| EdiZon | <https://github.com/Dimasick-git/EdiZon> |
| Fizeau | <https://github.com/Dimasick-git/Fizeau> |
| ovlSysmodules | <https://github.com/Dimasick-git/ovlSysmodules> |
| Mission-Control | <https://github.com/Dimasick-git/Mission-Control> |
| Ryazha-Status-Monitor | <https://github.com/Dimasick-git/Ryazha-Status-Monitor> |
| Ryazhenkabestcfw-Tuner | <https://github.com/Dimasick-git/Ryazhenkabestcfw-Tuner> |
| Telegram канал | <https://t.me/Ryazhenkacfw> |

### Сборка из исходников

Нужен установленный
[devkitPro / devkitARM](https://devkitpro.org/wiki/Getting_Started):

```bash
sudo (dkp-)pacman -Sy
sudo (dkp-)pacman -S switch-glfw switch-curl switch-glad switch-glm \
                    switch-mbedtls switch-zlib devkitarm-rules
git clone --recursive https://github.com/Dimasick-git/AIO-Switch-Updater
cd AIO-Switch-Updater
make -C aiosu-forwarder
make -j"$(nproc)"
bash scripts/ryazhenka/pack-release.sh
```

В CI используется контейнер `devkitpro/devkita64:latest` — см.
[`.github/workflows/build.yml`](.github/workflows/build.yml).

### Как мы синхронизируемся с апстримом

Раз в сутки workflow [`sync-upstream.yml`](.github/workflows/sync-upstream.yml)
делает следующее:

1. Подтягивает `HamletDuFromage/aio-switch-updater@master`.
2. Сливает в наш `main` через `git merge --no-ff`.
3. Перенакладывает патчи из `scripts/ryazhenka/patches/` (через
   `apply-patches.sh` с reverse-check — идемпотентно).
4. Если был конфликт merge или патч не наложился — создаётся issue с
   лейблом `upstream-conflict` и человек разруливает вручную.

Подробности — в [`docs/sync-policy.md`](docs/sync-policy.md).

### Контрибьютинг

PR и идеи приветствуются. Перед коммитом проверьте:

- `bash scripts/ryazhenka/apply-patches.sh --check` — патчи на месте.
- `python3 scripts/ryazhenka/check_i18n.py` — ru покрывает en-US.
- Сборка проходит в CI (`build.yml`).

Если хотите помочь с переводами — правьте `resources/i18n/ru/menus.json`,
шаблоны в `resources/i18n/en-US/menus.json`.

### Лицензия

GPL-3.0, как и апстрим. Полный текст — [LICENSE](LICENSE).

### Благодарности

Этот форк не существовал бы без огромной работы апстрим-команды:

- [HamletDuFromage](https://github.com/HamletDuFromage) — оригинальный
  AIO-Switch-Updater.
- [natinusala](https://github.com/natinusala) — UI-фреймворк borealis.
- [tiansongyu](https://github.com/tiansongyu) — мультиязычность и
  китайский перевод.
- [yyoossk](https://github.com/yyoossk), [sergiou87](https://github.com/sergiou87),
  [pedruhb](https://github.com/pedruhb), [AD2076](https://github.com/AD2076),
  [qazrfv1234](https://github.com/qazrfv1234),
  [Nota Inutilis](https://github.com/NotaInutilis/) — локализации.
- [Team Neptune](https://github.com/Team-Neptune) — RCM код.
- [fennectech](https://github.com/fennectech) — тесты и фидбек.
- Iliak — [Cheat Slips](https://www.cheatslips.com/).
