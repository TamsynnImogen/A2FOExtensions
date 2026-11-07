# A2FORefitMenus — main-menu candidate, 18 October 2026

The first main-menu milestone is implemented as an optional Windows i386
module in `modules/A2FORefitMenus/`. It adapts Refit's left-column layout,
gunmetal/amber palette, rounded buttons and hover/pressed states to C++ GDI
inside the existing FO main form. It does not add another rendering device.
The DLL is built and packaged. The corrected October Roots build is deployed and enabled for the next launch; visual game acceptance remains pending. See `refit-menus-roots-20261018-215041-deployment.json`.

This is a presentation adaptation of pinned upstream
`ui/src/menus/screens/main_menu.rs` and `ui/src/widgets/theme.rs`, not a port
of the complete Rust UI. Stock background artwork, the 3D menu scene, HUD,
loading screens and other menu screens are outside this milestone. No fonts
or game artwork are copied. The upstream MIT notice is retained.

## Implemented behavior

- Preserve the real FO main form, initialization, modal loop and cleanup.
- Call its original FormShow before attaching the new presentation.
- Read localized action captions and enabled/visible flags from the real form.
  Single Player therefore follows the installed game's availability.
- Hide nine original visual controls only after validating the entire form,
  actions, control owners/parents and visibility values, and installing the
  scoped window subclass. No other forms are skinned.
- Draw into the existing main window's physical client area, with fitted
  layout and offscreen GDI buffering. Source code creates no window/device.
- Handle the player's ordinary mouse and navigation keys in that form.
  Queue actions until after painting, recheck native action state and reject
  stale generation tokens or duplicate pending actions.
- Invoke actual native FO action methods, with Delphi EAX=self / EDX=sender.
  No synthesized mouse/keyboard events are used for dispatch or testing.
- Restore saved native control visibility and repaint on presentation failure
  or requested shutdown. Destruction removes the subclass and leaves the
  native form lifecycle intact. The patched DLL remains pinned resident;
  restart removes the hook entirely.

The seven entries are Single Player, Instant Action, Multiplayer, Options,
Map Editor, Credits and Exit. Refit's Replay Intro row is replaced by the
verified FO Map Editor action. Options retains the existing route to Mods
and extension/module settings; its live navigation still needs acceptance.
The footer identifies FO/A2FOExtensions; the original version-label website
link is not part of this first presentation.

## Reviewed bindings

All addresses here are FleetOpsHook RVAs, not Armada executable addresses.
Original event bytes contain no address operands requiring relocation in the
copied eight-byte hook block.

| Contract | RVA / offset | Evidence |
| --- | --- | --- |
| TMainForm.FormShow | 0x001CC330 | Exact 15-byte body checked; first eight whole instruction bytes hooked |
| TMainForm VMT | 0x001CB7A8 | Native class-name pointer at VMT−44 resolves to TMainForm; instance size 0x3B4 |
| Form HWND | instance +0x1B4 | Native TWinControl.GetHandle at 0x000CD81C, plus exact TMainForm window class |
| Parent window | instance +0x1C4 | FormShow re-enables this parent; never used as the presentation window |
| Native SetVisible | 0x000C563C | Full 54-byte body; native control visibility at +0x57 |
| Action caption | action +0x64 | SetCaption at 0x000D7444; bounded native AnsiString reads |
| Action enabled / visible | action +0x6A / +0x86 | Native setters at 0x000D75AC / 0x000D78B8 |
| Single Player | 0x001CC854 | Native dispatch 2, then Close(self) |
| Instant Action | 0x001CC28C | Native dispatch 258, then Close(self) |
| Multiplayer | 0x001CC2A4 | Native dispatch 13, then Close(self) |
| Options | 0x001CC2BC | Native dispatch 25, then Close(self) |
| Map Editor | 0x001CBD54 | Native dispatch 300, then Close(self) |
| Credits | 0x001CC274 | Native dispatch 30, then Close(self) |
| Exit | 0x001CBDD0 | Native dispatch 0, running state 2, then Close(self) |

Runtime admission retains reviewed Armada identity/profile checks and excludes
the SDK's timestamp-only canonical shortcut. FO admission uses the original full-file CRC/length profiles plus the reviewed October 12 Roots DLL, which also requires its complete SHA-256. Thirteen exact live binding signatures are still required. Unknown inputs or competing hooks stay native. The following files were independently checked:

| Input | SHA-256 |
| --- | --- |
| Local patched FleetOpsHook.dll | 0843058788c6ffd1138d69871e2d9b1911bb6be8ee06db42f59f56b9a4494a97 |
| Clean RE FleetOpsHook.dll | b53e91281be33ee998e30bd971b6c944cb4f5fb2174fb9f0cc756ca3ef94e8a9 |
| Installed October Roots DLL | 3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11 |

The October DLL is byte-identical to the retained October 12 release. It
changes 30 bytes from the approved September Roots DLL, only at the branch
near RVA 0x13F24A and handler padding near 0x1FE110. The complete main-menu
unit, relevant VCL routines, class layout and embedded resources are unchanged.
The September Roots profile remains admitted as before.

## Activation correction

