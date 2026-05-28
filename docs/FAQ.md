# Часто задаваемые вопросы

## Чем Ryazhenka Updater отличается от оригинального AIO-Switch-Updater?

Тремя вещами:

1. **Источники.** URL для самообновления, CFW, sigpatches,
   bootloader'ов указывают на репозитории `Dimasick-git/*`, а не на
   `HamletDuFromage/nx-links`.
2. **Русский по умолчанию.** При отсутствии `language.json` форк
   стартует на русском, а не на системном языке Switch.
3. **Доп. инструменты в Tools tab:** установка пака Ряженки целиком,
   просмотр лога, диагностический отчёт, проверка сети, очистка
   резервных копий.

Всё остальное (Atmosphere, Hekate, cheats, Joy-Con/Pro colors,
internet settings, web browser) — как в апстриме.

## Будет ли форк отставать от апстрима?

Нет. `.github/workflows/sync-upstream.yml` каждые сутки в 03:00 UTC
делает `git merge upstream/master`. Наши правки изолированы как
patch-файлы, которые перенакладываются после merge. Конфликт
открывает issue — мы разрешаем вручную.

## Как обновлять Ryazhenka Updater?

Кнопка "Обновить приложение" в `Tools` появится сама, когда тег
последнего release окажется новее установленной версии. Скачает
`Ryazhenka_AIO.zip` и перезапишет .nro.

## Как откатиться на старую версию?

Идёте в [Releases](https://github.com/Dimasick-git/AIO-Switch-Updater/releases),
качаете нужный `Ryazhenka_AIO.zip` (теги — числовые: 1, 2, 3, …),
распаковываете в корень SD. Все версии остаются доступными.

## Что внутри пака Ряженки?

Полный SD-набор: Atmosphere fork, Hekate, sigpatches, sysmodules
(Sys-clk, nx-ovlloader, RCU и т.д.), homebrew (FPSLocker, EdiZon,
Fizeau, Mission-Control, SwitchWave, PPSSPP), overlay-меню
(Ryazhahand-Overlay), русский интерфейс везде где можно. Список
актуальных компонентов — на странице
<https://github.com/Dimasick-git/Ryzhenka>.

## Можно ли установить пак не через Updater?

Да. Качаете `Ryazhenka_AIO.zip` со страницы
<https://github.com/Dimasick-git/Ryzhenka/releases/latest>,
распаковываете в корень SD. Updater делает то же самое + бэкап
`/atmosphere/` перед перезаписью.

## Куда жаловаться на читы / прошивки?

- Бага самого Updater — issue в этом репо.
- Бага конкретного homebrew (например EdiZon) — issue в его репо
  (`Dimasick-git/EdiZon`).
- Бага конкретного чита — issue в `HamletDuFromage/switch-cheats-db`
  (мы не форкаем эту базу, чтобы не отставать).

## Где Telegram?

[@Ryazhenkacfw](https://t.me/Ryazhenkacfw). Объявления о релизах,
обсуждения, поддержка.

## Что значит "v2.23.3-ryazhenka.N"?

Семантика: `<upstream-версия>-ryazhenka.<наш-патч>`. Версия 2.23.3
взята из последнего стабильного апстрим-релиза AIO-Switch-Updater;
суффикс `.N` инкрементируется при наших значимых изменениях. Это
строка, которую видит код самообновления.

Релизные **git-теги** — отдельная история. Они числовые
(1, 2, 3, …) и инкрементируются на каждый build из main. Это
позволяет иметь `releases/latest` без ручного тегирования.

## А есть ли pre-release / nightly?

Раньше был тег `nightly`. Сейчас он считается legacy — каждый build
сам по себе release с `make_latest=true`. Если хотите попробовать
сборку с конкретного коммита — она доступна как Actions Artifact
(вкладка Actions → run → "ryazhenka-updater-zip" / "ryazhenka-updater-nro").

## Откуда название "Ряженка"?

Ряженка — это кисломолочный продукт. Так же называется русскоязычный
кастом-пак для Switch, под который сделан этот форк. Никакой
скрытой семантики, просто домашнее название.

## Я не нашёл ответа здесь

[`docs/TROUBLESHOOTING.md`](TROUBLESHOOTING.md) — частые проблемы.
Если нет — открывайте issue с шаблоном "Сообщить о баге" и приложите
diag-отчёт.
