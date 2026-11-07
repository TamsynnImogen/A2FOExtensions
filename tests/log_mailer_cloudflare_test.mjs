import test from 'node:test';
import assert from 'node:assert/strict';
import { Buffer } from 'node:buffer';
import {
  RECIPIENT, MAX_REQUEST, configuration, parseReport, makeMessage,
  sendGmail, MailBudget, handleRequest,
} from '../modules/A2FOLogMailer/cloudflare/worker.mjs';

const token = 'test-token-0123456789-abcdefghijklmnop';
const envBase = { A2FO_RELAY_TOKEN: token, GMAIL_USER: 'sender@gmail.com',
  GMAIL_APP_PASSWORD: 'abcdefghijklmnop' };
const bytes = Buffer.from([0, 1, 2, 46, 13, 10, 255, 128]);
const file = (name, content = bytes) => ({ name, content_base64: Buffer.from(content).toString('base64') });
const report = (files = [file('bugreport.txt')]) => ({ version: 1, exit_code: 0xc0000005, files });
const body = value => JSON.stringify(value);
const goodRequest = (payload = body(report()), headers = {}) => new Request(
  'https://relay.example/v1/crash-reports', { method: 'POST', headers: {
    Authorization: `Bearer ${token}`, 'Content-Type': 'application/json', ...headers,
  }, body: payload, duplex: 'half' });

test('configuration requires bounded token, Gmail sender, and 16-letter app password', () => {
  assert.deepEqual(configuration(envBase), { token, user: 'sender@gmail.com', password: 'abcdefghijklmnop' });
  assert.equal(configuration({ ...envBase, GMAIL_APP_PASSWORD: 'abcd efgh ijkl mnop' }).password,
    'abcdefghijklmnop');
  for (const changed of [
    { A2FO_RELAY_TOKEN: 'short' }, { A2FO_RELAY_TOKEN: 'x'.repeat(2041) },
    { A2FO_RELAY_TOKEN: `${token}\n` }, { GMAIL_USER: 'sender@example.com' },
    { GMAIL_USER: 'sender@gmail.com\r\nBcc: attacker@example.com' },
    { GMAIL_APP_PASSWORD: 'invalid' }, { GMAIL_APP_PASSWORD: undefined },
  ]) assert.throws(() => configuration({ ...envBase, ...changed }));
});

test('version 1 Python relay payload and exact field contract', () => {
  assert.deepEqual(parseReport(body(report())), report());
  for (const bad of [
    { ...report(), version: 2 }, { ...report(), version: true },
    { ...report(), exit_code: -1 }, { ...report(), exit_code: 0x100000000 },
    { ...report(), exit_code: true }, { ...report(), files: [] },
    { ...report(), recipient: 'attacker@example.com' },
    { ...report(), files: [{ ...file('bugreport.txt'), recipient: 'attacker@example.com' }] },
    { ...report(), files: [file('a.log'), file('A.LOG')] },
  ]) assert.throws(() => parseReport(body(bad)));
});

test('version 1 duplicate JSON fields are rejected as in the Python relay', () => {
  for (const raw of [
    '{"version":1,"version":1,"exit_code":0,"files":[{"name":"x.log","content_base64":"AA=="}]}',
    '{"version":1,"\\u0076ersion":1,"exit_code":0,"files":[{"name":"x.log","content_base64":"AA=="}]}',
    '{"version":1,"exit_code":0,"files":[{"name":"x.log","name":"y.log","content_base64":"AA=="}]}',
  ]) assert.throws(() => parseReport(raw));
});

test('JSON number spelling matches Python integer parsing', () => {
  const raw = (version, exitCode) => `{"version":${version},"exit_code":${exitCode},"files":[{"name":"x.log","content_base64":"AA=="}]}`;
  for (const value of ['1.0', '1e0']) {
    assert.throws(() => parseReport(raw(value, '0')));
    assert.throws(() => parseReport(raw('1', value)));
  }
  assert.doesNotThrow(() => parseReport(raw('1', '-0')));
});

