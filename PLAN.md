# PLAN.md — Кадровая база данных сотрудников (HR-Desk)

**Стек:** C++20 · CMake · Dear ImGui (docking) · GLFW + OpenGL3 · SQLite3 · LibreOffice (headless) · Linux  
**Назначение:** ведение кадровой базы, приём/перевод/увольнение, приказы, табель, отпуска, больничные, отчёты, резервное копирование.

> `PLAN.md` — **чистый проект**: только функции, архитектура и решения. Статус выполнения, прогресс и журнал изменений ведутся отдельно в [`PROGRESS.md`](PROGRESS.md).

---

## 1. Принципы

| Принцип | Реализация |
|---|---|
| Минимализм и плотность | тёмная/светлая тема, компактный шрифт, таблицы вместо карточек, dock-панели |
| Табличная работа | ImGui Tables (resizable/sortable/sortable-multi, clip), виртуализация строк, пагинация |
| Групповые операции | выделение строк (checkbox + shift-диапазон), пакетные приказы/проверки/печать |
| Целостность данных | валидатор перед сохранением, FK, CHECK, UNIQUE, WAL, транзакции |
| Неизменяемость приказов | state-machine документов: `DRAFT → APPROVED → ARCHIVED`, блокировка правки, аудит |
| Автосохранение | debounce 800 мс после изменения поля, commit в фоновом потоке, индикатор «сохранено» |
| Низкая нагрузка CPU | событийный рендер (`glfwWaitEvents`), cap FPS, кэш строк, prepared statements, ленивые запросы |
| Русский текст | загрузка TTF с кириллическими глифами при старте + `u8`-строки + корректная сортировка |
| Ссылки только по ID | UI-модели хранят `id` (PK) как единственный ключ; индекс/позиция в списке — лишь временный рендер, не используется для CRUD/связей (защита от рассинхронизации при сортировке, фильтрах, пагинации, удалении) |
| Подтверждение операций | групповые и критичные действия (удаление, увольнение, утверждение/отмена группы, восстановление из бэкапа) — диалог подтверждения с перечнем затронутых записей и отменой |
| Многострочный ввод | `MultilineField` (UTF-8/кириллица, автоперенос по ширине, Ctrl+Enter, счётчик символов) для причин, примечаний, тела приказа |
| Привлекательность без нагрузки | цветные акценты, кастомные виджеты и событийные анимации (см. §4.6); в idle — 0 перерисовок, эконом-режим для батареи |
| Эталонная форма | сначала доводим одну сложную форму до приемлемого качества, затем воспроизводим по аналогии остальные (см. §11) |
| Динамические формы | `FormManager`: создание/уничтожение/закрытие всех форм, сохранение изменений всех форм (см. §4.8) |

---

## 2. Архитектура

```
src/
  main.cpp                 инициализация, цикл рендера (event-driven)
  app/
    Application.{h,cpp}    композиция, жизненный цикл, командный диспетчер
    FormManager.{h,cpp}    динамические формы: создать/уничтожить/закрыть все, сохранить изменения всех, реестр открытых форм
    Settings.{h,cpp}       config.toml/json: тема, шрифт, пути, FPS, бэкапы
    EventBus.{h,cpp}       события (DocumentApproved, EmployeeChanged, ...)
  ui/
    Theme.{h,cpp}          Dark/Light/HighContrast, ImGuiStyle, иконки (Font Awesome 6 + монохромный эмодзи-шрифт)
    Fonts.{h,cpp}          загрузка/подбор кириллических шрифтов
    Dockspace.{h,cpp}      компоновка панелей, сохранение layout.ini
    widgets/               DataTable, FilterBar, Toolbar, Form, DateField, Money, Tag, Toast,
                           ComboBox (поиск+фильтр), MultilineField (автоперенос), ConfirmDialog, StatusBar,
                           Button (accent/hover), Badge, Chip, ProgressRing, Sparkline, Splitter (master–detail)
    views/
      EmployeesView        штат/список, поиск, фильтры, групповые операции
      StructureView        подразделения + штатное расписание
      DocumentsView        кадровые документы и приказы
      VacationView         график отпусков (T-7) + отпуска
      TimesheetView        табель (T-12)
      SickLeaveView        больничные листы
      ReportsView          отчёты
      AdminView            база, бэкап, проверка, настройки, журнал
      AuditView            журнал изменений
  domain/
    Employee, Department, Position, StaffUnit (ставка)
    CadreDocument (HIRE/TRANSFER/DISMISS/VACATION/SICK/OTHER)
    Order (приказ), TimesheetEntry, VacationPlan, SickLeave
    DocumentStateMachine, Validator, Rules
  db/
    Database.{h,cpp}       sqlite3 wrapper, WAL, prepared stmt cache, создание схемы
    Schema/0001_init.sql   единый DDL (создаёт таблицы/индексы при новой БД; миграций нет — временно, до релиза 1.0)
    Repositories/          EmployeeRepo, DocumentRepo, TimesheetRepo ...
    BackupService.{h,cpp}  sqlite3_backup, ротация, restore, VACUUM/ANALYZE
  seed/
    SeedDb.{h,cpp}         генератор тестовой/демо-базы (детерминированный, по seed)
    LoadGen.{h,cpp}        нагрузочный генератор (N сотрудников/документов/табель)
  export/
    OdtWriter / OdsWriter  (zip + content.xml)
    TemplateEngine         подстановка {{placeholder}}, циклы {{#each}}
    LibreOfficeBridge      soffice --headless --convert-to
  report/                  отчёты (табличные + печатные)
  util/                    date, ru_locale, csv, hash, logger, threadpool
tests/                     unit (ctest) + smoke (интеграция с sqlite/odt) + load (нагрузка через seed/)
assets/
  fonts/  NotoSans-Regular.ttf, NotoSans-Bold.ttf, DejaVuSans.ttf, FontAwesome6-Free.ttf, NotoEmoji-Regular.ttf
  templates/  order_hire.odt, order_transfer.odt, order_dismiss.odt, t7.odt, t12.ods, ...
  icons/
```

