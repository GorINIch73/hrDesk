# HR-Desk

Кадровая база данных сотрудников: приём/перевод/увольнение, приказы, табель, отпуска, больничные, отчёты и резервное копирование.

- **Стек:** C++20 · CMake · Dear ImGui (docking) · GLFW + OpenGL3 · SQLite3 · LibreOffice (headless) · Linux
- **Интерфейс:** плотный, табличный, dock-панели, кириллица и монохромные иконки, событийные анимации без нагрузки на батарею.

> Проект на стадии планирования/разработки. Подробности — в [`PLAN.md`](PLAN.md).

---

## Основные возможности

- Ведение штата, подразделений и штатного расписания
- Кадровые документы и приказы с state-machine (`DRAFT → APPROVED → ARCHIVED`) и блокировкой правки
- Отпуска (график T-7), больничные, табель (T-12)
- Валидация данных и аудит изменений (`audit_log`)
- Экспорт в ODT/ODS через шаблоны и LibreOffice
- Резервное копирование (WAL, `sqlite3_backup`, ротация, восстановление)
- Низкое потребление CPU: событийный рендер, idle-режим, эконом-режим для ноутбуков

---

## Требования

Нужны только базовые инструменты — зависимости (`imgui`, `ImGuiFileDialog`, `glfw`, `sqlite`, `minizip`) **клонируются и компилируются статически самим CMake** (FetchContent), системные `-devel` пакеты не требуются.

- CMake ≥ 3.24
- Компилятор с C++20 (gcc/clang)
- git, make
- OpenGL/GLX (драйвер графики)
- Опционально: LibreOffice (`soffice`) для экспорта

---

## Сборка

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

## Запуск

```bash
./build/src/hrdesk                                   # стартовый диалог: новая база / открыть / восстановить
./build/src/hrdesk --seed demo.db --employees 5000  # сгенерировать демо/нагрузочную базу
```

## Тесты

```bash
ctest --test-dir build --output-on-failure
```

---

## Структура проекта

```
src/
  app/        Application, FormManager, Settings, EventBus
  ui/         Theme, Fonts, Dockspace, widgets/, views/
  domain/     Employee, Department, CadreDocument, Order, Validator ...
  db/         Database, Schema/, Repositories/, BackupService
  seed/       SeedDb, LoadGen (генератор тестовой/нагрузочной базы)
  export/     OdtWriter, OdsWriter, TemplateEngine, LibreOfficeBridge
  report/     отчёты
  util/       date, ru_locale, csv, hash, logger, threadpool
tests/        unit (ctest) + smoke + load
assets/       fonts/, templates/, icons/
third_party/  vendored зависимости (статическая сборка)
```

---

## Разработка (кратко)

1. Hello World: окно ImGui + загрузка шрифтов (кириллица/иконки).
2. Меню и каркас UI + `FormManager` (динамические формы).
3. Схема БД (DDL) + консольная утилита генерации тестовой базы.
4. Эталонная (самая сложная) форма master–detail — доводим до эталона.
5. Остальные формы — по аналогии с эталоном.

> На начальной разработке схема пересоздаётся заново (миграций нет) — временная мера; после релиза 1.0 вернёмся к миграциям.

---

## Лицензия

Лицензия будет добавлена позже (например, MIT / GPL — уточнить).
