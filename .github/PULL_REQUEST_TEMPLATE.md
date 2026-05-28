<!-- Спасибо за PR! Заполните чек-лист ниже. -->

## Что изменилось

<!-- 1-3 предложения о цели изменения. Если PR — фикс, ссылка на issue. -->

## Чек-лист

- [ ] `bash scripts/ryazhenka/apply-patches.sh --check` — все патчи applicable
- [ ] `python3 scripts/ryazhenka/check_i18n.py` — keysets EN/RU parity не нарушены
- [ ] Если правил апстримный файл — патч в `scripts/ryazhenka/patches/` регенерирован через `git diff`
- [ ] `CHANGELOG.md` обновлён (если изменение видимо пользователю)
- [ ] Если новый Tools/About ListItem — добавлены i18n-ключи в `en-US` и `ru`
- [ ] CI build green (https://github.com/Dimasick-git/AIO-Switch-Updater/actions)

## Снимки экрана / выводы

<!-- Если UI-правка или CLI-вывод — прикрепите. -->
