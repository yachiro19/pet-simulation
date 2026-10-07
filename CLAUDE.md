# CLAUDE.md

Учебная симуляция ПЭТ-сканера на Geant4 11.3 (C++17, CMake) плюс анализ на Python. Это дипломный проект. Подробности в README.md.

## Правило коммитов (обязательно)

История коммитов — часть диплома. Каждое изменение кода, макросов или анализа коммитится вместе с описанием:

1. Новый файл `docs/changelog/NNN-краткое-название.md` (следующий номер, шаблон в `docs/changelog/README.md`) с разделами «Что сделано», «Зачем», «Как», «Проверка и результат». Всё на русском.
2. Добавить строку в таблицу «Записи» в `docs/changelog/README.md`.
3. Сообщение коммита на русском: заголовок `NNN: краткое описание`, пустая строка, тело в 2–5 строк (что и зачем).
4. Одно логическое изменение = один коммит = один файл описания. Историю не переписывать (без amend, rebase, force-push).

Мелкие правки без смысловой нагрузки (опечатки в документации) можно коммитить без отдельного файла, но с понятным сообщением.

## Окружение (машина автора, Windows 11)

- Geant4 11.3.0: `C:/Geant4/geant4-v11.3.0-inst`, папка `bin` в PATH. `CMakeLists.txt` использует этот путь по умолчанию на Windows.
- CMake не в PATH: `C:\Program Files\CMake\bin\cmake.exe`.
- Visual Studio 2022 Community, генератор `"Visual Studio 17 2022"`, multi-config. Исполняемый файл лежит в `build\Release\`.
- Python 3.12 с pandas, numpy, matplotlib.
- Smart App Control отключён (2026-10-07). Если exe снова завершается без вывода с кодом `0xC0E90002`, значит какая-то политика Application Control блокирует неподписанные DLL Geant4. Не обходить это самостоятельно, сообщить пользователю.
- `test.mac` (100 событий) идёт около 25 с. В CSV попадает лишь ~1–2 % событий: кольцо из 24 кристаллов высотой 10 мм покрывает малую долю телесного угла при изотропном источнике.

## Команды

```powershell
$cm = "C:\Program Files\CMake\bin\cmake.exe"
& $cm -S . -B build -G "Visual Studio 17 2022" -A x64   # конфигурация
& $cm --build build --config Release                     # сборка; макросы копируются в build\Release

# запуск: только из build\Release (макросы и pet_data.csv берутся из текущей папки)
cd build\Release
.\PET_simulation.exe test.mac     # быстрая проверка, 100 событий
.\PET_simulation.exe batch.mac    # 50 000 событий
.\PET_simulation.exe              # интерактив + OpenGL, выполняет run.mac

# анализ: из папки с pet_data.csv
$env:MPLBACKEND="Agg"; python ..\..\analysis\analyse.py   # Agg, чтобы plt.show() не блокировал
```

## Устройство

- `main.cc`: без аргументов запускается интерактив (`/control/execute run.mac`), с одним аргументом выполняется указанный макрос в batch-режиме. Используется последовательный `G4RunManager`.
- `DetectorConstruction`: 24 кристалла LYSO по кольцу R=150 мм, на каждом SiPM, водный куб в центре. Copy number кристалла = `CrystalID`.
- `SteppingAction`: энерговыделение не-оптических частиц в объёмах `Crystal*` идёт в `EventAction::AddHit`. Оптические фотоны, входящие в `SiPM*`, считаются и убиваются.
- `EventAction`: пишет строку в `pet_data.csv` (static ofstream в текущей папке).
- Формат CSV и `analysis/analyse.py` связаны: колонки `NumGammaHits` и `PhotonCount`, списки через `;`. При изменении колонок в `EventAction.cc` нужно обновить скрипт. В скрипте зашито `N_crystals = 24`.
- Новые `.mac` класть в `macros/`: CMake копирует их рядом с exe (POST_BUILD).
