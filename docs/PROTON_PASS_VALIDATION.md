# Proton Pass live validation

The automated `proton_pass_client` test uses a fake CLI with the official Proton Pass command and JSON shapes. Run this checklist against an installed `pass-cli` before calling a release live-vault verified.

Never add screenshots, logs, fixture files, or command output containing real credentials or vault data to the repository.

## Prerequisites

- Install the latest stable Proton Pass CLI and confirm `pass-cli --version` works.
- Sign in with a test account or an account containing non-production test items.
- Keep at least one login item with a username, password, URL, and TOTP field.
- If possible, include a secure note, card, identity, SSH key, Wi-Fi item, alias, and custom item.

## Checklist

- [ ] Start peekpass with no active Proton Pass session and select Proton Pass.
- [ ] Confirm username/password login succeeds.
- [ ] Confirm TOTP and extra-password prompts work when the account requires them.
- [ ] For SSO or a hardware security key, complete `pass-cli login` in a terminal and reopen peekpass.
- [ ] Restart peekpass and confirm the CLI-owned session is detected without another login.
- [ ] Confirm every accessible vault contributes items to search results.
- [ ] Search by title, username/email, URL, notes, and a non-sensitive custom field.
- [ ] Copy username, password, and TOTP using both shortcut presets.
- [ ] Confirm sensitive clipboard content is cleared after 30 seconds and excluded from clipboard history.
- [ ] Open details for each available item type and verify sensitive fields remain concealed until copied.
- [ ] Run manual sync and confirm the current vault contents are reloaded.
- [ ] Use the tray lock action and confirm it signs out of Proton Pass CLI and clears cached items.
- [ ] Switch to Bitwarden and back to Proton Pass; confirm provider state and search results do not leak across backends.
- [ ] Reset peekpass and confirm local settings are cleared and installed provider CLIs are signed out.

Record only the tested `pass-cli` version, peekpass commit, Windows version, and pass/fail result. Do not record item names, account addresses, or secrets.
