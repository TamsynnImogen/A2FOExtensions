# A2FO Log Mailer Cloudflare relay

This folder contains the separate Cloudflare Worker that receives A2FO Log
Mailer reports and sends them to `aclaydon1@googlemail.com` through Gmail SMTP.
It uses Cloudflare's TCP sockets for SMTP over STARTTLS. The existing Pokedex
Worker, `webdex.aclaydon1.workers.dev`, is unrelated and is not changed by these
steps.

The Worker accepts `POST /v1/crash-reports`. It does not store report contents
in its rate-limit Durable Object. It permits at most five report attempts per
minute and 100 per day by default (the daily cap is configurable up to 400);
failed SMTP attempts count toward both limits. It does not automatically retry
failed mail. A successful HTTP
response means Gmail's SMTP server accepted the message, not that it reached
the inbox.

## Before you start

- Have access to the Cloudflare account that owns the `aclaydon1.workers.dev`
  Workers namespace.
- Have access to the Google account for `aclaydon1@googlemail.com`, with
  2-Step Verification enabled.
- Use a recent Node.js installation so `npx wrangler` can run.
- Run the project's checks before deploying. Deploy only from this folder.

No custom domain, route, static asset, or paid plan is needed for this design.
The Worker has no relationship to the Pokedex Worker beyond using the same
Cloudflare account.

## 1. Create a Gmail app password

1. Open [Google App Passwords](https://myaccount.google.com/apppasswords) and
   sign in to the Google account for `aclaydon1@googlemail.com`.
2. Create an app password named `A2FO Log Mailer` and copy the generated
   password. Google may display it with spaces; enter it without spaces.
3. Keep this password private. It is the mailbox credential: never put it in
   the game INI file, source code, repository, or a message.

Google only offers app passwords when the account's security setup allows
them. If that page does not offer the option, stop and resolve the account
settings directly with Google; do not substitute the normal Google password.

## 2. Confirm the Cloudflare account

Open a terminal in this `cloudflare` directory and run:

```sh
npx wrangler login
npx wrangler whoami
```

Complete Cloudflare's sign-in in your browser yourself, then check that
`whoami` identifies the intended account. If you have multiple Cloudflare
accounts, do not continue until the correct account is selected. These commands
authenticate Wrangler; they do not deploy the existing Pokedex Worker.

## 3. Deploy this Worker

From this directory, after the project's checks have passed, run:

```sh
npx wrangler deploy
```

The configuration names this Worker `a2fo-log-relay` and includes the
Durable Object migration. Wrangler should report the deployed Worker URL. If
your account's Workers namespace is `aclaydon1`, the expected endpoint is:

```text
https://a2fo-log-relay.aclaydon1.workers.dev/v1/crash-reports
```

Use the URL Wrangler reports if it differs. The expected URL is not available
until Cloudflare has created the Worker in that account. A deployment with
missing secrets fails closed and returns a service error; add the secrets
before testing report delivery.

## 4. Add the two secrets

In Cloudflare, open **Workers & Pages**, select **a2fo-log-relay**, then open
**Settings → Variables and Secrets**. Add these as encrypted secrets, not plain
text variables:

| Secret name | Value |
| --- | --- |
| `A2FO_RELAY_TOKEN` | A random printable ASCII token at least 32 characters long |
| `GMAIL_APP_PASSWORD` | The 16-letter Google app password from step 1, with whitespace removed |

Save the changes and redeploy/restart the Worker if Cloudflare prompts you to
activate the updated settings. The relay token is also configured in the
distributor's client INI as `SubmissionToken`. Because that token ships with
the game, it is an abuse deterrent and must not be treated as confidential.
The Gmail app password stays only in Cloudflare and must never be distributed
to players.

To generate a submission token, run this locally and use its output for
`A2FO_RELAY_TOKEN` and the client INI's `SubmissionToken`:

```sh
python3 -c 'import secrets; print(secrets.token_hex(24))'
```

The Worker requires a 32 to 2,040 character printable ASCII submission token
and exactly 16 letters for the app password after removing whitespace. Its
configuration supplies `GMAIL_USER=aclaydon1@googlemail.com` and
`MAX_REPORTS_PER_DAY=100` by default. The daily cap can be raised to at most
400. Do not change these values unless there is a specific operational need.
The SMTP destination is Gmail at `smtp.gmail.com:587` using STARTTLS, and the
recipient is fixed in the Worker.

## 5. Configure the game client

Set the distributor's `A2FOLogMailer.ini` values to the URL Wrangler reported,
ending in `/v1/crash-reports`, and the same `A2FO_RELAY_TOKEN` value entered in
Cloudflare. Do not put the Gmail app password in the INI. Provide the
submission token to players only as needed for the client configuration; it
cannot be kept secret in a distributed game.

## 6. Test delivery

Run these checks from this exact directory so Wrangler targets this Worker,
not another Worker such as Webdex:

```sh
cd /home/tamsynn/A2FOHookExtensions/modules/A2FOLogMailer/cloudflare
node --test /home/tamsynn/A2FOHookExtensions/tests/log_mailer_cloudflare_test.mjs
npx wrangler deploy --dry-run
```

The Node tests and Wrangler dry run do not publish the Worker or send SMTP
mail. The Wrangler dry run checks this directory's `wrangler.toml`.

You can check `GET https://<worker-host>/health` to see whether the required
settings and Durable Object binding are present. A response with
`"configured":true` means configuration is present; it does not test SMTP or
prove that email delivery works.

To preview the synthetic request without sending anything, run:

```sh
python3 send-test.py
```

It prints the fixture JSON locally. To send one test email containing only
public dummy text, run:

```sh
python3 send-test.py --send
```

The script prompts for the submission token without displaying it; enter the
Cloudflare `A2FO_RELAY_TOKEN`, not the Gmail app password. To use another
Worker URL, add `--url https://<worker-host>/v1/crash-reports`. The script sends
no game logs or Gmail secret. It reports whether Gmail's SMTP server accepted
the message; check the inbox and spam folder to confirm delivery. Live delivery
also depends on Cloudflare TCP socket availability and Cloudflare and Gmail
limits.

## Security and service limits

- The Durable Object stores only a global UTC daily attempt counter and up to
  five recent reservation timestamps for its rolling 60-second limit, not report
  contents. Failed SMTP attempts consume both the minute and daily budgets.
- The attempt cap limits accidental or abusive volume but cannot make the
  distributed submission token secret.
- Gmail credentials are held as Cloudflare secrets. Never commit them or share
  them in chat.
- Cloudflare and Gmail apply their own runtime, quota, and sending limits.
  Consult the current [Cloudflare TCP sockets
  documentation](https://developers.cloudflare.com/workers/runtime-apis/tcp-sockets/),
  [Worker secrets documentation](https://developers.cloudflare.com/workers/configuration/secrets/),
  [Durable Objects pricing and limits](https://developers.cloudflare.com/durable-objects/platform/pricing/),
  and [Gmail sending limits](https://support.google.com/mail/answer/22839).
- See Google's [app password guidance](https://support.google.com/accounts/answer/185833)
  for account eligibility and security details.