Слои: `ui → app → domain → db/export` (UI не знает о sqlite; репозитории не знают об ImGui).

---

## 3. Модель данных (SQLite)

```sql
PRAGMA journal_mode=WAL; PRAGMA foreign_keys=ON; PRAGMA synchronous=NORMAL;

CREATE TABLE dept(id INTEGER PRIMARY KEY, code TEXT UNIQUE, name TEXT NOT NULL,
                  parent_id INTEGER REFERENCES dept(id), head_emp_id INTEGER, archived INTEGER DEFAULT 0);

CREATE TABLE position(id INTEGER PRIMARY KEY, code TEXT UNIQUE, name TEXT NOT NULL, class TEXT);

CREATE TABLE staff_unit(                    -- штатное расписание
  id INTEGER PRIMARY KEY, dept_id INTEGER NOT NULL REFERENCES dept(id),
  position_id INTEGER NOT NULL REFERENCES position(id),
  headcount REAL NOT NULL DEFAULT 1,        -- ставок
  as_of_date TEXT NOT NULL,                 -- действует с
  rate_min REAL, rate_max REAL);

CREATE TABLE employee(
  id INTEGER PRIMARY KEY,
  tab_no TEXT UNIQUE NOT NULL,              -- табельный номер
  last_name TEXT NOT NULL, first_name TEXT NOT NULL, middle_name TEXT,
  birth_date TEXT NOT NULL, birth_place TEXT,
  inn TEXT, snils TEXT, passport_series TEXT, passport_number TEXT,
  passport_issued TEXT, passport_date TEXT,
  address_reg TEXT, address_fact TEXT, phone TEXT, email TEXT,
  hire_date TEXT NOT NULL, dismiss_date TEXT,
  status TEXT NOT NULL DEFAULT 'ACTIVE',    -- ACTIVE|DISMISSED|SUSPENDED|ARCHIVED
  employment_type TEXT DEFAULT 'FULL_TIME', -- FULL_TIME|PART_TIME|CIVIL|SEASON
  note TEXT, created_at TEXT, updated_at TEXT);

CREATE TABLE assignment(                    -- должность/подразделение по периодам
  id INTEGER PRIMARY KEY, employee_id INTEGER NOT NULL REFERENCES employee(id),
  dept_id INTEGER NOT NULL REFERENCES dept(id),
  position_id INTEGER NOT NULL REFERENCES position(id),
  staff_unit_id INTEGER REFERENCES staff_unit(id),
  start_date TEXT NOT NULL, end_date TEXT,  -- NULL = по настоящее
  rate REAL DEFAULT 1, salary REAL, is_primary INTEGER DEFAULT 1);

CREATE TABLE cadre_doc(                     -- кадровое событие = основание приказа
  id INTEGER PRIMARY KEY,
  kind TEXT NOT NULL,                       -- HIRE|TRANSFER|DISMISS|VACATION|SICK|TRAVEL|MATERNITY|OTHER
  employee_id INTEGER NOT NULL REFERENCES employee(id),
  doc_date TEXT NOT NULL, start_date TEXT, end_date TEXT,
  reason TEXT, base_doc TEXT,               -- заявление/соглашение
  status TEXT NOT NULL DEFAULT 'DRAFT',     -- DRAFT|APPROVED|ARCHIVED|CANCELED
  approved_at TEXT, approved_by TEXT,
  created_at TEXT DEFAULT (datetime('now')), updated_at TEXT);

CREATE TABLE doc_payload(doc_id INTEGER PRIMARY KEY REFERENCES cadre_doc(id),
                         key TEXT NOT NULL, value TEXT, UNIQUE(doc_id,key)); -- гибкие поля

CREATE TABLE orders(
  id INTEGER PRIMARY KEY, doc_id INTEGER NOT NULL REFERENCES cadre_doc(id),
  number TEXT NOT NULL, date TEXT NOT NULL, kind TEXT NOT NULL,
  title TEXT, body TEXT, signature TEXT,
  status TEXT NOT NULL DEFAULT 'DRAFT',     -- блокируется при APPROVED
  UNIQUE(kind, date, number));

CREATE TABLE vacation_plan(id INTEGER PRIMARY KEY, employee_id INTEGER NOT NULL REFERENCES employee(id),
  year INTEGER NOT NULL, part INTEGER NOT NULL DEFAULT 1, start_date TEXT NOT NULL,
  days INTEGER NOT NULL, status TEXT DEFAULT 'PLANNED', doc_id INTEGER REFERENCES cadre_doc(id),
  UNIQUE(employee_id, year, part));

CREATE TABLE vacation_balance(employee_id INTEGER PRIMARY KEY REFERENCES employee(id),
  year INTEGER NOT NULL, entitlement_days REAL DEFAULT 28, used_days REAL DEFAULT 0);

CREATE TABLE sick_leave(id INTEGER PRIMARY KEY, employee_id INTEGER NOT NULL REFERENCES employee(id),
  reg_no TEXT, issue_date TEXT, start_date TEXT NOT NULL, end_date TEXT, days INTEGER,
  grade TEXT, workplace TEXT, care_type TEXT, benefit REAL, paid_from_day INTEGER DEFAULT 4,
  status TEXT DEFAULT 'DRAFT');

CREATE TABLE timesheet(
  id INTEGER PRIMARY KEY, employee_id INTEGER NOT NULL REFERENCES employee(id),
  day TEXT NOT NULL, code TEXT NOT NULL,    -- Я|В|ОТ|ОТ16|НН|Б|РВ|ГП|К|О
  hours REAL DEFAULT 0, note TEXT, UNIQUE(employee_id, day));

CREATE TABLE audit_log(id INTEGER PRIMARY KEY, ts TEXT DEFAULT (datetime('now')),
  user TEXT, entity TEXT, entity_id INTEGER, action TEXT, diff TEXT);

CREATE TABLE settings(key TEXT PRIMARY KEY, value TEXT);
CREATE INDEX idx_emp_status ON employee(status, last_name);
CREATE INDEX idx_assign_emp ON assignment(employee_id, start_date);
CREATE INDEX idx_doc_emp ON cadre_doc(employee_id, doc_date);
CREATE INDEX idx_ts_day ON timesheet(day, employee_id);
```

