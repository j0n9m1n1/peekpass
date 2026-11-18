# Translations

peekpass uses Qt Linguist translation files.

To add a language:

1. Add a new `peekpass_<locale>.ts` file, for example `peekpass_ja.ts`.
2. Add that file to `PEEKPASS_TRANSLATIONS` in `CMakeLists.txt`.
3. Refresh the source strings with Qt `lupdate`, then translate them in Qt Linguist.
4. Build normally. CMake runs `lrelease` and copies the generated `.qm` file beside the executable under `translations/`.

`src/TranslationCatalog.cpp` keeps current user-visible strings discoverable for Qt
translation tooling, including strings selected dynamically by password-manager
backends.

Korean is provided in `translations/peekpass_ko.ts` (compiled into
`translations/peekpass_ko.qm` by `lrelease`) and is also supplied as an inline
fallback to `I18n::translate` so the app remains bilingual even if `.qm` files
are absent. New languages must provide completed `.ts` translations because they
do not have an inline fallback.
