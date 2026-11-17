# Translating the port

This guide explains how the game's texts are organised and what to do to add a new language to the port.

## Where the texts are

The texts on screen come from three places. Only the third one is the port's.

| Source | Examples | How to translate it |
|---|---|---|
| Game data files | Movie subtitles (`FLICS/*.IFF` in No Regret, `FLICS/*.TXT` in No Remorse), computer terminals and in-game messages (`USECODE/EUSECODE.FLX`), conversations | Edit those files in your copy of the game data. The port reads them as the original does. |
| Pictures in the game data | Main menu buttons, options screen, the quit question, the status bar | Edit the pictures, mostly in `STATIC/GUMPS.FLX`. |
| Texts of the executable and of the port | Item names, Weasel's shop, a few messages ("TARGETING RETICLE ACTIVE."), the version lines on the main menu, the Enhancements and controller screens | **The language files** of the `languages` folder: this guide. |

A full translation of the game is usually a translated copy of the game data plus a language file for the port.

## The language files

The `languages` folder sits next to the executable. It holds one file per language, named after the language's id: `en.txt`, `fr.txt`, `pt-br.txt`... An id is written in lower case, with no spaces.

- `en.txt` is the reference: every text the port prints, in English. It is generated from the sources and must not be edited.
- `fr.txt` holds the French texts: those of the French `CRUSADER.EXE` 1.08, which the French data needs, and those of the port's screens.
- Any other file is an additional language.

In the game, **Options**, **+** (Enhancements), **Game**, **Language** cycles through **AUTO** and every file of the folder, showing each file's name. AUTO uses `fr.txt` with the French data and `en.txt` with the English data. The choice is saved in `CRUSADER.CFG` (`language=pt-br`).

### Format

The file is UTF-8 text:

```
# A comment.
@name = PORTUGUES (BRASIL)

# --- enhancements window (src/engine/ui/enhancements_screen.cpp)
ENHANCEMENTS = MELHORIAS
BACK = VOLTAR
Do you want\nanything else? = Quer\nmais alguma coisa?
Cost:%u = Preco:%u
```

- `@name` is the name the game shows for the language.
- Every other line is an English text exactly as in `en.txt`, then `=`, then the translation. The spaces around both are ignored.
- `\n` is a line break and `\\` is a backslash. Line breaks only work where the English text has them, in the questions of Weasel's shop.
- If the translation is left empty (`BACK =`) or the line is missing, the English text is shown.
- Keep the `%` codes of the English text (`%u`, `%02u`, `%ld`) in the same order. The game ignores a translation that changes them and reports it on the error output.
- Lines starting with `#` are comments. The `# ---` lines say where each text is used.

### Letters and fonts

The game's fonts use code page 850. The game converts the file to it, so most accented Latin letters (á, ã, ç, é, ê, õ, ü...) can be written directly. A character outside code page 850 becomes `?`.

A letter is only drawn if the font of the game data has a picture for it. The menus of the port use a font with no accented capitals, which is why the French texts use capitals without accents. To keep the accents, add the letters to the fonts in `STATIC/FONTS.FLX` of your translated data.

The screens have a fixed layout: an option's name and its value share one row. Prefer short texts, and check them in the game.

## Adding a language

1. Copy `languages/en.txt` to `languages/<id>.txt`, for example `languages/pt-br.txt`.
2. Set `@name` to the language's name, as players should see it (in capitals for the menus: `@name = PORTUGUES (BRASIL)`).
3. Replace the right-hand side of each line with its translation. Leave the left-hand side unchanged. A text you do not translate yet can stay in English.
4. Optionally, run `python scripts/update_languages.py` from the repository. It replaces the header copied from `en.txt`, puts the lines in the reference order, and adds an empty line for every text missing from the file.
5. Test in the game: put the file in the `languages` folder next to the executable, start the game, and pick the language in **Options**, **+**, **Game**, **Language**. The file is read again every time the language is picked, so after editing it, pick another language and then yours again. Translations the game rejects are reported on the error output (`stderr`).
6. To ship it with the port, open a pull request that adds the file to `languages/`. The release packages include the whole folder.

## When the port's texts change (developers)

The game's code writes its texts in English with `tr("...")`: `tr("BACK")` returns the translation of `BACK` in the language in use. The level editor's own texts are not in the language files: they still use `tr("french", "english")`, and only the editor may use that form.

After adding, changing or removing a text in the code (a `tr("...")` call, an item name in `src/core/dtable.cpp`, a controller label in `src/core/pad_config.hpp`), run:

```sh
python scripts/update_languages.py
```

The script rewrites `en.txt` and brings every other file up to date. It keeps their comments at the top, their `@name` and their translations. It adds new texts with an empty translation, and moves texts the game no longer uses to a commented section at the end of the file. Then complete `fr.txt`.

The `language_files` test (`ctest`) runs the script with `--check`. It fails when a file is out of date, when `fr.txt` misses a translation, or when game code calls `tr()` with two texts.

Two different texts must not share the same English wording, because a language file has one translation per English text. For example, the controller action that selects objects is "SELECT OBJECT", so that it does not collide with the "SELECT" button.

## Reference

- `src/core/translation.hpp`: file format and code page conversion.
- `src/core/language.hpp`: `tr()`, the choice of language, the `languages` folder.
- `scripts/update_languages.py`: generation and checks.
- [enhancements.md](../enhancements.md), section 8b: the Language option.