Справочники (предзаполняемые): причины увольнения, виды отпусков, коды табеля, категории больничных, шаблоны приказов.

---

## 4. UI / UX

### 4.1 Компоновка (dockspace)
```
┌ Toolbar: [База] [Справочники] [+Приём][+Перевод][+Увольнение][+Отпуск][+Больничный] [Проверка] [Отчёт] [Бэкап] [Тема]
├ Nav (слева)      │  Таблица (центр)                     │ Инспектор (справа)
│ Штат             │  sortable/resizable колонки, чекбоксы│ Редактирование записи
│ Подразделения    │  фильтры, поиск (fuzzy), счётчик     │ Карточка сотрудника
│ Приказы          │  групповые кнопки над таблицей       │ История/аудит
│ Отпуска/Табель   │  строки-документы со статус-бейджем  │ Предпросмотр приказа
└ Status bar: записей · фильтр · «сохранено 12:04» · WAL · CPU/FPS
```
- `Ctrl+K` — командная палитра; `Ctrl+F` — поиск; `F2` — редактировать; `Del` — пометить; `Ctrl+Enter` — утвердить; `Esc` — закрыть форму.
- Правый клик — контекстное меню (копировать табельный номер, создать приказ из документа, дублировать).
- Формы: компактные, 2 колонки, авто-подсказки (автодополнение должностей/подразделений), маска дат `ДД.ММ.ГГГГ`, поля денег с разрядками.
- Статус-бейджи: `черновик` (серый), `утверждён` (зелёный), `отменён` (красный), `ошибка валидации` (жёлтый + всплывающая подсказка).
- Блокировка: при `APPROVED` все поля `BeginDisabled(true)`, кнопка «Разблокировать» → требует причину → запись в `audit_log`.
- Иконки: в меню, на кнопках toolbar, в пунктах контекстного меню и статус-бейджах — из монохромного эмодзи-шрифта (см. §5.1), без цветовой зависимости от темы (ЧБ-глифы красятся токеном `accent`/`text_dim`).

### 4.2 Табличная часть
- `ImGui::BeginTable` с `TableFlags_Resizable | Reorderable | Sortable | RowBg | BordersInnerH | HideHeaders`.
- Сортировка: клик по заголовку = сортировка, `Shift+клик` = многоуровневая (ФИО → подразделение → дата); стабильная сортировка (при равенстве сохраняется порядок), индикатор направления (▲/▼ из эмодзи-шрифта); сортировка по кириллице через `strcoll`/locale, даты — по ISO.
- Виртуализация: вывод только видимых строк (clipper), серверная пагинация `LIMIT/OFFSET` при > 2000 записей.
- Ссылки на строки: каждая строка несёт `id` (PK); чекбокс/выделение/групповые операции работают по `id`, а не по позиции в векторе (позиция меняется при сортировке/фильтрах).
- Колонки: чекбокс, табельный №, ФИО, подразделение, должность, дата приёма, статус, отметки (🔒/⚠).
- Массовые операции: утвердить/отменить группу, назначить отпуск группе, сформировать табель за месяц, пакетный экспорт — каждая через `ConfirmDialog` (см. §4.4).