test('file count, per-file, total decoded, names, and base64 limits', () => {
  assert.equal(parseReport(body(report(Array.from({ length: 32 }, (_, i) =>
    file(`${i}.log`, Buffer.alloc(32768)))))).files.length, 32);
  assert.throws(() => parseReport(body(report(Array.from({ length: 33 }, (_, i) => file(`${i}.log`))))));
  assert.doesNotThrow(() => parseReport(body(report([file('max.log', Buffer.alloc(65536))]))));
  assert.throws(() => parseReport(body(report([file('over.log', Buffer.alloc(65537))]))));
  assert.throws(() => parseReport(body(report(Array.from({ length: 17 }, (_, i) =>
    file(`${i}.log`, Buffer.alloc(65536)))))));
  for (const name of ['x\r\nBcc: evil.log', '../bad.log', 'bad".log', '.log', 'x.txt',
    `${'a'.repeat(252)}.log`]) assert.throws(() => parseReport(body(report([file(name)]))));
  for (const encoded of ['!', 'AA', 'A===', 'AA=A', 'AAAA=', 'AA A', '===='])
    assert.throws(() => parseReport(body(report([{ name: 'x.log', content_base64: encoded }]))),
      `invalid base64 ${encoded}`);
  // Python's b64decode(validate=True) accepts nonzero pad bits.
  for (const [encoded, decoded] of [['AB==', 1], ['AAB=', 2]]) {
    const parsed = parseReport(body(report([{ name: 'x.log', content_base64: encoded }])));
    assert.deepEqual(Buffer.from(parsed.files[0].content_base64, 'base64'), Buffer.alloc(decoded));
  }
});

test('message has fixed recipient and lossless base64 attachment', () => {
  const message = makeMessage(configuration(envBase), report());
  assert.match(message, new RegExp(`^To: ${RECIPIENT.replaceAll('.', '\\.')}\\r\\n`, 'm'));
  assert.match(message, /Content-Disposition: attachment; filename="bugreport.txt"/);
  const encoded = message.match(/Content-Transfer-Encoding: base64\r\n\r\n([A-Za-z0-9+/=\r\n]+)/)?.[1];
  assert.deepEqual(Buffer.from(encoded.replaceAll(/\s/g, ''), 'base64'), bytes);
  assert.doesNotMatch(message, /Bcc:/);
});

function budgetBinding(status = 204) {
  const calls = [];
  return { calls, idFromName(name) { calls.push(['id', name]); return name; },
    get(id) { calls.push(['get', id]); return { fetch: async (url, init) => {
      calls.push(['fetch', url, init.method]); return new Response(null, { status });
    } }; } };
}

test('request gate rejects before budget and SMTP; secrets and failures stay out of responses', async () => {
  const budget = budgetBinding();
  let sends = 0;
  const env = { ...envBase, MAIL_BUDGET: budget };
  const send = async () => { sends++; };
  const invalid = [
    [new Request('http://relay.example/v1/crash-reports', { method: 'POST', body: body(report()) }), 400],
    [new Request('https://relay.example/v1/crash-reports', { method: 'POST', body: body(report()) }), 401],
    [goodRequest(body(report()), { Authorization: 'Bearer incorrect' }), 401],
    [goodRequest(body(report()), { 'Content-Type': 'text/plain' }), 400],
    [goodRequest(body(report()), { 'Content-Encoding': 'gzip' }), 400],
    [goodRequest('{'), 400],
    [goodRequest(body({ ...report(), files: [] })), 400],
    [goodRequest('x'.repeat(MAX_REQUEST + 1)), 413],
    [new Request('https://relay.example/v1/crash-reports', { method: 'GET' }), 405],
    [new Request('https://relay.example/v1/crash-reports?x=1', { method: 'POST' }), 404],
  ];
  for (const [request, status] of invalid) {
    const result = await handleRequest(request, env, send);
    assert.equal(result.status, status);
    const reply = await result.text();
    assert.doesNotMatch(reply, /abcdefghijklmnop|test-token|bugreport|smtp/i);
  }
  assert.equal(budget.calls.length, 0);
  assert.equal(sends, 0);
  assert.equal((await handleRequest(goodRequest(), { ...env, GMAIL_APP_PASSWORD: undefined }, send)).status, 503);
  assert.equal(budget.calls.length, 0);
});

