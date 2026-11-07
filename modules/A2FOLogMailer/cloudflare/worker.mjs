// Separate Cloudflare receiver for the existing native module's JSON protocol.
// No mailbox credentials or account identifiers belong in this source file.
export const RECIPIENT = "aclaydon1@googlemail.com";
export const MAX_REQUEST = 3 * 1024 * 1024;
const MAX_FILE = 64 * 1024;
const MAX_TOTAL = 1024 * 1024;
const encoder = new TextEncoder();

export function configuration(env) {
  const token = env.A2FO_RELAY_TOKEN;
  const user = env.GMAIL_USER;
  const password = typeof env.GMAIL_APP_PASSWORD === "string"
    ? env.GMAIL_APP_PASSWORD.replace(/\s/g, "") : "";
  if (typeof token !== "string" || token.length < 32 || token.length > 2040 ||
      /[^\x21-\x7e]/.test(token) || typeof user !== "string" ||
      /\s/.test(user) || !/^[A-Za-z0-9._%+-]{1,64}@(gmail|googlemail)\.com$/i.test(user) ||
      !/^[A-Za-z]{16}$/.test(password)) throw new Error("Missing or invalid settings");
  return { token, user, password };
}

// JSON.parse discards repeated fields and the distinction between 1 and 1.0.
// Check field uniqueness and integer tokens before that information is lost.
// indexOf skips large base64 strings without walking every attachment byte in JS.
function checkJsonFields(text) {
  if (typeof text !== "string" || text.length > MAX_REQUEST) throw new Error("Invalid report");
  const stack = [];
  for (let i = 0; i < text.length;) {
    const c = text[i];
    const top = stack.at(-1);
    if (c === '"') {
      let end = text.indexOf('"', i + 1);
      for (;;) {
        if (end === -1) throw new Error("Invalid JSON string");
        let slashes = 0;
        for (let j = end - 1; j >= i && text[j] === "\\"; --j) ++slashes;
        if (slashes % 2 === 0) break;
        end = text.indexOf('"', end + 1);
      }
      if (top?.keys && top.expectKey) {
        const key = JSON.parse(text.slice(i, end + 1));
        if (top.keys.has(key)) throw new Error("Duplicate JSON field");
        top.keys.add(key);
        top.key = key;
        top.expectKey = false;
      }
      i = end + 1;
    } else if (c === "{" || c === "[") {
      stack.push(c === "{" ? { keys: new Set(), expectKey: true } : {});
      if (stack.length > 8) throw new Error("JSON nesting too deep");
      ++i;
    } else if (c === "}" || c === "]") { stack.pop(); ++i; }
    else if (c === ",") { if (top?.keys) { top.expectKey = true; top.key = undefined; } ++i; }
    else if (c === ":" || /\s/.test(c)) { ++i; }
    else {
      let end = i + 1;
      while (end < text.length && !/[\s,}\]]/.test(text[end])) ++end;
      if ((top?.key === "version" || top?.key === "exit_code") &&
          !/^-?(0|[1-9][0-9]*)$/.test(text.slice(i, end)))
        throw new Error("Expected integer");
      i = end;
    }
  }
}

export function parseReport(text) {
  checkJsonFields(text);
  const report = JSON.parse(text);
  const keysAre = (value, names) => value !== null && typeof value === "object" &&
    !Array.isArray(value) && Object.keys(value).length === names.length &&
    names.every(name => Object.hasOwn(value, name));
  if (!keysAre(report, ["version", "exit_code", "files"]) || report.version !== 1 ||
      !Number.isInteger(report.exit_code) || report.exit_code < 0 ||
      report.exit_code > 0xffffffff || !Array.isArray(report.files) ||
      report.files.length < 1 || report.files.length > 32) throw new Error("Invalid report");
  const seen = new Set();
  let total = 0;
  for (const file of report.files) {
    if (!keysAre(file, ["name", "content_base64"]) || typeof file.name !== "string" ||
        file.name.length > 255 || /[^A-Za-z0-9_.-]/.test(file.name) ||
        !/^(?:bugreport\.txt|[A-Za-z0-9][A-Za-z0-9_.-]*\.log)$/i.test(file.name) ||
        seen.has(file.name.toLowerCase()) || typeof file.content_base64 !== "string")
      throw new Error("Invalid attachment");
    const encoded = file.content_base64;
    const padding = encoded.endsWith("==") ? 2 : encoded.endsWith("=") ? 1 : 0;
    const firstPad = encoded.indexOf("=");
    if (encoded.length > Math.ceil(MAX_FILE / 3) * 4 || encoded.length % 4 !== 0 ||
        /[^A-Za-z0-9+/=]/.test(encoded) ||
        (firstPad !== -1 && firstPad !== encoded.length - padding))
      throw new Error("Invalid attachment encoding");
    const bytes = encoded.length / 4 * 3 - padding;
    if (bytes < 0 || bytes > MAX_FILE || (total += bytes) > MAX_TOTAL)
      throw new Error("Attachments too large");
    seen.add(file.name.toLowerCase());
  }
  return report;
}

