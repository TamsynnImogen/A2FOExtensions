# STA1 Classic development parent

Install this template as:

```text
Data/Mods/STA1 Classic/
```

The first skeleton inherits `Fleet Ops 4.0` and supplies `A1Compat.dll`. It
does not yet contain the proprietary Armada 1 races or assets and is therefore
not a playable Armada 1 conversion by itself.

The development build stages the compatibility modules under:

```text
build/sta1-classic/modules/
```

It also supplies the four stock Armada 2 race CFG layouts in `misc`. Armada 1's
interface layouts are not compatible with the A2/FO executable. The A2 files
inherit Fleet Operations' complete `gui_interface.cfg` contract while retaining
the filenames used by both games (`gui_fed.cfg`, `gui_bor.cfg`, `gui_kli.cfg`,
and `gui_rom.cfg`).

The intended installed layout is:

```text
STA1 Classic/
  info.ini
  a1compat.ini
  modules/
    A1Compat.dll
    A2FORGBTextures.dll
  AI/
  bzn/
  misc/
  odf/
  sod/
  sounds/
  sprites/
  techtree/
  textures/
```

`A2FORGBTextures.dll` remains presence-based: it only redirects legacy TGA
lookups when an active extension root contains `Textures/RGB`,
`Textures/Index8`, or `Textures/Compressed`.

`a1compat.ini` is the activation marker required by `A1Compat.dll`. Keep it in
the parent root so child addons inherit the compatibility policy without
shipping another DLL or marker.

The Wingman compatibility alias also supplies the 13 stock defaults recorded
in `odf/ships/a2craft.odf` when an object and all of its included parents omit
them. Explicit or inherited values always win.

Construction rigs receive the same treatment for the six common constructor
commands in `odf/ships/a2const.odf`; their `constructionrig` classlabel remains
native and is not aliased.

Freighters receive the seven mining/resource defaults from
`odf/ships/a2freight.odf` under the same missing-only rule; their native
`freighter` classlabel is also retained.

The current A1Compat build restores A1 starbase officer capacity, upgrade
limits, sequential `oq1`, `oq2`, ... model reveals, ownership changes, and
save/load state. An installed Armada 1 `gui_global.spr` must retain the four A1
button registrations `b_fedoff`, `b_klingoff`, `b_romoff`, and `b_borgoff`.
Their stock textures are `gbfoffq`, `gbkoffq`, `gbroffq`, and `gbboffq`.
Because A2/FO removed the race `officerUpgradeODF` command, the four upgrade
ODFs must also declare their owning `race` so FO's ordinary Producer menu
selects the correct one from the A1 starbase build list.

An original Armada 1 mod should become a child addon by setting:

```ini
ParentMod="STA1 Classic"
AssetVersion=50000
standalone=0
```

Original A1 assets are local validation inputs and must not be committed to the
A2FOExtensions source repository.
