# A2FOLogMailer

Optional automatic diagnostic emails to `aclaydon1@googlemail.com`. The module does
nothing beyond reading its settings while consent is off: no helper, collection,
exception handler, game hook, or network request. It is built separately with
`make log-mailer`; it is not a dependency of any other module or the release target.

**Delivery is already configured for players.** The binaries use the distributor's
hosted HTTPS receiver by default, even when the preferences INI is missing.
Players only choose consent and mode. The bundled submission token is distributed
with the game; mailbox credentials remain on the server.

## Installation and consent

1. Back up the installed core DLL before replacing it with the matching newly
   built `A2FOExtensions.dll` that includes the consent control.
2. Copy `build/modules/A2FOLogMailer.dll` and
   `build/modules/A2FOLogMailerHelper.exe` to `Data/modules/`.
3. Optionally copy `A2FOLogMailer.ini.example` to `Data/A2FOLogMailer.ini`.
   Leave `Enabled=0`. A missing INI also defaults to off; the menu creates player
   preferences when saved. No relay configuration is needed.
4. In **Mods → Modules**, select `A2FOLogMailer`, check **Allow automatic log
   emails**, choose the reporting mode, and Save. The menu identifies the recipient.
   Consent is global for this Data installation; module selection remains per mod.
   Mod-required rules cannot replace consent. A missing DLL changes no menu layout.

To revoke consent, uncheck the email box and Save, or set
`[Reporting] Enabled=0`. A running watcher notices this within about half a second.
Revocation cannot undo an email already submitted. Unselecting the module applies
on the next mod launch. Removing both module files disables future launches.

Legacy mods automatically load all installed DLLs; the additional consent setting
keeps this module inactive even in that mode. Required/active entries in a mod's
`info.ini` do not override the consent setting. A player can always disable email.

## Reporting modes and files

**Crash reports only** (`Mode=crash_only`, the default) sends the fresh FO crash
report plus the session's `A2FO*.log` diagnostics after a detected crash.

**All game logs** (`Mode=all_sessions`) sends the session's game log files after
every exit, including a normal exit. Use this when a feature fails without a
crash: reproduce the issue and close the game to submit the logs. This mode also
includes other safe ASCII `.log` files in Data, such as `dxwrapper-armadal.log`.

An out-of-process helper waits for the current game's process handle. In crash-only mode it reports
when either the process exits with a fatal-exception style exit code (excluding
Ctrl+C), or Fleet Ops wrote a nonempty `bugreport.txt` during that session. This
also covers FO catching a crash and subsequently exiting with code zero. An
ordinary successful exit with no fresh native crash report sends nothing in
crash-only mode. All-game-logs mode submits after a normal exit too.

The helper reads only regular files directly inside Data:

- `bugreport.txt`, when its timestamp falls within the current game session.
- ASCII basenames matching `A2FO[A-Za-z0-9_-]*.log`, including extension, renderer,
  startup proxy and refit scene logs, when written during the current session.
- In all-game-logs mode, other `.log` basenames using ASCII letters, digits,
  periods, underscores and hyphens, starting with a letter or digit. The file
  must have been written during the current session.

Old reports, unrelated files, directories and file reparse points are excluded.
There are no recursive scans, arbitrary path settings, memory dumps or screenshots.
Only logs directly in Data are included, not nested log folders or unrelated text
files. Log text can contain local paths, mod names and diagnostic details.

At most 32 attachments and 1 MiB of decoded data are included. Each file is capped
at 64 KiB: the **beginning** of the FO crash report and the **end** of other logs.
FO's report and the main extension log take priority over extra module logs.
Consequently very large logs/reports are partial attachments.

Before transmission, `Data/CrashReports/report-*.json` preserves the exact bounded
report locally. An adjacent `.status.txt` records relay acceptance or failure.
Failure leaves the original logs and saved report intact; there are no automatic
retries, silent background resends, or uploads of earlier sessions. Saved reports
are not automatically deleted; the player may remove them. The file contains no
relay token. It contains log text, which should be treated like the source logs.