test('oversized streamed body rejects before reservation or send', async () => {
  const budget = budgetBinding();
  let sends = 0;
  const stream = new ReadableStream({ start(controller) {
    controller.enqueue(new Uint8Array(MAX_REQUEST)); controller.enqueue(new Uint8Array([1])); controller.close();
  } });
  const request = new Request('https://relay.example/v1/crash-reports', {
    method: 'POST', headers: { Authorization: `Bearer ${token}`, 'Content-Type': 'application/json' },
    body: stream, duplex: 'half',
  });
  assert.equal((await handleRequest(request, { ...envBase, MAIL_BUDGET: budget }, async () => sends++)).status, 413);
  assert.equal(budget.calls.length, 0);
  assert.equal(sends, 0);
});

test('declared body length rejects oversize and incomplete bodies before reservation', async () => {
  const budget = budgetBinding();
  let sends = 0;
  for (const [length, status] of [[String(MAX_REQUEST + 1), 413], ['1', 400], ['abc', 413]]) {
    const request = goodRequest(body(report()), { 'Content-Length': length });
    assert.equal((await handleRequest(request, { ...envBase, MAIL_BUDGET: budget },
      async () => sends++)).status, status);
  }
  assert.equal(budget.calls.length, 0);
  assert.equal(sends, 0);
});

test('budget failures fail closed, delivery is awaited, and 202 follows acceptance', async () => {
  assert.equal((await handleRequest(goodRequest(), envBase, async () => {})).status, 503);
  for (const status of [429, 503]) {
    let sends = 0;
    assert.equal((await handleRequest(goodRequest(), { ...envBase, MAIL_BUDGET: budgetBinding(status) },
      async () => sends++)).status, status);
    assert.equal(sends, 0);
  }
  let accept;
  const pending = new Promise(resolve => { accept = resolve; });
  let settled = false;
  const response = handleRequest(goodRequest(), { ...envBase, MAIL_BUDGET: budgetBinding() },
    async () => { await pending; }).then(value => { settled = true; return value; });
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(settled, false);
  accept();
  assert.equal((await response).status, 202);
  const failed = await handleRequest(goodRequest(), { ...envBase, MAIL_BUDGET: budgetBinding() },
    async () => { throw new Error(`SMTP provider ${token} ${envBase.GMAIL_APP_PASSWORD}`); });
  assert.equal(failed.status, 503);
  assert.deepEqual(await failed.json(), { status: 'error' });
});

function fakeStorage() {
  let value;
  let queue = Promise.resolve();
  const writes = [];
  return { writes, transaction(fn) {
    const result = queue.then(() => fn({ get: async key => key === 'budget' ? value : undefined,
      put: async (key, next) => { assert.equal(key, 'budget'); value = next; writes.push({ ...next }); },
    }));
    queue = result.catch(() => {});
    return result;
  } };
}

test('transactional budget limits concurrent attempts, caps day and rolls over', async () => {
  const originalNow = Date.now;
  try {
    let now = Date.parse('2026-10-21T12:00:00Z');
    Date.now = () => now;
    const storage = fakeStorage();
    const budget = new MailBudget({ storage }, {});
    const reserve = () => budget.fetch(new Request('https://budget.internal/reserve', { method: 'POST' }));
    const responses = await Promise.all(Array.from({ length: 20 }, reserve));
    assert.equal(responses.filter(x => x.status === 204).length, 5);
    assert.equal(storage.writes.length, 5);
    now += 60000;
    const next = await Promise.all(Array.from({ length: 100 }, reserve));
    assert.equal(next.filter(x => x.status === 204).length, 5);
    assert.equal(storage.writes.at(-1).daily, 10);
    now += 86400000;
    assert.equal((await reserve()).status, 204);
    assert.equal(storage.writes.at(-1).daily, 1);
    assert.deepEqual(Object.keys(storage.writes.at(-1)).sort(), ['daily', 'day', 'hits']);
    const capped = new MailBudget({ storage: fakeStorage() }, { MAX_REPORTS_PER_DAY: '1' });
    assert.equal((await capped.fetch(new Request('https://budget.internal/reserve', { method: 'POST' }))).status, 204);
    assert.equal((await capped.fetch(new Request('https://budget.internal/reserve', { method: 'POST' }))).status, 429);
    for (const cap of ['0', '401', '1.5', 'invalid']) {
      assert.equal((await new MailBudget({ storage: fakeStorage() }, { MAX_REPORTS_PER_DAY: cap })
        .fetch(new Request('https://budget.internal/reserve', { method: 'POST' }))).status, 503);
    }
  } finally { Date.now = originalNow; }
});

