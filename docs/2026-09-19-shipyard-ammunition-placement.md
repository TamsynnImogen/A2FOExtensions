# Shipyard ammunition placement

Report: GUI-configured torpedo-count placement works for ordinary Craft but
fails on a station classified as a shipyard, including repair-only yards.
The precise visual symptom has not yet been confirmed in the reporting mod.

The producer render hook was already installed. Its anchor selection differed
from the ordinary panel: it used the producer name/class GUIText rather than
the initialized captain GUIText. A usable producer name rectangle also stopped
the fallback search even when its matching CFG rectangle was missing; it was
then treated as captain-relative without being rebased. Hidden producer labels
could leave no anchor at all.

Both panels now prefer the shared captain component at `InfoDisplay+0xbc`,
retaining the same geometry, font, and display context. Disassembly of the
supported Armada executable confirms that the common InfoDisplay initialization
creates this component and its live rectangle before either render path runs.
The producer panel need not draw a captain name for this component to exist.

Name/class fallbacks remain available. With a configured captain rectangle,
a fallback needs its own matching configured rectangle before it can be
rebased. Without a configured captain rectangle, the existing automatic rows
below the available name/class anchor are retained.

Validation passed:

- Native CraftIdentity tests, including shared torpedo coordinates, hidden or
  displaced producer labels, missing source configuration, class fallback, and
  automatic-row fallback.
- MinGW build of `A2FOCraftIdentity.dll`.
- Four modder documentation tests and whitespace checks.
- Headless Wine DLL-loading/export smoke. Wine printed Mesa initialization
  warnings but the test exited successfully.

Only `Data/modules/A2FOCraftIdentity.dll` was deployed, with a verified SHA-256
of `c7a37e354e3fc7f1fe1cfd13cf8cebfa368d3f87befe4920f67b07be03755f94`.
The previous DLL and manifest are under
`Data/rollback/2026-09-19-shipyard-ammunition-132032/`.

In-game confirmation is pending: compare the configured Photon/Quantum rows
on a ship, an ordinary station, and a repair-only shipyard in the reporting
mod. These headless tests verify anchor selection and rectangle calculations;
they do not establish that the reported visual symptom is resolved.
