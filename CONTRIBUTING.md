# Гайд для контрибьюторов

Спасибо, что хотите внести вклад в Ryazhenka Updater. Этот документ
описывает рабочий процесс форка и нюансы, специфичные для проекта.

## Общая структура

- `source/` и `include/` — C++23 исходники. Файлы с префиксом
  `ryazhenka_` — наши, остальные — апстрим.
- `scripts/ryazhenka/` — инфраструктура форка (apply-patches, sync,
  i18n-чек, pack-release).
- `scripts/ryazhenka/patches/` — наши точечные правки апстримных
  файлов в формате unified-diff. Восстанавливаются на каждый sync с
  апстримом.
- `.github/workflows/` — CI: `build.yml`, `sync-upstream.yml`,
  `lint.yml`.
- `docs/` — `build.md`, `sync-policy.md`, `TROUBLESHOOTING.md`, `FAQ.md`.

## Перед первым PR

1. Прочитайте [`docs/sync-policy.md`](docs/sync-policy.md) — там
   объяснено, какие изменения куда идут (новый файл vs. патч).
2. Прочитайте [`docs/build.md`](docs/build.md) — как собрать локально.
3. Установите [devkitPro](https://devkitpro.org/wiki/Getting_Started)
   или используйте docker `devkitpro/devkita64:latest`.

## Если меняете апстримный файл

Любой файл, которого нет в `ryazhenka_*` префиксе, считается
апстримным. Алгоритм:

1. Внесите правку в файл напрямую (через `Edit` или редактор).
2. Сгенерируйте patch:
   ```bash
   git diff 3d38eaa -- <путь> > scripts/ryazhenka/patches/NN-name.patch
   ```
   где `3d38eaa` — текущий `cat .upstream-base`, а `NN` — следующий
   свободный номер.
3. Проверьте roundtrip:
   ```bash
   bash scripts/ryazhenka/apply-patches.sh --reset
   bash scripts/ryazhenka/apply-patches.sh
   ```
   Оба должны завершиться кодом 0, и git working tree должен совпадать
   с состоянием до reset.
4. Коммитьте файл и патч **одним коммитом**.

## Если добавляете i18n-строку

1. Добавьте ключ **одновременно** в `resources/i18n/en-US/menus.json`
   и `resources/i18n/ru/menus.json`. Если других переводов вам не
   хватает на остальные 11 локалей — оставляйте англ. fallback.
2. Запустите `python3 scripts/ryazhenka/check_i18n.py`. Должно
   завершиться кодом 0.
3. Используйте ключ через `"menus/foo/bar"_i18n`.

## Если добавляете C++ модуль

- Имя файла обязательно с префиксом `ryazhenka_` — это:
  - попадает в `Makefile` `*.cpp` glob автоматически;
  - не конфликтует с апстримными именами при merge;
  - не попадает под clang-format апстрима.
- Создайте `include/ryazhenka_<name>.hpp` с doxygen header
  (`@file`/`@brief`/`@author`) и парный
  `source/ryazhenka_<name>.cpp`.

## Стиль кода

- C++23. `nlohmann::json`, `<filesystem>`, `<thread>`, `<chrono>` —
  свободны для использования.
- Не подключаем новые libnx-зависимости; всё что нужно — уже линкуется
  (`-lcurl`, `-lminizip`, `-lmbedcrypto`, `-lmbedtls`, `-lz`).
- Логируйте через `ryazhenka::log::{trace,debug,info,warn,error}`, а
  не напрямую через `brls::Logger` — наша обёртка делает SD-зеркало.
- Не валите приложение: catch'ите exception на границе любого worker
  потока и логируйте.

## Чек-лист PR

См. [`.github/PULL_REQUEST_TEMPLATE.md`](.github/PULL_REQUEST_TEMPLATE.md).

## Лицензия

GPL-3.0, как и апстрим. Соглашаясь сделать PR, вы публикуете свой
вклад под этой же лицензией.
