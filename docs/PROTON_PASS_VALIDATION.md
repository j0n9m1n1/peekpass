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
- [ ] Click Browser sign-in and confirm the CLI window opens with the current `pass-cli login` command.
- [ ] Follow the browser link and complete authentication yourself, including SSO, TOTP, or a hardware security key if required.
- [ ] Enter any Pass extra password directly in the CLI, then click Check connection in peekpass.
- [ ] Confirm peekpass never requests the Proton account password or creates credential files.
- [ ] Restart peekpass and confirm the CLI-owned session is detected without another login.
- [ ] Confirm every accessible vault contributes active items to search results and trashed items are excluded.
- [ ] Search by title, username/email, URL, notes, and a non-sensitive custom field.
- [ ] Copy username, password, and TOTP using both shortcut presets.
- [ ] Confirm sensitive clipboard content is cleared after 30 seconds and excluded from clipboard history.
- [ ] Open details for each available item type and verify sensitive fields remain concealed by default, can be toggled via Show/Hide or copied directly, and display a live TOTP countdown when configured.
- [ ] Run manual sync and confirm the current vault contents are reloaded.
- [ ] Use the tray lock action and confirm it signs out of Proton Pass CLI and clears cached items.
- [ ] Lock or log out using the CLI outside peekpass, then try copying a previously cached password; verify the copy is rejected and cached data is cleared.
- [ ] Switch to Bitwarden and back to Proton Pass; confirm provider state and search results do not leak across backends.
- [ ] Reset peekpass and confirm local settings are cleared and installed provider CLIs are signed out.

Record only the tested `pass-cli` version, peekpass commit, Windows version, and pass/fail result. Do not record item names, account addresses, or secrets.
