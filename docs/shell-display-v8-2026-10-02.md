# A2FOShellDisplay v8

Fixes the shell window remaining at Fleet Ops' authored 1200x900 callback size even when another display mode is selected.

The Fleet Ops shell resize callback still receives the legacy canvas dimensions. While inside the verified primary-shell scope, A2FOShellDisplay now forwards the selected `Mode.width` / `Mode.height` to the native resize callback and to the genuine VCL client-size setters. Gameplay, editor, secondary displays and all unscoped paths retain their original dimensions.

Expected diagnostic example at 1920x1080:

`Shell window: borderless, requested=1920x1080, device=1920x1080, client=1920x1080, adapter=0`
