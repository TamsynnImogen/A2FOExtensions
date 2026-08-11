# Persistent shield visibility

A2FOAlwaysShowShields.dll adds one optional object ODF command:

    alwaysShowShields = 1

The default is 0, which leaves Fleet Operations' native shield visibility
unchanged.

When enabled, the object's normal shield geometry remains visible while its
current shield strength is greater than zero. Hits still produce their normal
impact effects. When shield strength reaches zero, the persistent effect is
removed; it returns automatically when shield strength becomes positive
again.

Put the command in the ship or station ODF, or in an included parent ODF when
the whole family should use it. Values inherited through ordinary ODF includes
are supported.

## Runtime ownership

The module deliberately creates an infinite-duration instance of Armada's
type-7 `WEclairlink1` Clairvoyance-link effect. That clean outline is reused as
the always-visible shield visual and retains its native effect colour. Its
effect identifier is tracked separately from the Craft field used by ordinary
impact and collapse effects, so hits retain their native visuals. The module
does not replace shield geometry, damage, regeneration, or gameplay state.

A2FOCraftIdentity.dll owns the shared CraftClass construction hook and forwards
completed ship and station ODFs to this module. A2FOTurrets.dll owns the shared
Craft::Simulate and Craft cleanup hooks and forwards post-simulation and
pre-cleanup observations. This prevents multiple DLLs from competing for the
same engine sites. Starbases override `Craft::Simulate` without entering that
shared path, so this module owns one additional checked
`Starbase::Simulate` observer. Fleet Operations can also replace those native
virtual paths; A2FOHybridBuild therefore forwards its already-owned common
Craft render callback to the same visual observer. The module also observes
the central GameObject mission-publication dispatcher used by both ordinary
object creation and `AiMission::AddObject`. That common boundary covers newly
created hidden Fleet Operations subclasses independently of their
construction and simulation virtuals. As the authoritative fallback, the
module observes Armada's `RenderGameObjects` pass and enumerates the complete
global GameObject list immediately before each frame is built. Hidden Fleet
Operations rendering virtuals therefore cannot omit configured stations. All
four DLLs must be installed in Fleet Operations.

Some Fleet Operations classes may already be cached before the construction
observers run. For those classes, the first simulation resolves the command
from the loose ODF hierarchy using normal Data/parent/active-mod precedence.
Constructor-time ParameterDB lookup remains authoritative for classes loaded
later and supports inherited values from packed ODF archives.

The setting is visual only and does not alter shield strength or damage
resolution. Using the same ODFs and DLLs across multiplayer participants is
still recommended so every client displays the same result.
