# peekpass

[![Build](https://github.com/j0n9m1n1/peekpass/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/j0n9m1n1/peekpass/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/j0n9m1n1/peekpass?display_name=tag)](https://github.com/j0n9m1n1/peekpass/releases/latest)
[![License](https://img.shields.io/github/license/j0n9m1n1/peekpass)](LICENSE)

## References and Acknowledgements

peekpass was designed with reference to:

- [1Password Quick Access](https://support.1password.com/quick-access/) and [1Password keyboard shortcuts](https://support.1password.com/keyboard-shortcuts/)
- [Trikzon/quickwarden](https://github.com/Trikzon/quickwarden)
- [ixnas/Quickwarden](https://github.com/ixnas/Quickwarden)

Development was assisted by OpenAI ChatGPT 5.5.

peekpass is a small Qt Widgets desktop quick-access app for password-vault
items. It is inspired by 1Password Quick Access and delegates vault access to
the selected password-manager CLI instead of implementing vault encryption or
service APIs directly.

peekpass supports two password-manager backends:

- Bitwarden-compatible vaults, including Bitwarden US, Bitwarden EU, and
  self-hosted Vaultwarden servers through the Bitwarden CLI (`bw`).
- Proton Pass through the Proton Pass CLI (`pass-cli`).

This project is still early, Windows-first software.

## Features

- Global quick-access hotkey on Windows.
- Frameless always-on-top search window.
- Selectable Bitwarden or Proton Pass backend.
- First-run login screen that adapts to the selected password manager.
- Bitwarden CLI login and unlock flow.
- Proton Pass CLI (`pass-cli`) login flow, including optional TOTP and extra
  password input.
- Startup preload so unlocked vault items are cached before the search window is opened.
- In-memory search over item names, usernames, URIs, notes, and non-sensitive
  provider fields.
- Fast keyboard actions for copying username, password, TOTP, and opening details.
- Independent, draggable item-details windows that leave quick access open.
- Provider-aware item details for Bitwarden and Proton Pass, including logins,
  secure notes, cards, identities, SSH keys, Wi-Fi, aliases, custom sections,
  clickable websites, and provider metadata where available.
- Optional web favicon loading for search results.
- System tray menu with open, lock, settings, and quit.
- Settings actions to log out of the selected password manager or reset local
  peekpass data and return to first-run state.
- Configurable hotkey, shortcut preset, quick window size, position, opacity, corner style, theme, font, language, result fields, result count, auto-sync interval, and focus-loss behavior.
- Optional Windows Credential Manager session storage for Bitwarden.
- Optional Windows startup registration without administrator rights.
- English and Korean UI with Qt Linguist translation files for adding more languages.

## Screenshots

| Login | Quick access |
| --- | --- |
| <img src="login.png" alt="Bitwarden login screen" width="420"> | <img src="search.png" alt="Quick access search window" width="420"> |

| Search results | Item details | Settings |
| --- | --- | --- |
| <img src="result.png" alt="Search results" width="320"> | <img src="detail.png" alt="Item details" width="320"> | <img src="setting.png" alt="Settings dialog" width="320"> |

## Install and Run

- Windows 10/11
- One supported password-manager CLI:
  - Bitwarden CLI (`bw`)
  - Proton Pass CLI (`pass-cli`)

1. Download the latest `peekpass-<version>-win64.zip` from the
   [latest release](https://github.com/j0n9m1n1/peekpass/releases/latest).
2. Extract the complete archive. Keep `peekpass.exe`, the Qt DLLs, and the
   deployment subdirectories together.
3. Install at least one supported password-manager CLI.
4. Run `peekpass.exe`. A tray notification confirms that the app is running.
5. If no usable session exists, choose Bitwarden or Proton Pass in the login
   window and enter the fields shown for that provider.
6. Press `Ctrl+Shift+Space` to open quick access.

The password manager can later be changed in
**Settings > General > Password manager**.
peekpass first looks for each CLI next to the application executable. Bitwarden
then falls back to `bw` on `PATH`. Proton Pass additionally checks its standard
Windows install location, `%LOCALAPPDATA%\Programs\ProtonPass\pass-cli.exe`,
before falling back to `pass-cli` on `PATH`. An explicit CLI location can also
be set in the corresponding settings tab.

- [Bitwarden CLI download and setup](https://bitwarden.com/help/cli/)
- [Proton Pass CLI installation](https://protonpass.github.io/pass-cli/get-started/installation/)

## Provider Behavior

| Behavior | Bitwarden / Vaultwarden | Proton Pass |
| --- | --- | --- |
| CLI | `bw` | `pass-cli` |
| Server selection | Bitwarden US, Bitwarden EU, or a custom URL | Managed by Proton Pass |
| Login inputs | Email, master password, optional 2FA method/code | Email, password, optional TOTP and extra password |
| Locked session | Unlocks with `bw unlock --raw` | Log in again; dedicated lock-code unlock is not implemented |
| Tray lock action | Runs `bw lock` | Runs `pass-cli logout` |
| Session ownership | Optional session key storage in Windows Credential Manager | Owned by Proton Pass CLI |
| Sync | Runs Bitwarden sync and reloads items | Reloads the current item list; Proton Pass CLI has no separate sync command |

## How It Works

```mermaid
flowchart LR
    A["Start peekpass"] --> B["Register tray and global hotkey"]
    B --> C["Check selected CLI and session"]
    C -->|"Signed out or locked"| D["Show provider-aware login or unlock UI"]
    C -->|"Ready"| E["Load vault items into memory"]
    D --> E
    E --> F["Filter cached items while the user types"]
    F --> G["Copy a field, TOTP, or open item details"]
    G --> H["Clear sensitive clipboard content after 30 seconds"]
```

Vault encryption, account storage, and remote service access remain the
responsibility of the selected CLI. peekpass starts vault preparation when the
application starts so opening quick access normally does not have to wait for
the initial item load. If authentication is required, the login or unlock UI is
shown immediately instead.

## Project Structure

- `src/main.cpp` - Application startup, tray setup, and quick window wiring.
- `src/QuickWindow.*` - Quick-access search window, login/unlock UI, result
  list, copy actions, detail view, and favicon loading.
- `src/BwClient.*` - Bitwarden CLI wrapper for status, login, unlock, sync,
  item retrieval, sensitive field lookup, and session handling.
- `src/ProtonPassClient.*` - Proton Pass CLI wrapper for status, login, vault
  listing, item retrieval, and TOTP lookup.
- `src/PasswordManager.*` - Selects the active backend and presents the common
  password-manager contract to the UI.
- `src/VaultTypes.h` - Backend-neutral vault item and detail data structures.
- `src/AppSettings.*` - Persistent settings for hotkeys, UI options, language,
  sync interval, result display, and session storage preferences.
- `src/SettingsDialog.*` - Settings dialog UI and validation.
- `src/WinHotkey.*` - Windows global hotkey registration.
- `src/I18n.*` and `src/TranslationCatalog.cpp` - Runtime text lookup and
  built-in translations.
- `translations/` - Qt translation source files and translation notes.

## Build

Building from source requires CMake 3.21+, a C++17 compiler, and Qt 6.11 or a
compatible Qt 6 release with Widgets, Concurrent, Network, LinguistTools, and a
matching compiler toolchain. Install the selected password-manager CLI before
running the application.

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:\Qt\6.11.1\mingw_64"
cmake --build build --parallel
```

If Qt is already discoverable by CMake, the `CMAKE_PREFIX_PATH` argument is not
needed. Multi-config generators can omit `CMAKE_BUILD_TYPE` and use
`cmake --build build --config Release` instead.

## GitHub Release Build

GitHub Actions builds a Windows release package when a version tag is pushed:

```powershell
git tag -a vX.Y.Z -m "peekpass vX.Y.Z"
git push origin main
git push origin vX.Y.Z
```

peekpass follows semantic versioning (`MAJOR.MINOR.PATCH`). Keep the version in
`project(peekpass VERSION ...)` in `CMakeLists.txt` aligned with the Git tag.

The workflow creates a `peekpass-<tag>-win64.zip` package with `peekpass.exe`,
Qt runtime DLLs from `windeployqt`, translations, `README.md`, and `LICENSE`, then
attaches it to the GitHub Release. It can also be run manually from the Actions
tab. To rebuild an existing tag after fixing the release workflow, run the
workflow from `main` and enter that tag in the `release_tag` input. Leave
`release_tag` empty for a build-only manual run. Use a new version tag for each
source release.

The latest published package is available on the
[Releases page](https://github.com/j0n9m1n1/peekpass/releases).

## Usage

Default quick-access hotkey:

```text
Ctrl+Shift+Space
```

1Password-style shortcut preset:

```text
Ctrl+C          Copy username or primary field
Ctrl+Shift+C    Copy password
Ctrl+Alt+C      Copy TOTP
Ctrl+Shift+O    Open item details
Ctrl+R          Sync vault
Ctrl+/          Open settings
Esc             Close
```

Bitwarden-style shortcut preset:

```text
Ctrl+Enter      Copy username
Enter           Copy password
Alt+T           Copy TOTP
Ctrl+Shift+O    Open item details
Ctrl+R          Sync vault
Ctrl+/          Open settings
Esc             Close
```

The tray menu provides quick access, lock/sign-out, settings, and quit actions.
To return to a completely fresh first-run state, use
**Settings > Behavior > Reset peekpass**. This signs out installed provider
CLIs, removes local settings and the stored Bitwarden session, unregisters
Windows startup, and closes the app.

## Security Notes

- Password-manager account passwords are never stored by peekpass.
- peekpass uses `bw login` and `bw unlock --raw` to obtain a Bitwarden CLI
  session.
- Vault item fields needed for quick search and copy are cached in memory while
  the app is running.
- In-memory vault data is cleared when the vault is locked or the app exits.
- Passwords and TOTP codes copied to the clipboard are cleared after 30 seconds if
  peekpass still owns the same clipboard content.
- Sensitive clipboard entries opt out of Windows clipboard history and cloud sync.
- TOTP secrets and passwords are not written to peekpass settings.
- Optional Windows Credential Manager storage persists the Bitwarden CLI session
  key, not the master password.
- Logging out runs `bw logout`, clears in-memory vault data, and removes the
  stored peekpass session.
- Proton Pass login delegates to `pass-cli login --interactive`. Proton Pass CLI
  owns its own local session; peekpass does not copy that session into Windows
  Credential Manager.
- Proton login credentials are passed through owner-only, short-lived temporary
  credential files supported by `pass-cli`, rather than direct-value environment
  variables. Inherited Proton credential variables are removed from the child
  process before those files are supplied.
- Item details use provider-specific field sections so Bitwarden and Proton Pass
  item schemas are not forced into the same display model.
- Until a dedicated Proton Pass CLI lock-code UI exists, the tray action for
  Proton Pass safely runs `pass-cli logout` instead of `session lock`.
- Web favicon loading is optional. When enabled, peekpass requests
  `/favicon.ico` directly from websites stored in matching vault items, which
  exposes those hostnames to the corresponding web servers.

## Localization

English and Korean are currently supported. User-visible text is routed through
the Qt translation layer, and the selected language can also apply a suitable
default quick-access font and size.

To contribute another language, add `translations/peekpass_<locale>.ts`, list it
in `PEEKPASS_TRANSLATIONS` in `CMakeLists.txt`, and build it with Qt Linguist.
See [translations/README.md](translations/README.md) for the exact steps.

## Current Limitations

- Windows is the primary target.
- macOS/Linux global hotkey support is not implemented yet.
- Multi-account support is not implemented yet.
- The app depends on the selected Bitwarden or Proton Pass CLI being installed.
- Vault items are cached in memory for fast searching.
- YubiKey, passkey, and other hardware security key authentication flows have
  not been tested yet.
- Proton Pass CLI compatibility has not yet been tested against a live user
  vault in this project.

## Status

This is a personal prototype and should be treated as preview software.

## License

peekpass application source code is licensed under the MIT License. See
[LICENSE](LICENSE).

peekpass currently links against Qt Widgets, Qt Concurrent, and Qt Network.
Qt is not relicensed by this project. When distributing Windows builds made with
open-source Qt, peekpass should use Qt as dynamically linked DLLs and include
the notices, license texts, and source-code access or written offer required by
the applicable Qt/LGPL terms. Users must be able to replace the Qt DLLs with
compatible versions.

This project does not currently use Qt modules that Qt documents as GPL-only.
If that changes, or if Qt is statically linked, the distribution requirements may
change significantly.

Proton Pass CLI is a separate GPL-3.0-or-later program. peekpass invokes a
user-installed `pass-cli` executable and does not copy, link, bundle, or derive
code from its source. peekpass itself remains MIT-licensed.

This is not legal advice.
