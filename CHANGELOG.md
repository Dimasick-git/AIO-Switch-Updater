# Changelog

Формат — [Keep a Changelog](https://keepachangelog.com/ru/1.1.0/),
версии — [SemVer](https://semver.org) с суффиксом `-ryazhenka.N`.

## [Unreleased] — Ryazhenka fork bootstrap

### Added
- Изоляционный слой `include/ryazhenka_config.hpp` со всеми
  бренд-строками, URL источников, цветами темы и параметрами retry/log
  в namespace `ryazhenka`.
- Декларации `include/ryazhenka_branding.hpp`,
  `include/ryazhenka_logger.hpp` для будущих этапов.
- `scripts/ryazhenka/apply-patches.sh` — идемпотентный аппликатор
  патчей через reverse-check.
- `scripts/ryazhenka/sync-upstream.sh` — оркестратор daily-sync с
  различимыми exit-кодами на конфликт merge'а и провал патчей.
- `scripts/ryazhenka/check_i18n.py` — diff русского и английского
  i18n keyset'ов.
- `scripts/ryazhenka/pack-release.sh` — упаковка `.nro` в zip с
  обратно-совместимым layout SD-карты.
- 4 GitHub Actions workflows: `build`, `release`, `sync-upstream`,
  `lint`. Заменяют апстримный `main.yml`.
- `docs/sync-policy.md`, `docs/build.md`, двуязычный `README.md`.
- 8 недостающих ключей в `resources/i18n/ru/menus.json` (cheats,
  common, worker).

### Changed
- `Makefile`: `APP_TITLE = "Ryazhenka Updater"`, `APP_AUTHOR =
  Dimasick-git`, `APP_VERSION = 2.23.3-ryazhenka.1`,
  `TARGET = ryazhenka-updater`.
- `include/constants.hpp`: URL для самообновления, nx-links каталога,
  CFW, Hekate, payloads, Joy-Con/Pro профилей перенаправлены на
  репозитории `Dimasick-git/*`. Пути на SD остались
  `/switch/aio-switch-updater/` и `/config/aio-switch-updater/` для
  обратной совместимости с конфигами пользователей.
- `source/main.cpp`: при отсутствии `language.json` дефолтный язык —
  русский (`ryazhenka::kDefaultLanguage`), а не системный.

### Removed
- `.github/workflows/main.yml` — заменён четырьмя focused workflows.
