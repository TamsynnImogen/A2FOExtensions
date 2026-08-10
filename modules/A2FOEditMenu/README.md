# A2FOEditMenu

`A2FOEditMenu.dll` extends the map editor's existing ODF menu hierarchy
without changing its established commands.

The stock hierarchy remains valid:

```text
editmenu.odf: menuNameX -> category ODF
category ODF: buildItemX -> object-list ODF
object-list ODF: itemX -> placeable object
```

A `buildItemX` target may now contain more `buildItemX` commands. Such a file
opens as another submenu. A target with no `buildItemX` commands remains a
normal `itemX` object list.

```cpp
// e_fed.odf
menuTitle = "Federation"
buildItem1 = "ef_ships.odf"

// ef_ships.odf -- an additional submenu
menuTitle = "Federation Ships"
buildItem1 = "ef_combat.odf"
buildItem2 = "ef_support.odf"

// ef_combat.odf -- an ordinary placement list
menuTitle = "Federation Combat Ships"
item1 = "fbattle"
item2 = "fcruise1"
```

Each visible level retains Armada's 12-entry limit. Recursion is limited to
32 levels and cycles are stopped safely. The native Back command returns one
level at a time, and native object placement, function-key labels,
`forceToNeutral`, and Fleet Operations' enhanced edit-menu renderer remain in
use.

The recursive lookup indexes loose `.odf` files in Data and the active
ParentMod chain. Packed-only menu files continue to use the stock three-level
behaviour unless a loose copy is supplied.