export function makeMessage(config, report) {
  const id = crypto.randomUUID();
  const boundary = "a2fo_" + id;
  const lines = [
    `From: ${config.user}`, `To: ${RECIPIENT}`,
    `Subject: Fleet Ops diagnostic logs (exit code ${report.exit_code})`,
    `Date: ${new Date().toUTCString()}`, `Message-ID: <${id}@${config.user.split("@")[1]}>`,
    "MIME-Version: 1.0", `Content-Type: multipart/mixed; boundary="${boundary}"`, "",
    `--${boundary}`, "Content-Type: text/plain; charset=us-ascii", "",
    `Fleet Ops diagnostic log report. Exit code: ${report.exit_code}.`,
    `Files attached: ${report.files.length}. Large files may be truncated by the game module.`, "",
  ];
  for (const file of report.files) {
    lines.push(`--${boundary}`, "Content-Type: application/octet-stream",
      `Content-Disposition: attachment; filename="${file.name}"`,
      "Content-Transfer-Encoding: base64", "",
      file.content_base64.replace(/.{76}/g, "$&\r\n"), "");
  }
  lines.push(`--${boundary}--`, "");
  return lines.join("\r\n");
}

// SMTP over verified TLS, with no plaintext fallback and no provider text in errors.
class SmtpSession {
  constructor(socket) {
    this.reader = socket.readable.getReader();
    this.writer = socket.writable.getWriter();
    this.decoder = new TextDecoder();
    this.buffer = "";
  }
  release() {
    try { this.reader.releaseLock(); } catch { /* pending read is closed by socket */ }
    try { this.writer.releaseLock(); } catch { /* pending write is closed by socket */ }
  }
  async line() {
    while (!this.buffer.includes("\r\n")) {
      const chunk = await this.reader.read();
      if (chunk.done) throw new Error("SMTP connection ended");
      this.buffer += this.decoder.decode(chunk.value, { stream: true });
      if (this.buffer.length > 16384) throw new Error("SMTP reply too large");
    }
    const end = this.buffer.indexOf("\r\n");
    if (end > 2048) throw new Error("SMTP reply too large");
    const line = this.buffer.slice(0, end);
    this.buffer = this.buffer.slice(end + 2);
    return line;
  }
  async reply(expected) {
    const lines = [];
    for (let count = 0; count < 64; ++count) {
      const match = /^([2-5][0-9]{2})([ -])(.*)$/.exec(await this.line());
      if (!match || Number(match[1]) !== expected) throw new Error("SMTP request rejected");
      lines.push(match[3]);
      if (match[2] === " ") return lines;
    }
    throw new Error("SMTP reply too large");
  }
  async command(command, expected) {
    await this.writer.write(encoder.encode(command + "\r\n"));
    return this.reply(expected);
  }
}

export async function sendGmail(config, report, connectOverride, timeoutMs = 8000) {
  const connect = connectOverride ?? (await import("cloudflare:sockets")).connect;
  let socket, session, timer;
  const operation = (async () => {
    socket = connect({ hostname: "smtp.gmail.com", port: 587 }, { secureTransport: "starttls" });
    socket.closed.catch(() => {});
    await socket.opened;
    session = new SmtpSession(socket);
    await session.reply(220);
    const capabilities = await session.command("EHLO a2fo-log-relay.workers.dev", 250);
    if (!capabilities.some(line => /^STARTTLS(?:\s|$)/i.test(line)))
      throw new Error("SMTP requires TLS");
    await session.command("STARTTLS", 220);
    session.release();
    socket = socket.startTls();
    socket.closed.catch(() => {});
    await socket.opened;
    session = new SmtpSession(socket);
    const secureCapabilities = await session.command("EHLO a2fo-log-relay.workers.dev", 250);
    if (!secureCapabilities.some(line => /^AUTH[ =].*\bLOGIN\b/i.test(line)))
      throw new Error("SMTP authentication unavailable");
    await session.command("AUTH LOGIN", 334);
    await session.command(btoa(config.user), 334);
    await session.command(btoa(config.password), 235);
    await session.command(`MAIL FROM:<${config.user}>`, 250);
    await session.command(`RCPT TO:<${RECIPIENT}>`, 250);
    await session.command("DATA", 354);
    const message = makeMessage(config, report).replace(/(^|\r\n)\./g, "$1..");
    await session.writer.write(encoder.encode(message + ".\r\n"));
    await session.reply(250); // SMTP acceptance, not proof of inbox delivery.
    // Once DATA is accepted, a failed QUIT must not turn acceptance into a retry.
  })();
  const deadline = new Promise((_, reject) => {
    timer = setTimeout(() => { reject(new Error("SMTP deadline reached")); }, timeoutMs);
  });
  try { await Promise.race([operation, deadline]); }
  finally {
    clearTimeout(timer);
    session?.release();
    try { await socket?.close(); } catch { /* Never include provider/credential text. */ }
  }
}

