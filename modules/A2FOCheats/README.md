# A2FOCheats

`A2FOCheats.dll` contains optional, signature-checked cheat behavior which is
independent of mod-specific compatibility modules.

The module replaces Fleet Operations' registered `showmethemoney` handler.
By default, each successful use grants the current player's team:

- 10,000 Dilithium
- 10,000 Tritanium
- 10,000 Metal
- 10,000 Supplies
- 10,000 Crew

Because Armada clamps available Crew to the team's Crew capacity, the handler
first raises that native capacity by 10,000 and then grants 10,000 Crew. This
keeps repeated cheat uses additive instead of silently doing nothing at the
normal starting cap.

Each amount can be overridden in `RTS_CFG.h`:

```cpp
int SHOWMETHEMONEY_DILITHIUM = 10000;
int SHOWMETHEMONEY_TRITANIUM = 10000;
int SHOWMETHEMONEY_METAL = 10000;
int SHOWMETHEMONEY_SUPPLIES = 10000;
int SHOWMETHEMONEY_CREW = 10000;
```

The module reads `RTS_CFG.h` from the standard Data, parent-mod, and active-mod
roots in that order. Overrides are applied per field, so a child mod only needs
to declare the amounts it changes. Values must be literal numbers from 0
through 100,000,000; zero disables that resource grant. A missing or invalid
field retains the lower-precedence value, or the 10,000 default when no root
defines it.

The module also restores four single-player debug cheats that are documented
for Fleet Operations but missing or misregistered in 3.2.7:

- `m`: queues the selected craft to move 200 world units forward
- `dis`: disables the selected craft's shields for 10,000 seconds
- `elim`: eliminates the player/team owning the selected craft
- `crash`: intentionally terminates the Fleet Operations process

Fleet Operations 3.2.7 registers `elim` with the same handler as `expl`, while
`m`, `dis`, and `crash` are not registered at all. The module chains the native
chat initialization, adds the missing commands through the native registration
routine, and replaces an existing `elim` registry entry in place. All four
remain blocked by Fleet Operations' multiplayer-cheat gate. `expl` is left
unchanged and continues to destroy only the selected unit or station.
