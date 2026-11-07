"""Headless tests for the local crash-report relay; no network mail is sent."""

import base64
import importlib.util
import io
import json
from pathlib import Path
import unittest
from unittest.mock import patch


MODULE = Path(__file__).resolve().parents[1] / "modules/A2FOLogMailer/relay.py"
spec = importlib.util.spec_from_file_location("a2fo_relay", MODULE)
relay = importlib.util.module_from_spec(spec)
import sys
sys.modules[spec.name] = relay
spec.loader.exec_module(relay)


def body(name="bugreport.txt", content=b"crash details", **extra):
    report = {"version": 1, "exit_code": 0xC0000005,
              "files": [{"name": name, "content_base64": base64.b64encode(content).decode()}]}
    report.update(extra)
    return json.dumps(report).encode()


class FakeServer:
    def __init__(self, deliver_report):
        self.config = relay.config_from_env({
            "A2FO_RELAY_TOKEN": "shared-secret",
            "A2FO_SMTP_HOST": "smtp.example.invalid",
            "A2FO_SMTP_FROM": "sender@example.invalid",
            "A2FO_SMTP_TLS": "starttls",
        })
        self.rate_limiter = relay.RateLimiter()
        self.deliver_report = deliver_report


def request(server, payload, path="/v1/crash-reports", token="shared-secret", method="POST", headers=None):
    headers = headers or {}
    lines = [f"{method} {path} HTTP/1.1", "Host: localhost",
             f"Content-Length: {len(payload)}", "Content-Type: application/json"]
    if token is not None:
        lines.append(f"Authorization: Bearer {token}")
    lines.extend(f"{key}: {value}" for key, value in headers.items())
    raw = ("\r\n".join(lines) + "\r\n\r\n").encode("latin-1") + payload

    class RetainedBytesIO(io.BytesIO):
        def close(self):
            pass

    class FakeSocket:
        def __init__(self):
            self.input = io.BytesIO(raw)
            self.output = RetainedBytesIO()
        def makefile(self, mode, *args, **kwargs):
            return self.input if "r" in mode else self.output
        def settimeout(self, seconds):
            pass
        def sendall(self, data):
            self.output.write(data)
    fake_socket = FakeSocket()
    relay.RelayHandler(fake_socket, ("127.0.0.1", 12345), server)
    return fake_socket.output.getvalue()


def status(response):
    return int(response.split(b"\r\n", 1)[0].split()[1])


