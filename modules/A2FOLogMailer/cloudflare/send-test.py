#!/usr/bin/env python3
"""Submit one synthetic report only when explicitly run with --send."""
import argparse
import base64
import getpass
import json
import urllib.error
import urllib.parse
import urllib.request


def payload():
    return json.dumps({
        "version": 1,
        "exit_code": 0,
        "files": [{
            "name": "A2FOSyntheticTest.log",
            "content_base64": base64.b64encode(
                b"Synthetic A2FO Log Mailer setup test. No game data.\r\n"
            ).decode("ascii"),
        }],
    }).encode("ascii")


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--send", action="store_true", help="send one test email through the relay")
    parser.add_argument("--url", default="https://a2fo-log-relay.aclaydon1.workers.dev/v1/crash-reports")
    args = parser.parse_args()
    if not args.send:
        print(payload().decode("ascii"))
        return 0
    url = urllib.parse.urlsplit(args.url)
    if (url.scheme != "https" or not url.hostname or url.username or url.password
            or url.path != "/v1/crash-reports" or url.query or url.fragment):
        parser.error("use the HTTPS relay URL ending in /v1/crash-reports")
    print("Sending one synthetic setup email to aclaydon1@googlemail.com.")
    token = getpass.getpass("Submission token (hidden; NOT your Google app password): ")
    if not 32 <= len(token) <= 2040 or any(not 33 <= ord(c) <= 126 for c in token):
        parser.error("submission token must be 32 to 2040 printable ASCII characters")
    request = urllib.request.Request(args.url, data=payload(), method="POST", headers={
        "Content-Type": "application/json", "Authorization": "Bearer " + token,
        "User-Agent": "A2FOLogMailer/1",
    })
    opener = urllib.request.build_opener(NoRedirect())
    try:
        with opener.open(request, timeout=15) as result:
            status = result.status
    except urllib.error.HTTPError as error:
        print(f"Relay returned HTTP {error.code}; check Worker settings and quotas.")
        return 1
    except (urllib.error.URLError, TimeoutError, OSError):
        print("Could not reach the relay. No automatic retry was attempted.")
        return 1
    if status != 202:
        print(f"Relay returned HTTP {status}; expected 202.")
        return 1
    print("Gmail accepted the email. Check the inbox and spam folder to confirm delivery.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