// One named object is shared by every Worker instance/location. No reports or
// credentials are persisted; failed SMTP attempts also consume the allowance.
export class MailBudget {
  constructor(state, env) { this.storage = state.storage; this.env = env; }
  async fetch(request) {
    if (request.method !== "POST" || new URL(request.url).pathname !== "/reserve")
      return new Response(null, { status: 404 });
    const configured = Number(this.env.MAX_REPORTS_PER_DAY ?? "100");
    if (!Number.isInteger(configured) || configured < 1 || configured > 400)
      return new Response(null, { status: 503 });
    const now = Date.now();
    const day = new Date(now).toISOString().slice(0, 10);
    const allowed = await this.storage.transaction(async tx => {
      const previous = await tx.get("budget") ?? {};
      const daily = previous.day === day ? previous.daily : 0;
      const hits = (previous.hits ?? []).filter(time => time > now - 60000);
      if (daily >= configured || hits.length >= 5) return false;
      await tx.put("budget", { day, daily: daily + 1, hits: [...hits, now] });
      return true;
    });
    return new Response(null, { status: allowed ? 204 : 429 });
  }
}

function response(status, data = { status: status === 202 ? "accepted" : "error" }) {
  return new Response(JSON.stringify(data), {
    status, headers: { "Content-Type": "application/json", "Cache-Control": "no-store" },
  });
}

async function authorized(header, token) {
  if (typeof header !== "string" || header.length !== token.length + 7) return false;
  const hashes = await Promise.all([header, "Bearer " + token].map(value =>
    crypto.subtle.digest("SHA-256", encoder.encode(value))));
  const a = new Uint8Array(hashes[0]), b = new Uint8Array(hashes[1]);
  let difference = 0;
  for (let i = 0; i < a.length; ++i) difference |= a[i] ^ b[i];
  return difference === 0;
}

async function readBody(request) {
  const declared = request.headers.get("Content-Length");
  if (declared !== null && (!/^[0-9]+$/.test(declared) || Number(declared) > MAX_REQUEST))
    throw new RangeError("Invalid request size");
  if (!request.body) throw new Error("Empty report");
  const reader = request.body.getReader();
  let timer, size = 0;
  const chunks = [];
  try {
    const deadline = new Promise((_, reject) => {
      timer = setTimeout(() => reject(new Error("Request deadline reached")), 5000);
    });
    for (;;) {
      const { done, value } = await Promise.race([reader.read(), deadline]);
      if (done) break;
      size += value.byteLength;
      if (size > MAX_REQUEST) throw new RangeError("Request too large");
      chunks.push(value);
    }
    if (declared !== null && Number(declared) !== size) throw new Error("Incomplete report");
    const bytes = new Uint8Array(size);
    let offset = 0;
    for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.byteLength; }
    return new TextDecoder("utf-8", { fatal: true }).decode(bytes);
  } finally {
    clearTimeout(timer);
    reader.cancel().catch(() => {});
    reader.releaseLock();
  }
}

export async function handleRequest(request, env, send = sendGmail) {
  const url = new URL(request.url);
  if (request.method === "GET" && url.pathname === "/health") {
    let configured = false;
    try { configuration(env); configured = Boolean(env.MAIL_BUDGET); } catch { /* not ready */ }
    return response(200, { service: "A2FOLogMailer", configured });
  }
  if (url.pathname !== "/v1/crash-reports" || url.search) return response(404);
  if (request.method !== "POST") return response(405);
  if (url.protocol !== "https:") return response(400);
  let config;
  try { config = configuration(env); } catch { return response(503); }
  if (!await authorized(request.headers.get("Authorization"), config.token)) return response(401);
  if (request.headers.get("Content-Type") !== "application/json" ||
      request.headers.has("Content-Encoding")) return response(400);
  let report;
  try { report = parseReport(await readBody(request)); }
  catch (error) { return response(error instanceof RangeError ? 413 : 400); }
  try {
    if (!env.MAIL_BUDGET) return response(503);
    const id = env.MAIL_BUDGET.idFromName("global");
    const reserve = await env.MAIL_BUDGET.get(id).fetch("https://budget.internal/reserve", { method: "POST" });
    if (reserve.status !== 204) return response(reserve.status === 429 ? 429 : 503);
    await send(config, report);
    return response(202);
  } catch { return response(503); } // Never log/echo attachments, tokens or SMTP errors.
}

export default { fetch: (request, env) => handleRequest(request, env) };