### 4.3 Комбобоксы и поля ввода
- `ComboBox` с поиском: открывается как popup + строка фильтра, подссвет совпадений, клавиши ↑/↓/Enter/Esc, группировка (подразделения/должности/причины), автодополнение, «создать новый» при отсутствии совпадения; источник — справочник или репозиторий (леникая подгрузка).
- `MultilineField`: `ImGui::GetIO().WantCaptureText` + `InputTextMultiline` с автопереносом по ширине контейнера, корректная работа UTF-8/кириллицы (без обрезки по байтам), Ctrl+Enter = сохранить, Esc = отмена, счётчик символов, сохранение в `doc_payload`/`note`.
- `DateField`/`Money` — маски, локализация, валидация на лету (подсветка ошибки до сохранения).

### 4.4 Подтверждение и статус
- `ConfirmDialog` для критичных/групповых операций: показывает список затронутых `id`, количество, необратимость; кнопки «Подтвердить/Отменить`; опция «больше не спрашивать» для повторяющихся действий.
- `StatusBar` (внизу dockspace): текущая операция + прогресс (worker через `EventBus`), «сохранено HH:MM», WAL, FPS/CPU, ошибки/Toast; длительные операции — отменяемая прогресс-строка.

### 4.5 Отчёты
Штатное расписание (план/факт/вакансии), кадровый состав, движение (приём/увольнение), списочный состав, график отпусков, использованные дни, больничные за период, табель-факт vs норма времени, просроченные документы (нет приказа), неполные карточки.

### 4.6 Привлекательность: акценты, кастомные виджеты, анимации
Цель — «живой», читаемый интерфейс **без лишней нагрузки на CPU/GPU и батарею** (ноутбук).

- **Кастомные виджеты** (`ui/widgets/`): стилизованные кнопки (accent-заливка, hover/active-состояния), чипы/теги, статус-бейджи с цветным фоном, прогресс-кольца и бары, sparkline/мини-графики движения кадров, карточка сотрудника с акцентной шапкой, подсвеченные поля с фокус-свечением.
- **Цветовые акценты:** семантические токены темы (`ok/warn/err/accent/sel`) для статусов, ошибок валидации, активных фильтров, выбранных строк; подсветка изменённых/неполных полей; контрастные разделители секций.
- **Анимации (субтильные, событийные):** fade-in/out меню и Toast, slide-in уведомления, плавное раскрытие панелей, hover-эффекты, «пульс» сохранения, прогресс-анимация операций. Длительность 120–250 мс, easing — линейный/ease-out.
- **Экономия (ноутбук/батарея):**
  - анимации **только по событию**, не постоянные лупы; в idle — 0 перерисовок (см. §9), анимация завершается и цикл уходит в `glfwWaitEvents`;
  - флаг `ReducedMotion`/`Animations=off` в настройках (для батареи и доступности); авто-отключение при низком заряде;
  - анимации считаются «WantRedraw»-событиями: пока идёт анимация — активный FPS, после — idle;
  - никаких шейдеров/текстур ради украшений — только ImGui-примитивы и цвета; минимум `DrawList`-операций;
  - переключение темы не пересобирает шрифты/атлас (см. §5.1).
- **Профиль энергоэффективности:** в status bar — FPS/CPU; опция «Эконом-режим» (отключает анимации, снижает FPS, упрощает виджеты).

### 4.7 Сложные формы (master–detail с перемещаемым разделителем)
Сложные экраны (карточка сотрудника, приказ, отпуск, табель) строятся как **master–detail**:
```
┌ Список-таблица (master) │ активный разделитель │ Редактор выбранной записи (detail)
│ сортируемая, поиск      │ перемещаемый (drag)  │ вкладки: Данные / Документы / История / Предпросмотр
│ выбор строки (id)       │                      │ соседние панели: справочники, валидация, аудит
└─────────────────────────┴──────────────────────┴──────────────────────────────────
```
- **Перемещаемый разделитель** (draggable splitter, как в dockspace): тянется мышью, фиксирует пропорцию master/detail; положение сохраняется в `layout.ini`; двойной клик = сброс; минимум/максимум ширины панелей.
- **Активная запись** выбирается в списке (по `id`, см. §4.2) → редактор и связанные панели (документы, валидация, аудит, предпросмотр приказа) подгружаются лениво и обновляются при смене выбора.
- **Доступность панелей:** соседние панели (справочники, фильтры, статус, подсказки валидации) группируются вокруг редактора; сворачиваемые секции; горячие клавиши `←/→` — переход по записям, `Tab` — переключение вкладок detail.
- Разделитель и раскрытие панелей используют те же событийные анимации (§4.6) без постоянной перерисовки.

### 4.8 Динамические формы (`FormManager`)
Формы не «зашиты» в код, а создаются динамически через `app/FormManager`:
- **Реестр открытых форм:** `create(type, id)`, `destroy(handle)`, `closeAll()`, `getOpen()`; каждая форма — экземпляр с `id`, типом (Employee/Order/Vacation/…) и состоянием.
- **Сохранение:** `saveAll()` / `save(form)` — агрегирует изменения всех открытых форм в очередь команд (debounce, транзакция, см. §8); индикатор «сохранено» в status bar.
- **Конфликты/подтверждения:** при закрытии/`closeAll()` формы с несохранёнными изменениями — подтверждение (§4.4); групповое сохранение перед выходом.
- **Единый шаблон:** все формы строятся по эталонной схеме (§4.7) — общая рамка, разделитель, вкладки, статус-панель; отличия только в полях и валидации.
- **Ленивые ресурсы:** форма подгружает данные при открытии и освобождает при закрытии; в idle открытые формы не перерисовываются (§9).

---

## 5. Шрифты и темы

### 5.1 Шрифты (загрузка при старте)
1. Поиск в `assets/fonts/` → `/usr/share/fonts/TTF/` → `fc-match "Noto Sans"` (Cyrillic).
2. `ImFontConfig` + `GetFontRanges` (Basic + Cyrillic `0x0400–0x04FF` + `0x0462`, псевдографика/стрелки, символы табеля).
3. Два размера: UI 15px, таблицы 14px, заголовки 16px; `FontGlobalScale` под HiDPI (`io.DisplayFramebufferScale`).
4. `FontAwesome6-Free.ttf` как иконки (дополнительный шрифт, `PushFont` в toolbar).
5. **Монохромный эмодзи-шрифт** (`NotoEmoji-Regular.ttf`, ЧБ-глифы) — второй иконочный шрифт, загружается при старте вместе с UI-шрифтами; используется в меню, на кнопках, в статус-бейджах и пунктах контекстного меню (`PushFont`/`PopFont` вокруг элемента). Глифы подбираются по диапазону `0x1F300–0x1F6FF` + `0x2600–0x26FF` (стрелки, галочки, предупреждения). Иконки задаются через `u8`-литералы (`"\U0001F50D"` поиск, `"\U0001F512"` блокировка и т.п.), при отсутствии глифа — fallback на FontAwesome или текстовую подпись.
6. Кэш атласа: `SaveFontToMemory`/`LoadFontFromMemory` (или `io.Fonts` build один раз), fallback-шрифт при отсутствии кириллицы.

```cpp
ImFont* loadUiFont(const char* path, float size) {
    static const ImWchar* ranges = io.Fonts->GetGlyphRangesCyrillic();
    ImFontConfig cfg; cfg.SizePixels = size; cfg.oversample_h = 2;
    cfg.PixelSnapH = true; cfg.OversampleV = 1;
    return io.Fonts->AddFontFromFileTTF(path, size, &cfg, ranges);
}
```

```cpp
// иконочный шрифт: UI-шрифт + монохромный эмодзи (одна сборка атласа)
ImFont* iconFont = io.Fonts->AddFontFromFileTTF("assets/fonts/NotoEmoji-Regular.ttf", 15.0f, &cfg, ranges);
// в UI: io.Fonts->PushFont(iconFont); ... ImGui::Text("\U0001F50D Поиск"); io.Fonts->PopFont();
```

### 5.2 Темы
`Theme` = структура токенов (bg, bg_alt, panel, accent, text, text_dim, ok, warn, err, sel, grid) → `ApplyStyle(ImGuiStyle&)`.  
Темы: `Dark` (по умолчанию), `Light`, `HighContrast`, пользовательская из `settings.json`. Сохранение/загрузка в `assets/themes/*.json`. Плавное переключение без пересоздания шрифтов.

---

## 6. Экспорт в ODT / ODS (LibreOffice)

Два режима:

**A. Шаблонный ODT (приказы, T-7).**  
1. Шаблон `assets/templates/order_hire.odt` (редактируется в LibreOffice), плейсхолдеры `{{employee.fio}}`, `{{order.number}}`, `{{order.body}}`.  
2. `unzip` шаблона (libzip/minizip) → правка `content.xml` (XML-безопасная подстановка, перенос строк через `<text:line-break/>`) → `zip` обратно.  
3. Опционально `soffice --headless --convert-to pdf` для печати.

**B. Flat-XML → ODS/ODT (табели, отчёты).**  
Генерируем `.fods`/`.fodt` (один XML-файл, без zip) и конвертируем:
```bash
soffice --headless --norestore --convert-to ods --outdir out report.fods
```
- Таблица: `office:spreadsheet` → `table:table-cell office:value-type="string|float"`, стили заголовков, ширина колонок, `table:number-rows-repeated` для больших диапазонов.
- Проверка `soffice` при старте (`which soffice`), fallback — прямая запись `.fods` и CSV.

Шаблоны в репозитории + возможность пользователю указать свою папку шаблонов; предпросмотр приказа в UI (упрощённый рендер текста + `{placeholder}` подсветка ошибок).

---

## 7. Валидация и блокировки

Движок правил (`Validator::validate(entity) → vector<Issue{severity, field, message}>`):
- ФИО/дата рождения: непусто, возраст ≥ 16, дата рождения < даты приёма.
- Приём: `hire_date` ≥ даты подразделения; табельный номер уникален; нет пересечения `assignment` по первичной ставке.
- Перевод: `start_date` > предыдущей `end_date`, указана причина, старый период закрыт.
- Увольнение: `dismiss_date` ≥ `hire_date`, обязательна причина/основание, закрытие всех активных `assignment`, отпускной баланс списан.
- Отпуск: дни ≤ остаток, нет пересечений с больничными/другим отпуском, даты в пределах года графика.
- Больничный: `end_date ≥ start_date`, пересечение с отпуском → предупреждение (продление отпуска), дни = датам.
- Табель: код из справочника, сумма часов = норма месяца (предупреждение), нет записей задним числом по утверждённому периоду.
- Приказ: уникальность `kind+date+number`, утверждённый приказ нельзя редактировать, нельзя утвердить документ с ошибками.
- Целостность: сироты `assignment`, дубли ФИО+дата рождения, отсутствующий приказ у утверждённого документа.

Панель «Проверка базы»: полный аудит → список проблем с переходом к записи (клик = выделить строку).  
`audit_log` — diff JSON на каждое изменение; просмотр из `AuditView`.

---

## 8. База, бэкап, автосохранение

- **Открытие/создание:** стартовый диалог (последние базы, «новая база», выбор пути `.db`); файловые операции (выбор БД, шаблонов, папки бэкапов, экспорт/импорт) — через **ImGuiFileDialog** (`github.com/aiekick/ImGuiFileDialog`, header-only, ImGui-native, без внешних зависимостей) вместо системных диалогов.
- **Сохранение/закрытие:** «Сохранить как…» (`.save` → `sqlite3_backup`), checkpoint WAL, `PRAGMA integrity_check` при закрытии.
- **Бэкап-сервис:**  
  - перед закрытием/при каждом 20-м изменении — автоснимок `backups/hr-YYYYMMDD-HHMMSS.db` (`sqlite3_backup_init`);  
  - ротация: хранить N (по умолчанию 30) + ежедневный 7 + ежемесячный 12;  
  - ручная кнопка «Создать резервную копию», «Восстановить из копии…» (только при закрытой сессии, с подтверждением);  
  - `VACUUM INTO` для компактной копии; `ANALYZE` раз в сессию; проверка `PRAGMA quick_check` после восстановления;  
  - опция шифрования копии (XOR нет — использовать `sqlcipher` опционально или архив `zstd` + чек-сумма SHA-256).
- **Автосохранение UI-изменений:** debounce 800 мс, фоновый worker + очередь команд, `BEGIN IMMEDIATE` транзакция, откат при ошибке + Toast, индикатор в status bar. Никаких «грязных» изменений при закрытии окна без подтверждения.
- **Схема (без миграций) — временная мера начальной разработки:** на этапе активной разработки схема создаётся заново из `Schema/0001_init.sql` при каждой новой БД; миграции между версиями **не применяются** (слишком много правок схемы). После стабилизации схемы (релиз 1.0) **вернёмся к миграциям**: последовательные скрипты `Migrations/NNNN_*.sql` + `schema_version`, обязательный бэкап перед миграцией.
- **Тестовая/демо-база:** отдельный модуль `seed/` (`SeedDb`, `LoadGen`) детерминированно генерирует базу по seed: справочники, сотрудники, документы, приказы, отпуска, больничные, табель. Используется для функциональных тестов (детерминированный результат) и нагрузочных (N сотрудников/документов, > 2000 строк) и для ручного демо-прогона.

---

## 9. Производительность (CPU)

- Цикл: `glfwWaitEventsTimeout(0.05)` вместо бесконечного `PollEvents`; рендер только при `io.WantCaptureMouse/изменениях/анимациях` (idle-режим 5 FPS, активный 30 FPS, `ConfigMaxFrameRate` при наличии).
- Анимации (§4.6) — событийные и короткоживущие: завершаются → `WantRedraw` снимается → цикл возвращается в idle; `ReducedMotion`/«Эконом-режим» отключает их для экономии батареи.
- `ImGui::TableFlags` + `ListClipper`; строки формируются из кэшированных `std::string_view` (без `std::format` на каждый кадр).
- Кэш запросов: prepared statements (LRU), `stmt.reset()` вместо finalize, `PRAGMA cache_size=-20000`, `mmap_size=64MB`.
- Фоновые потоки: тяжёлые отчёты/экспорт/проверка базы вне UI-потока, прогресс через `EventBus` (UI не блокируется).
- Ленивые данные: карточка сотрудника подгружает историю только при открытии; табель — помесячно.
- Избегать: пересборка колонок/фильтров каждый кадр, лишние `std::string` в `TableSetupColumn`, пересоздание шрифтов при смене темы.
- Метрики в status bar: FPS, ms/frame, CPU% (через `/proc/self/stat`), размер БД — для контроля.

---

## 10. CMake и зависимости (статическая сборка)

**Принцип:** все зависимости **клонируются и компилируются статически** (vendored в `third_party/` через git-submodules или FetchContent с зафиксированным `GIT_TAG`), чтобы сборка и запуск **не зависели от системного окружения** (отсутствующих пакетов, версий, динамических `.so`). Финальный бинарник — статически связанный.

```cmake
cmake_minimum_required(VERSION 3.24)
project(hrdesk CXX)
set(CMAKE_CXX_STANDARD 20)
set(HRDESK_VERSION "1.0.0")
add_definitions(-DHRDESK_VERSION="${HRDESK_VERSION}" -DBUILD_DATE="${CMAKE_BUILD_TYPE}")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -march=native -DNDEBUG")

find_package(OpenGL REQUIRED)          # единственный динамический системный компонент

include(FetchContent)
# --- vendored, статически компилируемые зависимости (фиксированные версии) ---
FetchContent_Declare(imgui   GIT_REPOSITORY https://github.com/ocornut/imgui   GIT_TAG docking GIT_SHALLOW TRUE)
FetchContent_Declare(imgui_file_dialog GIT_REPOSITORY https://github.com/aiekick/ImGuiFileDialog GIT_TAG master GIT_SHALLOW TRUE)
FetchContent_Declare(glfw    GIT_REPOSITORY https://github.com/glfw/glfw       GIT_TAG 3.4    GIT_SHALLOW TRUE)
FetchContent_Declare(sqlite  GIT_REPOSITORY https://github.com/sqlite/sqlite   GIT_TAG v3.45.0 GIT_SHALLOW TRUE)
FetchContent_Declare(zlib    GIT_REPOSITORY https://github.com/madler/zlib     GIT_TAG v1.3.1 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(imgui imgui_file_dialog glfw sqlite zlib)

add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL sqlite minizip)
add_subdirectory(src)
enable_testing(); add_subdirectory(tests)
```

- **Статическая линковка:** `imgui`, `glfw`, `sqlite3` (amalgamation `sqlite3.c`), `minizip` (из `zlib/contrib`) собираются как статические `.a`-библиотеки из `third_party` и линкуются в бинарник — системные `.so` этих пакетов не требуются.
- **Что остаётся динамическим:** только OpenGL/GLX (драйвер графики статически связать нельзя) и, опционально, `libreoffice` (вызывается как внешний процесс для экспорта). `fontconfig` — опционально, шрифты лежат в `assets/fonts/`.
- **Фиксация версий:** pinned-теги/хэши в `third_party/` (или `versions.lock`) → воспроизводимая сборка на любой машине.
- **Сборка:** `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j && ctest --test-dir build`.

---

## 11. Этапы

| # | Этап | Результат | Оценка |
|---|---|---|---|
| 0 | **Hello World:** CMake, окно ImGui, загрузка шрифтов (кириллица + иконочные FA6/монохромный эмодзи), проверка запуска и рендера текста/иконок | запускаемое приложение с корректным русским текстом и иконками | 1 день |
| 1 | **Меню и каркас UI:** toolbar, nav, dockspace, тема, status bar, `FormManager` (динамические формы: создать/уничтожить/закрыть все/сохранить все) | навигация + менеджер форм | 1–2 дня |
| 2 | **Схема БД (DDL) + консольная утилита** генерации тестовой базы (`seed/`, `--employees N --docs M`) | рабочая схема + демо/нагрузочная база из CLI | 2–3 дня |
| 3 | **Универсальная первая (эталонная) форма** — самая сложная (master–detail §4.7): таблица + перемещаемый разделитель + редактор + панели; доводим до приемлемого качества | эталон формы (карточка сотрудника) | 3–4 дня |
| 4 | **Остальные формы по аналогии** с эталоном (приём/перевод/увольнение, документы, приказы, отпуска, больничные, табель) | типовые формы на базе эталона | 4–6 дней |
| 5 | Validator + AuditView + панель «Проверка базы» | отчёт о проблемах, журнал изменений | 2–3 дня |
| 6 | Экспорт ODT/ODS: шаблоны, TemplateEngine, LibreOfficeBridge, PDF | приказы и отчёты «в бумаге» | 3–4 дня |
| 7 | Отчёты (7+ видов) + групповые операции + пакетный экспорт | аналитика и массовые операции | 3 дня |
| 8 | BackupService: автоснимки, ротация, восстановление, пересоздание схемы | надёжность данных | 2–3 дня |
| 9 | Оптимизация CPU: событийный рендер, клиппинг, кэши, метрики | ≤ 3–5% CPU в idle | 2 дня |
| 10 | Тесты, полировка UI: иконки (эмодзи), комбобоксы с поиском, multiline-поля, status bar, ConfirmDialog, About (версия/дата), горячие клавиши, документация | релиз 1.0 | 2–3 дня |

**Порядок разработки (не распыляемся):** сначала доводим **одну эталонную форму** (этап 3) до приемлемого качества, и только затем копируем подход на остальные формы (этап 4) — без параллельной правки всего интерфейса.

**MVP = этапы 0–4 + 6** (справочники, сотрудники, приказы с блокировкой, экспорт ODT).

---

## 12. Критерии приёмки

- Русский текст корректен во всех виджетах (включая ввод с русской раскладкой, сортировка по ФИО).
- Утверждённый приказ невозможно изменить без явной разблокировки с записью в аудит.
- Группа из 100 сотрудников → приказы/табель/отчёт формируются за один проход, UI не зависает (отклик в пределах активного FPS-капа, §9).
- Экспорт: `order_0001.odt` открывается в LibreOffice, значения подставлены, табель `.ods` содержит формулы/стили.
- После `kill -9` база целостна (WAL), автосохранённые изменения не потеряны.
- Idle-режим (окно открыто, без действий): CPU < 5%, FPS < 10.
- Анимации не «жгут» батарею: после завершения анимации FPS возвращается к idle; в «Эконом-режиме» анимации отключены и CPU/FPS не выше idle-значений.
- Сортировка/фильтр/удаление не ломают групповые операции: выделение и CRUD привязаны к `id`, а не к позиции строки (тест: сортировка → массовое утверждение затрагивает ровно выбранные `id`).
- Критичная/групповая операция требует подтверждения; отмена не изменяет БД (тест: ConfirmDialog → Cancel → `audit_log` пуст).
- `ctest` зелёный: репозитории, валидатор, state-machine, ODT/ODS writer (сравнение XML), backup/restore.

---

## 13. Риски и решения

| Риск | Решение |
|---|---|
| ImGui и кириллица/раскладка | явные glyph ranges, `io.ConfigInputTextCursorBlink`, проверка `AddFontFromFileTTF` с fallback |
| Обрыв UTF-8 в multiline/переносе | перенос по codepoint (не по байтам), `u8`-обёртки, тесты на многобайтовых глифах |
| Рассинхрон UI↔БД при сортировке/фильтрах | все ссылки и групповые операции — по `id` (PK), позиция в списке не используется как ключ |
| Сложность ODT-шаблонов | FODT/FODS + LibreOffice как конвертер; шаблоны правит пользователь |
| Долгие отчёты блокируют UI | worker-очередь + прогресс-бар + отмена |
| Конфликт правок (одна БД, несколько процессов) | `PRAGMA busy_timeout=5000`, advisory lock-файл, режим «только чтение» |
| Разрастание табеля | месячные таблицы-партиции по `day` + индексы, архив старых периодов |
| Нет `soffice` на машине | fallback: `.fods`/CSV/прямой ODT-writer (минимальный стиль) |

---

## 14. О программе (About)

- Диалог «О программе» (меню/иконка): версия, дата/время компиляции, хеш сборки (git SHA), путь к БД, версия схемы, список загруженных шрифтов, зависимости.
- Версия и дата фиксируются при сборке через CMake (`project(... VERSION x.y.z)`, `add_definitions`/генерируемый `version.h` с `__DATE__`/`__TIME__` или `git rev-parse --short HEAD`); отображаются в About и в status bar.
- Файл `CHANGELOG.md` + нумерация релизов; версия схемы (`schema_version`) фиксируется при создании БД из `Schema/0001_init.sql` (миграций нет — схема пересоздаётся).

---

## 15. Генератор тестовой базы (`seed/`)

Отдельный модуль для функциональной и нагрузочной проверки:

- **`SeedDb`** — детерминированная генерация (по `seed`) полной демо-базы: подразделения, должности, штатное расписание, сотрудники (ФИО/дата рождения/паспорт/СНИЛс), кадровые документы и приказы (все статусы), отпуска (T-7), больничные, табель за период. Один и тот же seed → идентичная база (воспроизводимость тестов).
- **`LoadGen`** — параметрическая нагрузка: `--employees N --docs M --months K` (например 5000 сотрудников, 20000 документов, табель за 12 месяцев) для проверки пагинации, клиппинга, сортировки, групповых операций, времени отчётов/экспорта и CPU в idle.
- **CLI-режим:** `hrdesk --seed demo.db --employees 5000 --docs 20000` создаёт базу и открывает её; `hrdesk --seed --check` прогоняет функциональные сценарии (приём→приказ→утверждение→блокировка→отчёт→экспорт) и печатает результат.
- **Тесты:** unit/smoke используют `SeedDb` как фикстуру (in-memory `:memory:` или временный файл); нагрузочные — сравнение с целевыми метриками (FPS, ms/frame, время отчёта).
- **Схема пересоздаётся** каждый раз из `Schema/0001_init.sql` (миграций нет на начальной разработке — временная мера, см. §8), поэтому генератор всегда работает против актуальной схемы; после релиза 1.0 генератор будет применять миграции.

---

## 16. Быстрый старт

Зависимости (`imgui`, `ImGuiFileDialog`, `glfw`, `sqlite`, `minizip`) **клонируются и компилируются статически самим CMake** (FetchContent, §10) — системные пакеты этих библиотек не нужны.

```bash
# нужны только базовые инструменты: cmake, компилятор, git, OpenGL-драйвер
sudo dnf install -y cmake gcc-c++ git make   # или apt equivalents
cmake -B build -DCMAKE_BUILD_TYPE=Release     # FetchContent скачает и соберёт зависимости
cmake --build build -j"$(nproc)"
./build/src/hrdesk            # стартовый диалог: новая база / открыть / восстановить
./build/src/hrdesk --seed demo.db --employees 5000 --docs 20000   # сгенерировать демо/нагрузочную базу
ctest --test-dir build --output-on-failure
```
