📱 **РЯЖЕНКА UPDATER — ПОЛНАЯ СВОДКА РЕЛИЗОВ**

Текущая версия: **v2.23.8** (от 01.06.2026)

---

✨ **ЧТО НОВОГО ЗА ВСЕ РЕЛИЗЫ:**

🎨 **UI/UX ПРЕОБРАЗОВАНИЕ**
✅ Живая панель мониторинга (Status Tab) — 2×2 сетка графиков: CPU °C, Skin °C, Battery %, SD free % с историей на 2 минуты
✅ Новый дизайн Ряженки — фирменный интерфейс с 4 переключаемыми палитрами
✅ Animated wave background — процедурно генерируемый волновой фон с анимацией
✅ Splash screen — стильный экран загрузки с логотипом
✅ Card-based UI — переработаны вкладки Tools, About, Settings в виде карточек
✅ Плавные переходы между табами с fade-in анимацией

🛠️ **ИНСТРУМЕНТЫ И ФУНКЦИИ**
✅ Sysmodule Manager — включение/отключение системных модулей через toggle boot2.flag
✅ Factory Restore — восстановление /atmosphere/ из резервных копий
✅ Диагностика CFW — здоровье системы (sigpatches, firmware, CFW статус)
✅ Detector staleness sigpatches — предупреждение если патчи старше 14 дней
✅ Логгер — просмотр логов, диаг-дамп, тестирование сети в Tools
✅ Browser-open для ссылок — открытие экосистемы ссылок из About

🎮 **ЗВУК И ТАКТИЛЬНАЯ ОБРАТНАЯ СВЯЗЬ**
✅ Процедурные звуки UI — эффекты через libnx audout
✅ Joy-Con / Pro Controller rumble — вибрация контроллеров
✅ Toggle audio в Settings — включение/отключение звуков с ленивой инициализацией

📡 **СЕТЕВОЕ И ЗАГРУЗОЧНОЕ**
✅ Самообновление приложения — установка новой версии на SD (больше не падает)
✅ Banner via downloadFile — динамическая загрузка баннеров релизов
✅ NX-Cheats интеграция — карточка "Install NXCheatCode" с ежедневными читами
✅ Dynamic resolve URLs — @latest_asset, @download_to маркеры для нестабильных имён файлов
✅ Retry логика — умные переповторы при временных сбоях сети с логированием curl ошибок
✅ Timeout защита — 15-секундный лимит на загрузки + curl таймауты
✅ Cache управление — TTL кэширование каталога nx-links (6 часов для sigpatches)

🎯 **ВВОД И СЕНСОР**
✅ Touchscreen support — минимальный сенсорный ввод (tap = нажатие A)

📦 **УПАКОВКА И РАСПРЕДЕЛЕНИЕ**
✅ Ryazhenka_AIO.zip — удобный архив для распаковки в корень SD
✅ Обратная совместимость — пути остались /switch/aio-switch-updater/ и /config/aio-switch-updater/

---

🐛 **ИСПРАВЛЕНИЯ (25+ BUG FIXES)**
✅ Крах при запуске + infinite splash (StatusTab в конструкторе)
✅ Крах RyazhenkaCard на Settings/Status табах
✅ Крах haptics::pulse (thread при неготовом HID)
✅ Крах банера при запуске (detached fetch)
✅ Таймауты после успешной загрузки
✅ Зависание при ошибке загрузки
✅ Потеря сигнала о завершении загрузки
✅ Crash при self-update (теперь устанавливает на SD root + chain-load)
✅ Старые ссылки в каталоге (cache не обновлялся)
✅ Утечка памяти в логгере + race condition в shutdown
✅ Ошибки UI sound (неправильная обработка)
✅ Проблемы с Settings crashes (haptics threads)
✅ Отключение status/sound toggle в старых версиях

---

🔧 **ИНФРАСТРУКТУРА И CI/CD**
✅ GitHub Actions workflows — build, sync-upstream, lint (заменили апстримный main.yml)
✅ Auto-release — каждый пуш в main автоматически публикует релиз
✅ Numeric tag releases — тэги вида #82, #81, #80...
✅ Патч-система — идемпотентный аппликатор с reverse-check
✅ Daily sync — синхронизация с апстримом с различными exit-кодами
✅ i18n checker — diff русского и английского наборов ключей
✅ Cppcheck job — статический анализ
✅ devkitA64 pin — версия 20260215 для совместимости с SwitchWave

---

📚 **ДОКУМЕНТАЦИЯ**
✅ docs/dashboard.md — описание панели мониторинга
✅ docs/sysmodule-manager.md — управление модулями
✅ docs/factory-restore.md — восстановление системы
✅ docs/sync-policy.md — политика синхронизации с апстримом
✅ docs/build.md — инструкция по сборке
✅ TROUBLESHOOTING.md и FAQ на русском
✅ Двуязычный README

---

🌍 **ЛОКАЛИЗАЦИЯ**
✅ 228 ключей в i18n (русский ↔ английский)
✅ Дефолт язык — русский (вместо системного)
✅ +16 новых ключей для Status Tab, sysmodule manager, factory restore

---

📊 **СТАТИСТИКА РЕЛИЗОВ**

v2.23.8 (01.06) — Fix самообновления (install on SD root + chain-load)
v2.23.7 (01.06) — Refresh nx-links cache на обновление
v2.23.6 (01.06) — Cheats DB, firmware, payloads, self-update, crash-safe
v2.23.5 (01.06) — Убрать timeout после успешной загрузки
v2.23.4 (31.05) — Retry + логирование curl ошибок
v2.23.3 (29.05) — Полная переработка UI, Sound/Haptics, Live Dashboard

---

🎯 **ФОКУС РАЗВИТИЯ**
1. Стабильность — 25+ критических крахей исправлено
2. UX — полный переделан интерфейс с Ряженкой дизайном
3. Мониторинг — живая панель системных метрик
4. Управление — инструменты для CFW, модулей, восстановления
5. Совместимость — обратная совместимость с конфигами пользователей

---

🔗 Скачать: Ryazhenka_AIO.zip или положить ryazhenka-updater.nro в /switch/aio-switch-updater/
📌 Версия на SD: aio-switch-updater.nro (для совместимости с внутренним апдейтером)
