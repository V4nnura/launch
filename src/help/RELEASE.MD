# Launch! 3.772

## SysBar actions

Enable SysBar in Configuration to use these left-click actions:

| Field | Action |
| --- | --- |
| MEM | Close Launch! and send `MEM /C` to the command processor. |
| C: free space | Close Launch! and send `FREE C:`. |
| CL / NL / SL | Toggle Caps Lock, Num Lock or Scroll Lock through the BIOS keyboard state. The BIOS synchronizes keyboard LEDs. |
| Country | Open **Enter country code:** with a live country-name lookup. OK closes Launch! and sends `COUNTRY <code>`; Cancel changes nothing. |

ENV, mouse, video and network indicators are informational. Country names use DOS country codes, including legacy DOS names. Unknown codes remain available for the operating system to validate. Reopen Launch! after a command to refresh the displayed values.

## Windows clock button

Start the Windows menu companion with `/CLOCK`:

```
!W30.EXE /CLOCK
!W31.EXE /CLOCK
```

Use the executable for your Windows version; `!W3X` refers to these companions. Add the option to the Program Manager command line if you want the clock whenever Windows starts Launch!.

The live clock replaces the normal Launch!/suite-title button label. It follows Windows 3.x regional time settings: 12/24-hour format, leading hour zero, time separator and configured time suffix. Seconds update using the existing timer. Changes to Windows regional settings are picked up while the program is running.

Right-click management labels (**Manage**, **Edit...**, **Remove...**) take precedence. The clock returns when that action finishes. `/CLOCK` also enables the clock on an already-running resident button. Ordinary later shortcut invocations leave it enabled; restart without `/CLOCK` to return to the title label.

The implementation uses Windows 3.0-compatible `GetProfileInt` / `GetProfileString` and `[intl]` settings (`iTime`, `iTLZero`, `sTime`, `s1159`, `s2359`), rather than newer Windows-only formatting APIs.

## Closing the Windows companion

`!W30.EXE /CLOSE` or `!W31.EXE /CLOSE` closes the resident Launch! button and exits the Windows companion. It is safe to run when no button is resident.

When Windows integration is selected, Install also adds the matching companion (`!W30.EXE` for Windows 3.0 or `!W31.EXE` for Windows 3.1/3.11) to the `[windows]` `load=` line in `WIN.INI`. Existing `load=` programs are retained and separated with semicolons.

## Screen columns

In **Configuration > Font**, **Screen columns** sits above **Persist**:

- **80 cpl** is the default.
- **40 cpl** selects the BIOS 40-column colour text mode. Characters occupy twice the horizontal space; their font bitmaps and vertical proportions are retained.

Click OK to apply the selected width. Cancel restores the previous choice. The width is saved with the font settings as `SCREEN_COLUMNS=40` or `SCREEN_COLUMNS=80` in `LAUNCH.CFG`; it is independent of the font-resident **Persist** checkbox. `!START.BAT` calls `!CONFIG.EXE /DISPLAY`, which applies the saved screen width and font without opening Configuration or the main menu. Configuration and installer upgrades preserve the selected width.

At 40 columns, the menu shows at most two panels at once. Opening deeper folders shifts the visible pair; Left returns to the parent. Tall panels scroll to keep the keyboard selection visible. SysBar uses two rows. Configuration and fixed-width core dialogs temporarily use 80 columns, then return to the selected width.

This option targets colour DOS text modes on EGA/VGA. Launch! does not switch a Windows-owned display or monochrome mode 7. Tiny retains its limited font UI. Accessories, games and external programs may require 80 columns; compatibility of their layouts at 40 columns is not guaranteed. Selecting 80 cpl remains available in Configuration.

## Typo Supersonic Mode

When Typo's displayed typing rate exceeds **70 WPM (temporary TC3 test threshold)**, each typing keypress briefly changes the dialog's Panel colour to **Bright White**. Each pulse lasts until the next BIOS timer tick; it does not block input or use the hardware blinking-text attribute. The original Panel colour returns between pulses. Faster input produces more frequent pulses, which may merge at very high speeds.

Pulses stop when paused, finished, below the threshold, or disabled by the accessibility setting below. Backspace counts as a typing keypress if the resulting rate still exceeds the threshold; navigation and toolbar shortcuts do not trigger pulses.

## Disable flashing effects

This accessibility preference is configured only in `LAUNCH.CFG`. There is no UI control.

```
flashing=true
```

The default is `true` when the key is absent. To disable all deliberate flashing/strobing animation effects in Pop, Metro and Typo, set:

```
flashing=false
```

- Pop removes a selected group without its colour-flash sequence and uses a steady selection highlight without the hardware blink bit.
- Metro centers the failed station and presents the normal failure state without its colour-cycle animation.
- Typo does not produce Supersonic flashes at any typing speed.

Game rules, scores and normal static selection indicators remain active. Restart an already-running game after changing this setting. Key/value capitalization and surrounding whitespace are accepted. An explicitly supplied value other than `true` disables flashing, including an empty or invalid value.

Saving Configuration preserves this setting. Resetting visual configuration also preserves the accessibility preference. New factory configuration starts with `flashing=true`; upgrade configuration retains the user's value.

## TC3 regression corrections

- SysBar block dividers use CP437 character 179.
- `flashing=true` restores Pop, Metro and Typo effects after `false`; restart the game after editing the file. The parser is safe with the older compiler's character macros.
- Stack: F11 or the title-bar maximize control opens fullscreen. Five older-card layers remain above/right of the active card; click exposed titles to navigate. Prev, Next, Add, Delete, Export, Print and Restore remain available. F11/Esc restores the window.
- Journal: F11 or the title-bar maximize control opens fullscreen, showing 22 text rows with the dated paper centered on the screen. The saved 59-character writing width is retained. F11/Esc restores the window. Ctrl+G, Ctrl+Left/Right, Ctrl+Ins, Ctrl+S Export and Ctrl+P remain available; `/PRINT` editing restrictions still apply.
- Double-click a Note tab or Markdown page tab to toggle fullscreen. The click interval is approximately half a second.
- Markdown Focus retains Ctrl+M Document Map, Ctrl+Ins Characters and F5 Show. Ctrl+/ cycles Split, Source and Preview; from Focus it returns to the selected window view. Enhanced-keyboard Ctrl+/ encodings are normalized.
- Note Export produces one `NOTE<n>.TXT` containing every persistent tab and page. External pages are excluded and keep their separate Save behavior. Single CP437 rules separate pages; double rules separate tabs. Widened pages are reflowed before saving their compatible persistent records.
- Markdown printing offers three choices: `Basic` sends the unformatted Markdown source as ordinary text to LPT1; `Epson ESC/P` uses resident printer fonts, bold, italic and underline, with a three-column margin, 72 printable columns and 60 rows per sheet; and `Postscript PS` writes a PostScript document to the Launch! `EXPORT` directory. ESC/P headings use Sans Serif; body text uses Prestige; tables use Sans Serif; blockquotes use Script; code uses Draft; and media placeholders use Draft Condensed. CP437 single/double box-drawing rules are retained for tables and heading rules. The PostScript output uses standard host fonts as close equivalents to the resident printer fonts. The same three-choice popup is used by Calendar, Journal, Note, Stack and To-Dos for their current view, while Calculator remains Basic-only. Markdown Show/Viewer has no print UI and sends formatted ESC/P directly on Ctrl+P.
- Wordz Hints Next uses the same button and shadow renderer as other controls.

TC3 includes host regression checks (`CHECK772.PY`, `CHECKTC3.PY`). These do not replace a native Microsoft C 7 build, DOS mouse/keyboard testing or a physical print test.