test('rolling 60-second budget does not reset at a wall-clock minute boundary', async () => {
  const originalNow = Date.now;
  try {
    let now = Date.parse('2026-10-21T12:00:59Z');
    Date.now = () => now;
    const storage = fakeStorage();
    const budget = new MailBudget({ storage }, {});
    const reserve = () => budget.fetch(new Request('https://budget.internal/reserve', { method: 'POST' }));
    const initial = await Promise.all(Array.from({ length: 5 }, reserve));
    assert.ok(initial.every(result => result.status === 204));
    now += 1000;
    assert.equal((await reserve()).status, 429);
    now += 58999;
    assert.equal((await reserve()).status, 429);
    now += 1;
    assert.equal((await reserve()).status, 204);
    assert.equal(storage.writes.at(-1).daily, 6);
    assert.equal(storage.writes.at(-1).hits.length, 1);
  } finally { Date.now = originalNow; }
});

test('default daily budget is 100 and configured ceiling permits 400', async () => {
  const originalNow = Date.now;
  try {
    let now = Date.parse('2026-10-21T00:00:00Z');
    Date.now = () => now;
    for (const [config, limit] of [[{}, 100], [{ MAX_REPORTS_PER_DAY: '400' }, 400]]) {
      const budget = new MailBudget({ storage: fakeStorage() }, config);
      const reserve = () => budget.fetch(new Request('https://budget.internal/reserve', { method: 'POST' }));
      for (let minute = 0; minute < limit / 5; minute++) {
        const results = await Promise.all(Array.from({ length: 5 }, reserve));
        assert.ok(results.every(result => result.status === 204));
        now += 60000;
      }
      assert.equal((await reserve()).status, 429);
      now = Date.parse('2026-10-21T00:00:00Z');
    }
  } finally { Date.now = originalNow; }
});

