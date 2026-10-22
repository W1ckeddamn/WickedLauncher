<p align="center">
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="/program_info/org.prismlauncher.PrismLauncher.logo-darkmode.svg">
  <source media="(prefers-color-scheme: light)" srcset="/program_info/org.prismlauncher.PrismLauncher.logo.svg">
  <img alt="Wicked Launcher" src="/program_info/org.prismlauncher.PrismLauncher.logo.svg" width="30%">
</picture>
</p>

<h1 align="center">Wicked Launcher</h1>

<p align="center">
  <b>Wicked Launcher</b> — это форк Prism Launcher с расширенными возможностями: поддержка кастомных офлайн-аккаунтов с локальным скин-сервером, интеграция Schematic, CPM/YSM моделей, подробное логирование сбоев с минидампами и оптимизированные легковесные сборки.
</p>

<p align="center">
  <a href="https://github.com/W1ckeddamn/WickedLauncher/actions"><img src="https://img.shields.io/github/actions/workflow/status/W1ckeddamn/WickedLauncher/build.yml?branch=develop&label=CI%20Build&logo=github" alt="CI Status"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-GPL%20v3-blue.svg?logo=gnu" alt="GPL v3"></a>
  <a href="https://github.com/W1ckeddamn/WickedLauncher/releases"><img src="https://img.shields.io/github/v/release/W1ckeddamn/WickedLauncher?label=Release&logo=github" alt="Latest Release"></a>
</p>

---

## ✨ Ключевые особенности и отличия

### 🎭 Расширенные офлайн-аккаунты (Enhanced Offline)
* **Кастомные UUID, скины и плащи:** возможность задать любой никнейм, произвольный UUID (или автоматически сгенерировать v4 UUID), модель игрока (`classic` / `slim`), а также локальный PNG-файл скина и плаща.
* **Встроенный локальный сервер скинов (`LocalSkinServer`):** собственный легковесный HTTP-сервер на `127.0.0.1`, реализующий протокол authlib-injector. При запуске игры ваши скины и плащи автоматически внедряются в клиент без использования внешних онлайн-сервисов!

### 🏗️ Управление схематиками (Schematics)
* Вкладка **Schematics** в настройках инстанса (папка `schematics`).
* Поддержка всех популярных форматов:
  * **`.schematic`** (MCEdit / WorldEdit legacy)
  * **`.schem`** (Sponge / WorldEdit modern)
  * **`.litematic`** (Litematica)
* Автоматический парсинг метаданных: размеры (ширина × высота × длина), автор, версия формата, палитра блоков и имя постройки.

### 🕺 Модели персонажей: CPM & YSM
* **Custom Player Models (CPM):**
  * Вкладка в инстансе (папка `cpmmodels`).
  * Поддержка файлов `.cpmmodel`, чтение имени модели, автора и версии.
* **Yes Steve Model (YSM):**
  * Вкладка в инстансе (папка `yesstevemodel`).
  * Поддержка `.ysm`, `.zip`, `.7z` с чтением метаданных и автора.

### 🛡️ Продвинутое логирование и Crash Handler
* **Windows:** перехват необработанных исключений (`SetUnhandledExceptionFilter`), раскрутка стека (`StackWalk64` + `dbghelp`), автоматическое сохранение подробного стектрейса и минидампа `.dmp` (`MiniDumpWriteDump`) в файл `crash.log`.
* **Linux / macOS:** обработчики `SIGSEGV`, `SIGABRT`, `SIGBUS`, `SIGFPE` с сохранением вызовов `backtrace` в `crash.log`.
* Потокобезопасный форматированный логгер сообщений с защитой от рекурсивных вызовов.

### ⚡ Оптимизированные и компактные сборки
* Все portable-пакеты и AppImage компилируются в режиме **Release** с оптимизациями и LTO.
* Стриппинг отладочных символов (`strip --strip-unneeded`) для Linux и Windows (MinGW), исключение промежуточных файлов PDB/ILK/LIB для MSVC.
* Размер архивов сокращен на **60–75%** по сравнению со стандартными отладочными сборками.

---

## 🚀 Установка и загрузка

Готовые сборки для всех платформ доступны в разделе **[Releases](https://github.com/W1ckeddamn/WickedLauncher/releases)** и во вкладке **[GitHub Actions](https://github.com/W1ckeddamn/WickedLauncher/actions)**:

| Платформа | Форматы |
| :--- | :--- |
| **Windows (x64 / arm64)** | Setup Installer (`.exe`), Portable (`.zip`) |
| **Linux (x86_64 / aarch64)** | AppImage (`.AppImage`), Portable Tarball (`.tar.gz`) |
| **macOS (Universal)** | Disk Image (`.dmg`), Archive (`.zip`) |

---

## 🛠️ Сборка из исходников

### Требования
* Компилятор C++20/C++23 (GCC 13+, Clang 16+, MSVC 2022 17.8+)
* **CMake** 3.22 или выше
* **Ninja**
* **Qt** 6.6+ (Core, Gui, Widgets, Network, Concurrent, Svg, Xml, DBus на Linux)
* **Java** Development Kit (JDK 8 или новее) для сборки вспомогательных jar-файлов

### Инструкция по сборке

```bash
# Клонирование репозитория вместе с подмодулями
git clone --recursive https://github.com/W1ckeddamn/WickedLauncher.git
cd WickedLauncher

# Настройка с помощью CMake presets (пример для Linux/GCC)
cmake --preset gcc-release -DLauncher_BUILD_PLATFORM="custom"

# Компиляция
cmake --build --preset gcc-release
```

На **Windows (MSVC)**:
```pwsh
cmake --preset msvc-release
cmake --build --preset msvc-release
```

---

## 📜 Лицензия и благодарности

* Wicked Launcher распространяется под лицензией **GPL-3.0-only**. Подробнее см. в файле [LICENSE](LICENSE).
* Логотип и графические ассеты распространяются под лицензией **CC BY-SA 4.0**.
* Данный проект является независимым форком **[Prism Launcher](https://github.com/PrismLauncher/PrismLauncher)** и **[MultiMC](https://github.com/MultiMC/Launcher)**.
* Отдельная благодарность разработчикам **[Luna Launcher](https://github.com/AndreaFrederica/LunaLauncher)** и сообществу за идеи и наработки.