The user's screenshot still showed FO's native menu. The actual extension log
said `Module loader: skipped A2FORefitMenus.dll`: `Mods/test/info.ini` owns
an explicit active list that overrides the global list, and omitted this
module. The root `A2FORefitMenus.ini` was also absent. The deployed FO DLL
was the October build above, requiring a new exact profile.

The corrected DLL is installed, `active42 = "A2FORefitMenus"` is appended only
to Test's existing list, and the global `[RefitMenus] Enabled=1` INI is created.
All prior Test entries and the global/parent mod lists are preserved. Original
DLL/config files and logs are backed up under
`.deployment-backups/refit-menus-roots-20261018-215041/`.
ArmadaL.exe, FleetOpsHook.dll, core DLL and compatibility profile are unchanged.
This is a next-launch deployment; no GUI launch or input automation occurred.

## Verification evidence and limits

- DLL, native console fixture and memory-DIB render fixture compile with
  `-Werror`. DLL exports are ModuleInit, ModuleShutdown and QueryStatus;
  imports are COMCTL32, GDI32, KERNEL32, msvcrt and USER32.
- Both portable suites pass with assertions active under `-DNDEBUG`:
  eight display sizes including portrait/ultrawide, hidden/disabled rows,
  geometry boundaries, navigation, native data capture, localized ANSI text,
  relocated model addresses, rejected malformed data, queued-action
  cancellation and state revalidation.
- File audit passes all thirteen native signatures and the class/size check
  for all three FO inputs. See `build/refit-menu-bindings-20261018.json`.
- **33/33 console DLL fixture processes pass** with the installed ArmadaL
  (`817b1ddb9876cfb9c5d781b42d2513de48fef9cbdbc408aa43c878c573581a18`)
  and all three FO inputs above: missing/disabled/invalid
  config, short API, altered FO/Armada identity, unknown complete file
  profile, changed hook/action bytes, failed patch and successful setup.
  Success checks the real checked inline patch and Delphi self/sender
  forwarding across all four possible native four-byte stack alignments,
  invalid-HWND native fallback, idempotence and shutdown forwarding. The original-event continuation is a fixture stub.
- In each success case, all seven real FO action bodies execute in a private
  data mapping. Three side-effect helpers are redirected to fixture recorders:
  dispatch setter 0x001DDCC0, running-state setter 0x001DDCD0 and Close
  0x000B1B08. Tests verify all dialog IDs, original self and Exit state 2.
  The real GetHandle body is also exercised with HandleNeeded isolated,
  proving that it returns the form’s own HWND at +0x1B4 instead of the
  distinct parent value at +0x1C4. No actual game global, native Close,
  window creation or game entry point executes.
- **Offscreen GDI render passes**: real renderer into a 1280×720 memory DIB,
  checked gradient/title and normal/hot/pressed/disabled pixels, invalid
  paint arguments rejected. BMP and lossless PNG preview produced without
  creating an HWND or automating input. Preview was visually inspected.
- Independent read-only review identified a paint-failure fallback defect;
  it was fixed and re-reviewed before packaging.

Logs: `build/refit-menu-roots-build-20261018.log`,
`build/refit-menu-roots-native-20261018.log`,
`build/refit-menu-render-20261018.log`.
Preview: `build/refit-menu-preview-20261018.png`.

These checks do not exercise a live VCL form. Real painting, keyboard routing,
screen transitions, cleanup, display changes and A2FOShellDisplay coexistence
remain manual acceptance items. Other menus, multiplayer sessions and gameplay
have not been changed or validated by this milestone. No whole-project legacy
test-suite result is claimed.

## Build, candidate and acceptance

```sh
make refit-menus refit-menus-test build/refit_menu_native.exe build/refit_menu_render.exe
python3 tools/verify_refit_menu_bindings.py --fo FleetOpsHook.dll \
  --fo reverse_engineering/FleetOpsHook/FleetOpsHook.dll
make refit-menus-native-test REFIT_WINEPREFIX=/absolute/initialized/disposable/prefix
```

Candidate: `dist/A2FORefitMenus-roots-20261018.zip`. It contains the DLL,
disabled INI, module and verification documentation, preview, evidence,
manifest and retained upstream license. No engine EXE, FO DLL or replacement
core is shipped. The local Roots deployment is recorded separately; other game installations are untouched.

For a controlled manual test, install into an existing compatible extension
setup: DLL under `Data/modules/`, INI under the extension root (`Data/`),
preserve the notice, append an unused `[modules] activeN = "A2FORefitMenus"`
entry and set `[RefitMenus] Enabled=1`. Preserve the existing module list and
restart fully. The optional QueryStatus export reports installation, active
presentation, attached forms and dispatched actions separately.

Confirm fresh logs report arming and attachment. Check each action and return
to the main menu, including Options → Mods/modules and Map Editor. Check
conditional Single Player, mouse press/release cancellation, Tab/arrows/Enter,
Alt+Tab, windowed/borderless operation, multiple resolutions and the existing
A2FOShellDisplay module. Disable the INI and restart for rollback. Broader menu
replacement follows only after this main-menu candidate is accepted.
