# Changelog

Формат — [Keep a Changelog](https://keepachangelog.com/ru/1.1.0/),
версии — [SemVer](https://semver.org) с суффиксом `-ryazhenka.N`.

## [Unreleased] — Live dashboard + операционные модули

### Added

- **Status tab** — новая первая вкладка приложения с живой панелью
  показателей. 2×2 сетка графиков: CPU °C, Skin °C, Battery %, SD
  free %. NanoVG-отрисовка, обновление ~10 Гц, кольцевой буфер на 120
  семплов (2 минуты истории). Sampler-поток (`psm`/`ts` + filesystem)
  стартует в `willAppear` и останавливается в `willDisappear`. См.
  `docs/dashboard.md`.
- **Sysmodule Manager** — `Tools → Управление sysmodules`: список всех
  модулей в `/atmosphere/contents/<tid>/` с toggle-ом по
  `flags/boot2.flag`. Семь системно-критичных TID помечены `[SYS]` и
  toggle для них запрещён. Имена читаются из `toolbox.json`. См.
  `docs/sysmodule-manager.md`.
- **Factory Restore** — `Tools → Восстановление /atmosphere/`: список
  снимков `/aio-backup/atmosphere-*` (новые сверху, размер и
  количество файлов рядом). Восстановление двухфазное: защитный
  pre-snapshot текущего /atmosphere/ → wipe + recursive copy. См.
  `docs/factory-restore.md`.
- **Sigpatches staleness detector** — в `About → Состояние CFW`
  появилась строка `sigpatches актуальность` со сравнением даты
  локальных патчей и `published_at` последнего релиза пака. WARN если
  разница ≥ 14 дней; кэш JSON на 6 часов.
- **Auto health re-check** — после установки пака, toggle sysmodule и
  factory restore автоматически прогоняется `health::run()` и при
  WARN/ERROR показывается `Application::notify` toast.
- **Dashboard quick-link** — `Tools → Открыть монитор системы`.
- Документация: `docs/dashboard.md`, `docs/sysmodule-manager.md`,
  `docs/factory-restore.md`. Дополнения в `docs/TROUBLESHOOTING.md`.

### Internal

- Новые модули: `ryazhenka_metrics`, `ryazhenka_chart_view`,
  `ryazhenka_status_tab`, `ryazhenka_sysmodule_manager`,
  `ryazhenka_sigpatches`, `ryazhenka_factory_restore`.
- `ryazhenka::sha256::ofBuffer` для in-memory хэширования.
- `ryazhenka::health::runAndNotifyIfDegraded` — централизованный
  post-action hook.
- В `ryazhenka_config.hpp`: `kSigpatchesReleasesUrl`,
  `kSigpatchesRemoteCachePath`, `kSigpatchesStaleThresholdDays`,
  `kSigpatchesCacheTtlHours`, `kAutoHealthAfterActions`,
  `kDashboardSampleHz`.
- Новый patch `scripts/ryazhenka/patches/07-main-frame.patch` —
  StatusTab регистрируется первым табом в MainFrame.
- i18n: +16 ключей (status_tab, status_header, sysmodule_*,
  open_dashboard, factory_restore, restore_*); parity ru ↔ en-US
  держится на 228 ключах.

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
- 3 GitHub Actions workflows: `build`, `sync-upstream`, `lint`.
  Заменяют апстримный `main.yml`. `build.yml` сам публикует release
  c числовым тэгом (`${{ github.run_number }}`) после каждого
  пуша в `main`, `make_latest=true`.
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
