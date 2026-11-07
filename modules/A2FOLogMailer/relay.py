"""Local HTTP intake for crash reports, intended to sit behind an HTTPS proxy.

The proxy must terminate HTTPS and restrict access as appropriate. This process
listens on loopback by default and never accepts a recipient from a client.
"""

import argparse
import base64
import binascii
from collections import OrderedDict, deque
from dataclasses import dataclass
from email.message import EmailMessage
from http.server import BaseHTTPRequestHandler, HTTPServer
import hmac
import json
import os
import re
import smtplib
import ssl
import time


RECIPIENT = "aclaydon1@googlemail.com"
MAX_REQUEST = 3 * 1024 * 1024
MAX_FILES = 32
MAX_FILE = 64 * 1024
MAX_DECODED = 1024 * 1024
FILE_NAME = re.compile(r"(?:bugreport\.txt|[A-Za-z0-9][A-Za-z0-9_.-]*\.log)\Z", re.IGNORECASE | re.ASCII)
RATE_WINDOW = 60.0
RATE_LIMIT = 5
MAX_RATE_CLIENTS = 4096


class BadReport(ValueError):
    pass


@dataclass(frozen=True)
class Config:
    token: str
    smtp_host: str
    smtp_port: int
    smtp_mode: str
    sender: str
    smtp_user: str | None
    smtp_password: str | None
    bind: str
    port: int


def config_from_env(env=os.environ):
    """Read and validate configuration without opening a socket or sending mail."""
    token = env.get("A2FO_RELAY_TOKEN", "")
    host = env.get("A2FO_SMTP_HOST", "")
    sender = env.get("A2FO_SMTP_FROM", "")
    mode = env.get("A2FO_SMTP_TLS", "").lower()
    user = env.get("A2FO_SMTP_USER") or None
    password = env.get("A2FO_SMTP_PASSWORD") or None
    if not token or not host or not sender:
        raise ValueError("A2FO_RELAY_TOKEN, A2FO_SMTP_HOST and A2FO_SMTP_FROM are required")
    if not all(0x21 <= ord(character) <= 0x7E for character in token):
        raise ValueError("A2FO_RELAY_TOKEN must contain visible ASCII characters")
    if "\r" in sender or "\n" in sender:
        raise ValueError("invalid SMTP sender")
    if mode not in ("starttls", "ssl"):
        raise ValueError("A2FO_SMTP_TLS must be starttls or ssl")
    if bool(user) != bool(password):
        raise ValueError("SMTP username and password must be supplied together")
    try:
        smtp_port = int(env.get("A2FO_SMTP_PORT", "465" if mode == "ssl" else "587"))
        port = int(env.get("A2FO_RELAY_PORT", "8080"))
    except ValueError as exc:
        raise ValueError("invalid port") from exc
    if not (1 <= smtp_port <= 65535 and 1 <= port <= 65535):
        raise ValueError("port out of range")
    return Config(token, host, smtp_port, mode, sender, user, password,
                  env.get("A2FO_RELAY_BIND", "127.0.0.1"), port)