function smtpServer({ noTls = false, authFailure = false, dataFailure = false,
  stall = false, closeFailure = false } = {}) {
  const events = [];
  let secure = false, mode = 'line', pending = '', data = '';
  let controller;
  const reply = value => {
    // Deliberately split server lines across stream chunks.
    for (let i = 0; i < value.length; i += 3)
      controller.enqueue(new TextEncoder().encode(value.slice(i, i + 3)));
  };
  const socket = tls => {
    let closed = false;
    let closeResolve;
    const closedPromise = new Promise(resolve => { closeResolve = resolve; });
    const readable = new ReadableStream({ start(next) {
      controller = next;
      if (!tls && !stall) queueMicrotask(() => reply('220 smtp.test ready\r\n'));
    } });
    const writable = new WritableStream({ write(chunk) {
      const value = new TextDecoder().decode(chunk);
      if (mode === 'data') {
        data += value;
        if (data.endsWith('\r\n.\r\n')) {
          events.push([secure ? 'tls-data' : 'plain-data', data]);
          mode = 'line';
          if (!stall) reply(dataFailure ? '550 data refused\r\n' : '250 queued\r\n');
        }
        return;
      }
      pending += value;
      while (pending.includes('\r\n')) {
        const end = pending.indexOf('\r\n');
        const command = pending.slice(0, end);
        pending = pending.slice(end + 2);
        events.push([secure ? 'tls' : 'plain', command]);
        if (stall) continue;
        if (command.startsWith('EHLO')) reply(tls
          ? '250-smtp.test\r\n250-AUTH PLAIN LOGIN\r\n250 SIZE 3000000\r\n'
          : noTls ? '250-smtp.test\r\n250 SIZE 3000000\r\n'
            : '250-smtp.test\r\n250-SIZE 3000000\r\n250 STARTTLS\r\n');
        else if (command === 'STARTTLS') reply('220 go ahead\r\n');
        else if (command === 'AUTH LOGIN') reply('334 username\r\n');
        else if (command === btoa(envBase.GMAIL_USER)) reply('334 password\r\n');
        else if (command === btoa(envBase.GMAIL_APP_PASSWORD)) reply(authFailure
          ? '535 authentication failed\r\n' : '235 authenticated\r\n');
        else if (command === 'DATA') { mode = 'data'; reply('354 send message\r\n'); }
        else reply('250 OK\r\n');
      }
    } });
    return { readable, writable, opened: Promise.resolve(), closed: closedPromise,
      startTls() { assert.equal(tls, false); secure = true; return socket(true); },
      async close() { events.push(['close', tls ? 'tls' : 'plain']);
        if (!closed) { closed = true; closeResolve(); }
        if (closeFailure) throw new Error('socket close failed');
      } };
  };
  return { events, connect(address, options) {
    assert.deepEqual(address, { hostname: 'smtp.gmail.com', port: 587 });
    assert.deepEqual(options, { secureTransport: 'starttls' });
    return socket(false);
  } };
}

test('SMTP STARTTLS precedes credentials, fixed RCPT, and DATA preserves binary attachment', async () => {
  const server = smtpServer();
  await sendGmail(configuration(envBase), report(), server.connect);
  const plain = server.events.filter(x => x[0] === 'plain').map(x => x[1]);
  const tls = server.events.filter(x => x[0] === 'tls').map(x => x[1]);
  assert.deepEqual(plain, ['EHLO a2fo-log-relay.workers.dev', 'STARTTLS']);
  assert.deepEqual(tls.slice(0, 7), [
    'EHLO a2fo-log-relay.workers.dev', 'AUTH LOGIN', btoa(envBase.GMAIL_USER),
    btoa(envBase.GMAIL_APP_PASSWORD), `MAIL FROM:<${envBase.GMAIL_USER}>`,
    `RCPT TO:<${RECIPIENT}>`, 'DATA',
  ]);
  const wire = server.events.find(x => x[0] === 'tls-data')?.[1];
  assert.ok(wire?.endsWith('\r\n.\r\n'));
  assert.deepEqual(Buffer.from(wire.match(/Content-Transfer-Encoding: base64\r\n\r\n([A-Za-z0-9+/=\r\n]+)/)[1]
    .replaceAll(/\s/g, ''), 'base64'), bytes);
  assert.equal(server.events.some(x => x[0] === 'plain-data'), false);
});

test('SMTP rejects absent STARTTLS, failed authentication, and failed DATA', async () => {
  for (const [options, lastStage] of [
    [{ noTls: true }, 'plain'], [{ authFailure: true }, 'tls'], [{ dataFailure: true }, 'tls-data'],
  ]) {
    const server = smtpServer(options);
    await assert.rejects(() => sendGmail(configuration(envBase), report(), server.connect, 500));
    assert.equal(server.events.some(x => x[0] === lastStage), true);
    if (options.noTls) assert.equal(server.events.some(x => x[1] === btoa(envBase.GMAIL_APP_PASSWORD)), false);
  }
});

test('accepted DATA survives socket close failure; deadline closes stalled connection', async () => {
  const accepted = smtpServer({ closeFailure: true });
  await sendGmail(configuration(envBase), report(), accepted.connect, 500);
  assert.equal(accepted.events.some(x => x[0] === 'tls-data'), true);
  const stalled = smtpServer({ stall: true });
  await assert.rejects(() => sendGmail(configuration(envBase), report(), stalled.connect, 20),
    /SMTP deadline reached/);
  assert.equal(stalled.events.some(x => x[0] === 'close'), true);
});