Only one HTTPS POST is made. The helper verifies certificates using WinHTTP,
rejects URL credentials, query strings and redirects, and does not use Windows
login credentials for HTTP authentication. It rereads consent before collection
and immediately before upload. No code runs inside the game's crash handler, and
no game exception handling or binary addresses are changed. The child process
remains separate from the game's lifetime; see Microsoft's [process termination
documentation](https://learn.microsoft.com/en-us/windows/win32/procthread/terminating-a-process).

## Hosting the relay

This section is for distributors. The default receiver and public submission
token are compiled from `distribution.hpp` into the core, module and helper.
To use another receiver, change those distributor defaults before building, or
set both `RelayUrl` and `SubmissionToken` in the INI. A partial override fails
closed; the default token is never borrowed for a custom host. Changing defaults
requires rebuilding all three binaries. Never include mailbox credentials here.

For the separate Cloudflare Workers + Gmail option, use the
[Cloudflare setup guide](cloudflare/README.md). It uses a `workers.dev` address
and requires no custom domain. Mailbox credentials remain in Worker secrets.
The following instructions are for hosting the original Python relay instead.

`relay.py` uses Python 3.10+ and its standard library. It listens on loopback HTTP;
put it behind your server's HTTPS reverse proxy exposing `/v1/crash-reports`.
Configure the proxy to reject bodies above 3 MiB, limit request rates, disable
request-body/Authorization logging, and restrict access to the backend port.
The backend's own limit is five requests per minute per connecting IP; a reverse
proxy normally appears as one IP, so this is a global backend limit in that setup.
This is a small low-volume relay, not a managed public reporting service.

Required environment variables on the **server**, supplied through its private
service configuration:

| Variable | Value |
| --- | --- |
| `A2FO_RELAY_TOKEN` | Printable ASCII submission token matching client INI |
| `A2FO_SMTP_HOST` | Sending provider's SMTP hostname |
| `A2FO_SMTP_FROM` | Sender address permitted by that provider |
| `A2FO_SMTP_TLS` | `starttls` or `ssl` |

Optional variables: `A2FO_SMTP_PORT` (587 for STARTTLS, 465 for SSL),
`A2FO_SMTP_USER` and `A2FO_SMTP_PASSWORD` (both supplied together),
`A2FO_RELAY_BIND` (default `127.0.0.1`) and `A2FO_RELAY_PORT` (default 8080).
The recipient is fixed server-side. A client cannot choose recipients. A token
distributed in a game package is an abuse deterrent, not a confidential secret;
keep provider credentials server-side and apply proxy limits for public use.

Validate configuration without sending mail:

```sh
python3 modules/A2FOLogMailer/relay.py --check-config
```

Run the service under your host's process supervisor:

```sh
python3 modules/A2FOLogMailer/relay.py
```

The relay sends attachment emails over verified TLS SMTP. It returns HTTP 202
only after the SMTP server accepts the message; this does not prove inbox
delivery. Invalid/authentication requests and SMTP errors do not echo log text,
tokens or provider errors. The relay has no durable mail queue.

## Verification and limits

`make log-mailer-tests` builds both binaries and the Windows fixture, runs portable
policy tests, and runs the relay's fake-SMTP tests. The Windows fixture can be run
headlessly with Wine; it performs no external network request or live email.

`make log-mailer-loader-smoke` builds an additional fixture which loads the actual
DLL and launches the actual helper, checks disabled startup, and verifies helper
cancellation on module shutdown. Run it with the DLL and helper paths as its two
arguments. It uses a temporary configuration with a loopback-only URL and keeps
its parent alive until the helper exits; it creates no report and sends no email.

Live Windows/Wine crash detection, the visible menu, hosted HTTPS interoperability,
SMTP delivery and inbox receipt still require manual acceptance. A watchdog cannot
report crashes occurring before its module starts, power loss, a frozen process,
or caught crashes that produce neither a fresh native report nor a fatal exit
code. Logs can also be truncated if the game is immediately relaunched before the
watcher snapshots them. A forced kill may resemble a fatal exit; detection is a
bounded heuristic, not a replacement for FO's own exception reporter.

No game installation or running process is changed by building these targets.
