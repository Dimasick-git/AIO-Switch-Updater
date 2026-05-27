# Политика синхронизации с апстримом

## Цель

Держать форк `Dimasick-git/AIO-Switch-Updater` максимально близко к
`HamletDuFromage/aio-switch-updater@master` так, чтобы новые фичи и багфиксы
апстрима подтягивались автоматически, но наши кастомизации под Ряженку не
терялись и не требовали ручного разрешения конфликтов на каждом коммите.

## Архитектура защиты

Наши изменения распределены на два класса.

### 1. Новые файлы (≈ 90% диффа)

Файлы, которых нет в апстриме — не могут конфликтовать ни с одним merge.
Сюда входят:

- `include/ryazhenka_config.hpp` — все Ряженка-специфичные URL, пути,
  бренд-строки, параметры темы и retry/log как `constexpr` в namespace
  `ryazhenka`.
- `include/ryazhenka_*.hpp`, `source/ryazhenka_*.cpp` — наш собственный код
  (логгер, branding, pack installer и т.п.). Префикс `ryazhenka_` гарантирует
  отсутствие коллизий имен и автоматический pickup стандартным `*.cpp` glob
  в апстримном `Makefile`.
- `scripts/ryazhenka/*` — вся sync-инфраструктура.
- `.github/workflows/{build,release,sync-upstream,lint}.yml` — наш CI
  (заменяет апстримный `main.yml`).
- `.upstream-base`, `docs/*`, `CHANGELOG.md`.

### 2. Патчи апстримных файлов (≈ 10%)

Точечные правки в `Makefile`, `include/constants.hpp`, `source/main.cpp` и
будущих файлах сводятся к замене конкретных литералов и добавлению
одного-двух `#include`. Каждая такая правка коммитится в репозиторий
ДВАЖДЫ:

1. В виде применённого изменения непосредственно в файле (то, что вы
   видите при `git diff main..3d38eaa`).
2. В виде unified-diff в `scripts/ryazhenka/patches/NN-name.patch`.

Эта дубликация — фича, не баг. Применённое изменение нужно чтобы код
просто работал; патч-файл нужен чтобы `sync-upstream.yml` мог переналожить
его после `git merge upstream/master`, который частично или полностью
откатит наши правки (если апстрим тронул те же строки).

## Идемпотентность apply-patches.sh

Логика проверки в `scripts/ryazhenka/apply-patches.sh`:

1. `git apply --check $patch` — если возвращает 0, патч ещё не применён и
   накатывается.
2. Если предыдущая команда упала, пробуем `git apply -R --check $patch` —
   если возвращает 0, патч уже применён (мы успешно «откатываем» его в
   проверочном режиме), молча пропускаем.
3. Если оба варианта упали — это конфликт, скрипт падает с exit 1 и просит
   человека запустить `git apply --3way` вручную.

Это означает: запускать `apply-patches.sh` можно сколько угодно раз,
поведение не зависит от того, был ли уже накатан патч или нет.

## Что делает sync-upstream.yml каждую ночь

Cron `0 3 * * *` (UTC) запускает `scripts/ryazhenka/sync-upstream.sh`,
который:

1. Добавляет remote `upstream` (если ещё не добавлен) и делает
   `git fetch upstream --tags --prune`.
2. Сравнивает HEAD апстрима с `.upstream-base`. Если совпадает — выходит
   с кодом 0, делать нечего.
3. Делает `git merge --no-ff upstream/master`.
   - Если merge падает с конфликтом — `git merge --abort` и выход с кодом
     **10** (workflow открывает GitHub Issue с лейблом
     `upstream-conflict`).
4. Запускает `apply-patches.sh`, который переналожит наши правки на
   возможно изменённый апстримом контент.
   - Если хотя бы один патч не накатывается — выход с кодом **11**
     (workflow открывает отдельное issue, у патча нужно регенерировать
     контекст: внести правку вручную и пересоздать `.patch` через
     `git diff`).
5. Обновляет `.upstream-base`, делает коммит и `git push origin main`.

В обоих сценариях с конфликтом ветка `main` остаётся в чистом состоянии —
никаких полу-применённых правок туда не попадает.

## Когда патч начинает «протухать»

Если апстрим переименовал константу или переместил блок, диапазон строк в
нашем `.patch` может перестать находиться в файле. `git apply --3way`
часто справляется, но не всегда. Что делать когда такое происходит:

1. `bash scripts/ryazhenka/apply-patches.sh --reset` — снимает все наши
   патчи.
2. Локально вручную внести изменения, эквивалентные тому что было в
   `NN-name.patch`, под новый контекст.
3. `git diff <file> > scripts/ryazhenka/patches/NN-name.patch` —
   регенерировать сам файл патча.
4. `bash scripts/ryazhenka/apply-patches.sh --check` — убедиться что
   reverse-check проходит.
5. Закоммитить.

## Что НЕЛЬЗЯ форкать (политика)

- `lib/borealis` — общий UI-фреймворк, активно используется десятками
  homebrew-проектов. Форк = неподдерживаемые cherry-pick'и багфиксов.
- `TegraExplorer` — низкоуровневый, изменения редкие. Форк только если
  апстрим перестанет принимать критичные правки.
- `HamletDuFromage/switch-cheats-db` — база читов в десятки МБ,
  обновляется community ежедневно. Форк = вечное отставание базы.

Если кто-то из этих апстримов «протухнет» (≥ 6 месяцев без коммитов и
открытые critical issues), то переключаемся точечно через
`git submodule set-url` (для подмодулей) или меняем константы в
`include/ryazhenka_config.hpp` (для cheats-db).

## Ручной запуск sync

```bash
# Локально, на любом чистом working tree main:
bash scripts/ryazhenka/sync-upstream.sh

# Из GitHub:
# Actions → "Sync upstream" → Run workflow
```

## Чек-лист при изменении апстрим-файла под Ряженку

1. Внести правку в файле через обычный `Edit` / редактор.
2. `git diff <file> > scripts/ryazhenka/patches/NN-name.patch` —
   с уникальным номером после имеющихся (`01-`, `02-`, …).
3. `bash scripts/ryazhenka/apply-patches.sh --check` — должно выводить
   "applicable" для нового патча и "already applied" для старых.
4. `bash scripts/ryazhenka/apply-patches.sh --reset && bash
   scripts/ryazhenka/apply-patches.sh` — round-trip test.
5. Закоммитить файл и патч одним коммитом.
