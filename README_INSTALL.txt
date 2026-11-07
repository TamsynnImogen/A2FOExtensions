A2FOExtensions / Premonitions follow-up fix
===========================================

This overlay is intended to be copied over the current A2FOHookExtensions
source tree, then rebuilt.

Changes in this revision
------------------------

1. Premonitions Dominion patrol plane
   - DOM1PATROL through DOM6PATROL remain the real fed1.bzn paths.
   - Their authored A1 gameplay plane is Y=38.5.
   - A1Premonitions.lua clamps the six Dominion craft back to Y=38.5 every
     gameplay tick, including while attacking, because Fleet Ops' 3D steering
     can otherwise drift them vertically out of A1-era weapon reach.
   - Goto(path) still tries first; stalled paths retry through the live
     ScriptInterface::Patrol route.

2. Premonitions end-of-mission crash
   - Mission:finish() no longer calls SucceedMission/FailMission while a Lua
     Timer/luabind callback is still unwinding.
   - A2FOMissions queues the result, then hands it to the native mission system
     from the next input callback with a small native grace delay.
   - MMM Timer::stop now correctly erases the stopped timer from its global
     vector.
   - Unsafe legacy rule destruction/global rule purge calls are not used during
     mission teardown on the supported ArmadaL/Fleet Ops build.

3. A1 embedded bridge BIK tiling
   - F1DEM1.bik is 400x200 (other A1 bridge movies use similar dimensions).
   - ArmadaL's embedded Bink renderer was found to allocate a fixed 256x128
     backing texture with UV scales for that backing. A 400x200 frame therefore
     wrapped/tiled exactly as seen in-game.
   - A2FOFeaturePack now signature-validates and expands that backing to
     512x256, with matching 1/256 and 1/512 UV constants.
   - Inset bridge/comms movie rectangles remain unscaled; fullscreen/menu Bink
     handling keeps the existing FeaturePack behaviour.

4. A1 alpha experiment reverted
   - The previous "missing alpha = opaque white" policy has been completely
     removed. Authored texture formats again use the normal legacy loader path.

5. Zoomed-out A1 manual mip handling
   - Same-mod authored name_1.tga/name_2.tga/etc. are now preserved rather than
     redirecting the request back to name.tga.
   - This targets the corruption that appears only after zooming far enough to
     switch LOD/mip level.
   - Cross-root incompatible inherited mip chains still retain the existing
     guarded synthesis path.

Build
-----

From the A2FOHookExtensions source root:

    rm -rf build/obj/A2FOMissions build/modules/A2FOMissions.dll
    make missions -j$(nproc)

    make build/modules/A2FOFeaturePack.dll \
         build/modules/A1Compat.dll \
         build/modules/A2FORGBTextures.dll \
         -j$(nproc)

A1Compat itself contains no new feature in this revision; rebuilding/deploying
it restores the pre-alpha-experiment source if the previous test build was
installed.

Deploy
------

Replace:

    Data/modules/A2FOMissions.dll
    Data/modules/A2FOFeaturePack.dll
    Data/modules/A1Compat.dll
    Data/modules/A2FORGBTextures.dll

Copy:

    armada_missions/a1/missions/A1Premonitions.lua

to:

    Data/Mods/STA1 Classic Mod/missions/A1Premonitions.lua

Then fully restart Fleet Ops.

Useful log lines
----------------

FeaturePack should report:

    Armada embedded Bink backing enlarged from 256x128 to 512x256 with matching UV scales

A2FOMissions should report at mission completion:

    MMM deferred mission result handed to native mission system

A2FORGBTextures should now say:

    Legacy manual-mip root request preserved: ..._1.tga -> ..._1.tga

rather than normalizing _1 back to the base texture.