class RelayTests(unittest.TestCase):
    def test_valid_delivery_and_fixed_recipient(self):
        calls = []
        server = FakeServer(lambda config, code, files: calls.append((config, code, files)))
        self.assertEqual(status(request(server, body(name="A2FOExtensions.log"))), 202)
        self.assertEqual(calls[0][1:], (0xC0000005, [("A2FOExtensions.log", b"crash details")]))

        smtp_messages = []
        smtp_events = []
        class FakeSMTP:
            def __init__(self, *args, **kwargs):
                pass
            def __enter__(self):
                return self
            def __exit__(self, *args):
                pass
            def ehlo(self):
                smtp_events.append("ehlo")
            def starttls(self, **kwargs):
                smtp_events.append("starttls")
            def login(self, *args):
                smtp_events.append("login")
            def send_message(self, message):
                smtp_events.append("send_message")
                smtp_messages.append(message)
        authenticated = relay.config_from_env({
            "A2FO_RELAY_TOKEN": "shared-secret", "A2FO_SMTP_HOST": "smtp.example.invalid",
            "A2FO_SMTP_FROM": "sender@example.invalid", "A2FO_SMTP_TLS": "starttls",
            "A2FO_SMTP_USER": "user", "A2FO_SMTP_PASSWORD": "dummy-password",
        })
        with patch.object(relay.smtplib, "SMTP", FakeSMTP):
            relay.deliver(authenticated, calls[0][1], calls[0][2])
        self.assertEqual(smtp_events, ["ehlo", "starttls", "ehlo", "login", "send_message"])
        self.assertEqual(smtp_messages[0]["To"], relay.RECIPIENT)
        self.assertEqual(smtp_messages[0].iter_attachments().__next__().get_content(), b"crash details")

    def test_auth_route_and_methods(self):
        calls = []
        server = FakeServer(lambda *args: calls.append(args))
        self.assertEqual(status(request(server, body(), token=None)), 401)
        self.assertEqual(status(request(server, body(), token="incorrect")), 401)
        self.assertEqual(status(request(server, body(), token="é")), 401)
        self.assertEqual(status(request(server, body(), path="/other")), 404)
        self.assertEqual(status(request(server, b"", method="GET")), 405)
        self.assertEqual(calls, [])

    def test_rejected_reports_never_deliver(self):
        calls = []
        server = FakeServer(lambda *args: calls.append(args))
        bad = [body(name="../bugreport.txt"), body(name="A2FOevil.log/other"),
               body(name="A2FOé.log"), body(content=b"x" * (relay.MAX_FILE + 1)),
               body(exit_code=-1), body(exit_code=True), body(version=2),
               body(files=[]), body(recipient="attacker@example.invalid"),
               b'{"version":1,"exit_code":0,"files":[{"name":"bugreport.txt","content_base64":"!"}]}',
               b"not json"]
        for payload in bad:
            with self.subTest(payload=payload[:70]):
                server.rate_limiter = relay.RateLimiter()
                self.assertEqual(status(request(server, payload)), 400)
        self.assertEqual(calls, [])

    def test_duplicate_and_count_limits(self):
        file = {"name": "bugreport.txt", "content_base64": ""}
        for files in ([file, file], [file] * (relay.MAX_FILES + 1)):
            with self.assertRaises(relay.BadReport):
                relay.parse_report(json.dumps({"version": 1, "exit_code": 0, "files": files}).encode())
        with self.assertRaises(relay.BadReport):
            relay.parse_report(b" " * (relay.MAX_REQUEST + 1))
        with self.assertRaises(relay.BadReport):
            relay.parse_report(b'{"version":1,"version":1,"exit_code":0,"files":[]}')
        with self.assertRaises(relay.BadReport):
            relay.parse_report(b"[" * 2000 + b"0" + b"]" * 2000)

    def test_http_request_limit_and_duplicate_length(self):
        server = FakeServer(lambda *args: self.fail("unexpected delivery"))
        self.assertEqual(status(request(server, b" " * (relay.MAX_REQUEST + 1))), 413)
        self.assertEqual(status(request(server, body(), headers={"Content-Length": "1"})), 400)

    def test_smtp_failure_is_sanitized(self):
        def fail(*args):
            raise RuntimeError("private smtp password and report contents")
        response = request(FakeServer(fail), body())
        self.assertEqual(status(response), 503)
        self.assertNotIn(b"private", response)
        self.assertNotIn(b"contents", response)

    def test_config_requires_tls_and_paired_credentials(self):
        env = {"A2FO_RELAY_TOKEN": "x", "A2FO_SMTP_HOST": "host",
               "A2FO_SMTP_FROM": "from@example.invalid", "A2FO_SMTP_TLS": "ssl"}
        self.assertEqual(relay.config_from_env(env).bind, "127.0.0.1")
        self.assertEqual(relay.config_from_env(env).smtp_port, 465)
        for change in ({"A2FO_SMTP_TLS": "none"}, {"A2FO_SMTP_USER": "x"},
                       {"A2FO_RELAY_TOKEN": ""}, {"A2FO_RELAY_TOKEN": "bad token"},
                       {"A2FO_RELAY_TOKEN": "bad\nvalue"}, {"A2FO_RELAY_TOKEN": "é"}):
            with self.subTest(change=change), self.assertRaises(ValueError):
                relay.config_from_env(env | change)

    def test_ssl_smtp_mode(self):
        env = {"A2FO_RELAY_TOKEN": "x", "A2FO_SMTP_HOST": "host",
               "A2FO_SMTP_FROM": "from@example.invalid", "A2FO_SMTP_TLS": "ssl"}
        calls = []
        class FakeSMTP:
            def __init__(self, *args, **kwargs):
                calls.append((args, kwargs))
            def __enter__(self):
                return self
            def __exit__(self, *args):
                pass
            def send_message(self, message):
                calls.append(message["To"])
        with patch.object(relay.smtplib, "SMTP_SSL", FakeSMTP):
            relay.deliver(relay.config_from_env(env), 0, [("bugreport.txt", b"x")])
        self.assertEqual(calls[1], relay.RECIPIENT)

    def test_rate_limit(self):
        clock = [0.0]
        limiter = relay.RateLimiter(lambda: clock[0])
        self.assertEqual([limiter.allow("ip") for _ in range(6)], [True] * 5 + [False])
        clock[0] = 61.0
        self.assertTrue(limiter.allow("ip"))


if __name__ == "__main__":
    unittest.main()