def parse_report(raw):
    """Return (exit_code, [(safe_name, bytes), ...]) for a bounded JSON body."""
    if len(raw) > MAX_REQUEST:
        raise BadReport("request too large")
    def unique_fields(pairs):
        fields = {}
        for key, value in pairs:
            if key in fields:
                raise BadReport("duplicate JSON field")
            fields[key] = value
        return fields
    try:
        report = json.loads(raw.decode("utf-8"), object_pairs_hook=unique_fields)
    except (UnicodeError, json.JSONDecodeError, RecursionError) as exc:
        raise BadReport("invalid JSON") from exc
    if not isinstance(report, dict) or set(report) != {"version", "exit_code", "files"}:
        raise BadReport("invalid report fields")
    if type(report["version"]) is not int or report["version"] != 1:
        raise BadReport("unsupported version")
    exit_code = report["exit_code"]
    if type(exit_code) is not int or not (0 <= exit_code <= 0xFFFFFFFF):
        raise BadReport("invalid exit code")
    files = report["files"]
    if not isinstance(files, list) or not (1 <= len(files) <= MAX_FILES):
        raise BadReport("invalid file count")
    decoded_files = []
    seen = set()
    total = 0
    for entry in files:
        if not isinstance(entry, dict) or set(entry) != {"name", "content_base64"}:
            raise BadReport("invalid file fields")
        name = entry["name"]
        encoded = entry["content_base64"]
        if not isinstance(name, str) or len(name) > 255 or not FILE_NAME.fullmatch(name):
            raise BadReport("invalid filename")
        if name.lower() in seen:
            raise BadReport("duplicate filename")
        if not isinstance(encoded, str) or len(encoded) > ((MAX_FILE + 2) // 3) * 4:
            raise BadReport("invalid file encoding")
        try:
            content = base64.b64decode(encoded, validate=True)
        except (ValueError, binascii.Error) as exc:
            raise BadReport("invalid file encoding") from exc
        if len(content) > MAX_FILE:
            raise BadReport("file too large")
        total += len(content)
        if total > MAX_DECODED:
            raise BadReport("decoded files too large")
        seen.add(name.lower())
        decoded_files.append((name, content))
    return exit_code, decoded_files


def deliver(config, exit_code, files):
    message = EmailMessage()
    message["From"] = config.sender
    message["To"] = RECIPIENT
    message["Subject"] = f"Fleet Ops diagnostic logs (exit code {exit_code})"
    message.set_content(f"Fleet Ops diagnostic log report. Exit code: {exit_code}. Files attached: {len(files)}.\n")
    for name, content in files:
        message.add_attachment(content, maintype="application", subtype="octet-stream", filename=name)
    context = ssl.create_default_context()
    if config.smtp_mode == "ssl":
        with smtplib.SMTP_SSL(config.smtp_host, config.smtp_port, timeout=10, context=context) as smtp:
            if config.smtp_user:
                smtp.login(config.smtp_user, config.smtp_password)
            smtp.send_message(message)
    else:
        with smtplib.SMTP(config.smtp_host, config.smtp_port, timeout=10) as smtp:
            smtp.ehlo()
            smtp.starttls(context=context)
            smtp.ehlo()
            if config.smtp_user:
                smtp.login(config.smtp_user, config.smtp_password)
            smtp.send_message(message)


class RateLimiter:
    def __init__(self, clock=time.monotonic):
        self.clock = clock
        self.clients = OrderedDict()

    def allow(self, ip):
        now = self.clock()
        hits = self.clients.pop(ip, deque())
        while hits and hits[0] <= now - RATE_WINDOW:
            hits.popleft()
        if len(hits) >= RATE_LIMIT:
            self.clients[ip] = hits
            return False
        hits.append(now)
        self.clients[ip] = hits
        while len(self.clients) > MAX_RATE_CLIENTS:
            self.clients.popitem(last=False)
        return True


class RelayServer(HTTPServer):
    def __init__(self, address, config, deliver_report=deliver):
        super().__init__(address, RelayHandler)
        self.config = config
        self.deliver_report = deliver_report
        self.rate_limiter = RateLimiter()


class RelayHandler(BaseHTTPRequestHandler):
    def setup(self):
        super().setup()
        self.connection.settimeout(10)

    def log_message(self, format, *args):
        # Do not log paths, headers, payloads, SMTP errors, or bearer tokens.
        pass

    def _respond(self, status):
        body = b'{"status":"accepted"}' if status == 202 else b'{"status":"error"}'
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)
        self.close_connection = True

    def do_POST(self):
        if self.path != "/v1/crash-reports":
            self._respond(404)
            return
        if not self.server.rate_limiter.allow(self.client_address[0]):
            self._respond(429)
            return
        authorization = self.headers.get("Authorization", "")
        try:
            supplied = authorization.encode("ascii")
        except UnicodeEncodeError:
            self._respond(401)
            return
        expected = ("Bearer " + self.server.config.token).encode("ascii")
        if not hmac.compare_digest(supplied, expected):
            self._respond(401)
            return
        if self.headers.get("Transfer-Encoding") or self.headers.get("Content-Type") != "application/json":
            self._respond(400)
            return
        lengths = self.headers.get_all("Content-Length", [])
        if len(lengths) != 1:
            self._respond(400)
            return
        length_text = lengths[0]
        try:
            length = int(length_text)
        except ValueError:
            self._respond(400)
            return
        if not (0 < length <= MAX_REQUEST):
            self._respond(413 if length > MAX_REQUEST else 400)
            return
        try:
            raw = self.rfile.read(length)
            if len(raw) != length:
                self._respond(400)
                return
            exit_code, files = parse_report(raw)
        except (BadReport, TimeoutError, OSError):
            self._respond(400)
            return
        try:
            self.server.deliver_report(self.server.config, exit_code, files)
        except Exception:
            self._respond(503)
            return
        self._respond(202)

    def do_GET(self):
        self._respond(405)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check-config", action="store_true", help="validate environment without serving")
    args = parser.parse_args()
    try:
        config = config_from_env()
    except ValueError as exc:
        parser.error(str(exc))
    if args.check_config:
        print("relay configuration valid")
        return
    with RelayServer((config.bind, config.port), config) as server:
        server.serve_forever()


if __name__ == "__main__":
    main()
