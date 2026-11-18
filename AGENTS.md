# Project: peekpass

peekpass is a personal, cross-platform password-manager quick-access app inspired by 1Password Quick Access.

## Product Direction

- Use Qt 6 Widgets, not QML.
- Use C++17 and CMake.
- Keep password-manager access behind installed command-line clients.
- Do not implement provider encryption or service APIs directly.
- Target Windows first, with macOS and Linux portability later.
- Keep the implementation small, security-conscious, and MVP-oriented.

## Supported Backends

- Bitwarden, Bitwarden EU, and Vaultwarden through the Bitwarden CLI (`bw`).
- Proton Pass through the Proton Pass CLI (`pass-cli`).

Provider-specific behavior belongs in `BwClient` or `ProtonPassClient`. UI code should use the common `PasswordManager` interface and backend-neutral types in `VaultTypes.h` whenever practical.

## Current Features

- Global quick-access hotkey on Windows.
- Frameless, always-on-top search window.
- Provider-aware login and session handling.
- In-memory vault cache and search over active items (trashed/deleted items excluded).
- Password, username, and TOTP copy actions.
- Sensitive clipboard clearing after 30 seconds.
- Standalone item details dialog (`ItemDetailDialog`) with show/hide secret toggles, live TOTP code and countdown (`TotpGenerator`), website open/copy actions, and notes copy.
- Tray menu, settings, startup registration, and English/Korean UI.

## Security Requirements

- Never store account or master passwords.
- Never pass passwords directly in process arguments.
- Keep sensitive clipboard entries out of Windows clipboard history and cloud sync.
- Clear cached vault data when locking, signing out, or exiting.
- Keep Bitwarden session persistence in Windows Credential Manager when enabled.
- Let Proton Pass CLI own its session; do not copy or persist it in peekpass.
- Invoke provider CLIs with `QProcess`; do not link, bundle, or derive code from them.

## CLI Discovery

- Prefer an explicit path from settings.
- Otherwise check beside the application executable first.
- Bitwarden then falls back to `bw` on `PATH`.
- Proton Pass additionally checks its standard Windows installation directory before falling back to `pass-cli` on `PATH`.

## Verification

- Build with a Qt 6.11-compatible Qt 6 release and a matching compiler toolchain.
- Run `ctest --test-dir <build-dir> --output-on-failure` after building.
- Keep the fake Proton Pass CLI test aligned with the official command and JSON contracts.
- Use `docs/PROTON_PASS_VALIDATION.md` for live-account smoke testing; never add real vault data or credentials to fixtures.

## Remaining Platform Work

- macOS/Linux global hotkeys.
- Multi-account support.
- Dedicated Proton Pass lock-code unlock flow.
- OS credential/session handling appropriate to non-Windows platforms.
