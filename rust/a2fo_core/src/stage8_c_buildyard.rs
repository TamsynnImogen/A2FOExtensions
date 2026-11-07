//! The frozen BuildYard MODULE XML grammar and portable rule policy.
extern crate alloc;
use alloc::vec::Vec;
use core::ffi::c_void;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Bytes {
    pub data: *const u8,
    pub size: usize,
}
impl Bytes {
    fn from(value: &[u8]) -> Self {
        Self {
            data: value.as_ptr(),
            size: value.len(),
        }
    }
}
#[repr(C)]
pub struct Event {
    pub op: u32,
    pub a: Bytes,
    pub value: i32,
}
pub type Classify = unsafe extern "C" fn(u8, u32) -> bool;
pub type Emit = unsafe extern "C" fn(*mut c_void, *const Event) -> bool;

fn lower(mut bytes: Vec<u8>) -> Vec<u8> {
    for byte in &mut bytes {
        if byte.is_ascii_uppercase() {
            *byte += b'a' - b'A';
        }
    }
    bytes
}
fn trim(bytes: &[u8], class: Classify) -> &[u8] {
    let mut first = 0;
    let mut last = bytes.len();
    while first < last && unsafe { class(bytes[first], 0) } {
        first += 1;
    }
    while last > first && unsafe { class(bytes[last - 1], 0) } {
        last -= 1;
    }
    &bytes[first..last]
}
fn normalize_odf(bytes: &[u8], class: Classify) -> Vec<u8> {
    let mut value = trim(bytes, class);
    if let Some(index) = value.iter().rposition(|&b| b == b'\\' || b == b'/') {
        value = &value[index + 1..];
    }
    let mut result = lower(value.to_vec());
    if !result.is_empty() && !result.ends_with(b".odf") {
        result.extend_from_slice(b".odf");
    }
    result
}
fn whitespace_only(bytes: &[u8], class: Classify) -> bool {
    bytes.iter().all(|&byte| unsafe { class(byte, 0) })
}

struct Node {
    name: Vec<u8>,
    attributes: Vec<(Vec<u8>, Vec<u8>)>,
    children: Vec<Node>,
    text: Vec<u8>,
}
impl Node {
    fn new() -> Self {
        Self {
            name: Vec::new(),
            attributes: Vec::new(),
            children: Vec::new(),
            text: Vec::new(),
        }
    }
    fn attribute(&self, key: &[u8]) -> Option<&[u8]> {
        self.attributes
            .iter()
            .find(|(name, _)| name == key)
            .map(|(_, value)| value.as_slice())
    }
}
struct Reader<'a> {
    input: &'a [u8],
    pos: usize,
    nodes: usize,
    error: &'static [u8],
    class: Classify,
}
impl<'a> Reader<'a> {
    fn new(input: &'a [u8], class: Classify) -> Self {
        Self {
            input,
            pos: 0,
            nodes: 0,
            error: b"",
            class,
        }
    }
    fn fail(&mut self, message: &'static [u8]) -> bool {
        if self.error.is_empty() {
            self.error = message;
        }
        false
    }
    fn starts(&self, token: &[u8]) -> bool {
        self.input[self.pos..].starts_with(token)
    }
    fn spaces(&mut self) {
        while self.pos < self.input.len() && unsafe { (self.class)(self.input[self.pos], 0) } {
            self.pos += 1;
        }
    }
    fn misc(&mut self) -> bool {
        loop {
            self.spaces();
            let (start, end) = if self.starts(b"<!--") {
                (4, b"-->".as_slice())
            } else if self.starts(b"<?") {
                (2, b"?>".as_slice())
            } else {
                return true;
            };
            let tail = &self.input[self.pos + start..];
            let Some(index) = tail.windows(end.len()).position(|part| part == end) else {
                return self.fail(if start == 4 {
                    b"unterminated XML comment"
                } else {
                    b"unterminated XML declaration"
                });
            };
            self.pos += start + index + end.len();
        }
    }
    fn name_char(&self, byte: u8) -> bool {
        (unsafe { (self.class)(byte, 2) }) || matches!(byte, b'_' | b'-' | b':' | b'.')
    }
    fn name(&mut self, out: &mut Vec<u8>) -> bool {
        let begin = self.pos;
        if self.pos >= self.input.len()
            || !(unsafe { (self.class)(self.input[self.pos], 1) }
                || matches!(self.input[self.pos], b'_' | b':'))
        {
            return self.fail(b"invalid XML name");
        }
        while self.pos < self.input.len() && self.name_char(self.input[self.pos]) {
            self.pos += 1;
        }
        *out = lower(self.input[begin..self.pos].to_vec());
        true
    }
    fn decoded(&mut self, raw: &[u8], out: &mut Vec<u8>) -> bool {
        let mut index = 0;
        while index < raw.len() {
            if raw[index] != b'&' {
                let byte = raw[index];
                if byte < 0x20 && !matches!(byte, b'\t' | b'\n' | b'\r') {
                    return self.fail(b"invalid control character in XML");
                }
                out.push(byte);
                index += 1;
                continue;
            }
            let Some(end) = raw[index + 1..].iter().position(|&b| b == b';') else {
                return self.fail(b"unterminated XML entity");
            };
            let end = index + 1 + end;
            let token = &raw[index + 1..end];
            match token {
                b"amp" => out.push(b'&'),
                b"lt" => out.push(b'<'),
                b"gt" => out.push(b'>'),
                b"quot" => out.push(b'"'),
                b"apos" => out.push(b'\''),
                _ if token.len() >= 2 && token[0] == b'#' => {
                    let hex = token.len() >= 3 && matches!(token[1], b'x' | b'X');
                    let begin = if hex { 2 } else { 1 };
                    if begin == token.len() {
                        return self.fail(b"empty numeric XML entity");
                    }
                    let base = if hex { 16u32 } else { 10u32 };
                    let mut code = 0u32;
                    for &byte in &token[begin..] {
                        let digit = match byte {
                            b'0'..=b'9' => u32::from(byte - b'0'),
                            b'a'..=b'f' if hex => u32::from(byte - b'a') + 10,
                            b'A'..=b'F' if hex => u32::from(byte - b'A') + 10,
                            _ => return self.fail(b"invalid numeric XML entity"),
                        };
                        if digit >= base || code > (0x10ffff - digit) / base {
                            return self.fail(b"invalid numeric XML entity");
                        }
                        code = code * base + digit;
                    }
                    if code == 0
                        || (code < 0x20 && !matches!(code, 9 | 10 | 13))
                        || code > 0x10ffff
                        || (0xd800..=0xdfff).contains(&code)
                    {
                        return self.fail(b"invalid Unicode XML entity");
                    }
                    if code < 0x80 {
                        out.push(code as u8);
                    } else if code < 0x800 {
                        out.push((0xc0 | (code >> 6)) as u8);
                        out.push((0x80 | (code & 0x3f)) as u8);
                    } else if code < 0x10000 {
                        out.push((0xe0 | (code >> 12)) as u8);
                        out.push((0x80 | ((code >> 6) & 0x3f)) as u8);
                        out.push((0x80 | (code & 0x3f)) as u8);
                    } else {
                        out.push((0xf0 | (code >> 18)) as u8);
                        out.push((0x80 | ((code >> 12) & 0x3f)) as u8);
                        out.push((0x80 | ((code >> 6) & 0x3f)) as u8);
                        out.push((0x80 | (code & 0x3f)) as u8);
                    }
                }
                _ => return self.fail(b"unknown XML entity"),
            }
            index = end + 1;
        }
        true
    }
    fn element(&mut self, node: &mut Node, depth: usize) -> bool {
        self.nodes += 1;
        if depth >= 64 || self.nodes > 262144 {
            return self.fail(b"XML depth/node limit exceeded");
        }
        let opening = self.input.get(self.pos).copied();
        if opening == Some(b'<') {
            self.pos += 1;
        }
        if opening != Some(b'<') || self.starts(b"!") || self.starts(b"?") {
            return self.fail(b"expected XML element");
        }
        if !self.name(&mut node.name) {
            return false;
        }
        loop {
            self.spaces();
            if self.starts(b"/>") {
                self.pos += 2;
                return true;
            }
            if self.starts(b">") {
                self.pos += 1;
                break;
            }
            let mut key = Vec::new();
            if !self.name(&mut key) {
                return false;
            }
            if node.attributes.len() >= 64 {
                return self.fail(b"XML attribute limit exceeded");
            }
            self.spaces();
            if self.input.get(self.pos) != Some(&b'=') {
                if self.pos < self.input.len() {
                    self.pos += 1;
                }
                return self.fail(b"missing XML attribute equals");
            }
            self.pos += 1;
            self.spaces();
            let quote = self.input.get(self.pos).copied();
            if !matches!(quote, Some(b'"' | b'\'')) {
                return self.fail(b"unquoted XML attribute");
            }
            let quote = quote.unwrap_or_default();
            self.pos += 1;
            let Some(offset) = self.input[self.pos..].iter().position(|&b| b == quote) else {
                return self.fail(b"unterminated XML attribute");
            };
            let end = self.pos + offset;
            if self.input[self.pos..end].contains(&b'<') {
                return self.fail(b"raw less-than in XML attribute");
            }
            let mut value = Vec::new();
            let raw = &self.input[self.pos..end];
            if !self.decoded(raw, &mut value) {
                return false;
            }
            node.attributes.push((key, value));
            self.pos = end + 1;
        }
        loop {
            if self.pos >= self.input.len() {
                return self.fail(b"unterminated XML element");
            }
            if self.starts(b"</") {
                self.pos += 2;
                let mut close = Vec::new();
                if !self.name(&mut close) {
                    return false;
                }
                self.spaces();
                let marker = self.input.get(self.pos).copied();
                if self.pos < self.input.len() {
                    self.pos += 1;
                }
                if marker != Some(b'>') || close != node.name {
                    return self.fail(b"mismatched XML closing tag");
                }
                return true;
            }
            if self.starts(b"<!--") || self.starts(b"<?") {
                if !self.misc() {
                    return false;
                }
                continue;
            }
            if self.starts(b"<!") {
                return self.fail(b"unsupported XML markup");
            }
            if self.starts(b"<") {
                let mut child = Node::new();
                if !self.element(&mut child, depth + 1) {
                    return false;
                }
                node.children.push(child);
            } else {
                let Some(offset) = self.input[self.pos..].iter().position(|&b| b == b'<') else {
                    return self.fail(b"unterminated XML text");
                };
                let end = self.pos + offset;
                let raw = &self.input[self.pos..end];
                if !self.decoded(raw, &mut node.text) {
                    return false;
                }
                self.pos = end;
            }
        }
    }
    fn document(&mut self, root: &mut Node) -> bool {
        if self.input.len() > 32 * 1024 * 1024 {
            return self.fail(b"XML byte limit exceeded");
        }
        if self.input.starts_with(b"\xef\xbb\xbf") {
            self.pos = 3;
        }
        if !self.misc() || !self.element(root, 0) || !self.misc() {
            return false;
        }
        if self.pos != self.input.len() {
            return self.fail(b"trailing XML content");
        }
        true
    }
}

struct Object {
    odf: Vec<u8>,
    must_not_have: bool,
}
struct Peer {
    ally: bool,
    race: Vec<u8>,
    objects: Vec<Object>,
}
struct Requirement {
    race: Vec<u8>,
    objects: Vec<Object>,
    peers: Vec<Peer>,
    groups: Vec<i32>,
}
struct Definition {
    key: Vec<u8>,
    requirements: Vec<Requirement>,
    unsupported: bool,
    reason: &'static [u8],
}
impl Definition {
    fn new(key: Vec<u8>) -> Self {
        Self {
            key,
            requirements: Vec::new(),
            unsupported: false,
            reason: b"",
        }
    }
    fn mark(&mut self, reason: &'static [u8]) {
        self.unsupported = true;
        if self.reason.is_empty() {
            self.reason = reason;
        }
    }
}
fn parse_bool(raw: &[u8], class: Classify) -> Option<bool> {
    let text = lower(trim(raw, class).to_vec());
    if matches!(text.as_slice(), b"true" | b"1" | b"yes") {
        Some(true)
    } else if matches!(text.as_slice(), b"false" | b"0" | b"no") {
        Some(false)
    } else {
        None
    }
}
fn parse_groups(raw: &[u8], class: Classify, ids: &mut Vec<i32>) -> bool {
    if trim(raw, class).is_empty() {
        return true;
    }
    let mut begin = 0;
    loop {
        let comma = raw[begin..]
            .iter()
            .position(|&b| b == b',')
            .map(|offset| begin + offset);
        let token = trim(&raw[begin..comma.unwrap_or(raw.len())], class);
        if token.is_empty() {
            return false;
        }
        let mut index = if matches!(token[0], b'+' | b'-') {
            1
        } else {
            0
        };
        if index == token.len() {
            return false;
        }
        let negative = token[0] == b'-';
        let limit = if negative {
            2_147_483_648i64
        } else {
            2_147_483_647i64
        };
        let mut value = 0i64;
        while index < token.len() {
            let byte = token[index];
            if !byte.is_ascii_digit() {
                return false;
            }
            let digit = i64::from(byte - b'0');
            if value > (limit - digit) / 10 {
                return false;
            }
            value = value * 10 + digit;
            index += 1;
        }
        ids.push(if negative {
            -value as i32
        } else {
            value as i32
        });
        let Some(comma) = comma else { break };
        begin = comma + 1;
    }
    true
}
fn parse_object(node: &Node, definition: &mut Definition, class: Classify) -> Object {
    if node.name != b"obj" || !node.children.is_empty() {
        definition.mark(b"unsupported nested object requirement");
    }
    let odf = normalize_odf(&node.text, class);
    if odf.is_empty() {
        definition.mark(b"empty object requirement");
    }
    let mut object = Object {
        odf,
        must_not_have: false,
    };
    for (key, value) in &node.attributes {
        if key == b"mustnothave" {
            if let Some(parsed) = parse_bool(value, class) {
                object.must_not_have = parsed;
            } else {
                definition.mark(b"invalid mustNotHave value");
            }
        } else {
            definition.mark(b"unsupported object attribute");
        }
    }
    object
}
fn parse_peer(node: &Node, definition: &mut Definition, class: Classify) -> Peer {
    let mut peer = Peer {
        ally: node.name == b"ally",
        race: Vec::new(),
        objects: Vec::new(),
    };
    if !whitespace_only(&node.text, class) {
        definition.mark(b"unexpected peer text");
    }
    for (key, value) in &node.attributes {
        if key == b"race" {
            peer.race = trim(value, class).to_vec();
        } else {
            definition.mark(b"unsupported peer attribute");
        }
    }
    for child in &node.children {
        if child.name != b"obj" {
            definition.mark(b"unsupported peer child");
            continue;
        }
        peer.objects.push(parse_object(child, definition, class));
    }
    peer
}
fn parse_requirement(node: &Node, definition: &mut Definition, class: Classify) -> Requirement {
    let mut requirement = Requirement {
        race: Vec::new(),
        objects: Vec::new(),
        peers: Vec::new(),
        groups: Vec::new(),
    };
    if !whitespace_only(&node.text, class) {
        definition.mark(b"unexpected requirement text");
    }
    for (key, value) in &node.attributes {
        if key == b"race" {
            requirement.race = trim(value, class).to_vec();
        } else if key == b"logicgroups" {
            if !parse_groups(value, class, &mut requirement.groups) {
                definition.mark(b"invalid logicGroups value");
            }
        } else {
            definition.mark(b"unsupported requirement attribute");
        }
    }
    for child in &node.children {
        if child.name == b"obj" {
            requirement
                .objects
                .push(parse_object(child, definition, class));
        } else if child.name == b"ally" || child.name == b"enemy" {
            requirement.peers.push(parse_peer(child, definition, class));
        } else {
            definition.mark(b"unsupported requirement child");
        }
    }
    requirement
}
fn check_duplicates(node: &Node, definition: &mut Definition) {
    for first in 0..node.attributes.len() {
        for second in first + 1..node.attributes.len() {
            if node.attributes[first].0 == node.attributes[second].0 {
                definition.mark(b"duplicate MODULE XML attribute");
            }
        }
    }
    for child in &node.children {
        check_duplicates(child, definition);
    }
}
fn parse_module(node: &Node, class: Classify) -> Option<Definition> {
    let module = node.attribute(b"module")?;
    let mut definition = Definition::new(normalize_odf(module, class));
    check_duplicates(node, &mut definition);
    if definition.key.is_empty() {
        definition.mark(b"empty MODULE name");
    }
    if !whitespace_only(&node.text, class) {
        definition.mark(b"unexpected MODULE text");
    }
    for (key, _) in &node.attributes {
        if key != b"module" {
            definition.mark(b"unsupported MODULE attribute");
        }
    }
    for child in &node.children {
        if child.name == b"requirement" {
            let requirement = parse_requirement(child, &mut definition, class);
            definition.requirements.push(requirement);
        } else {
            definition.mark(b"unsupported MODULE child");
        }
    }
    Some(definition)
}
fn send(context: *mut c_void, emit: Emit, op: u32, a: &[u8], value: i32) -> bool {
    let event = Event {
        op,
        a: Bytes::from(a),
        value,
    };
    unsafe { emit(context, &raw const event) }
}
fn emit_definitions(root: &Node, class: Classify, context: *mut c_void, emit: Emit) -> bool {
    let nodes: &[Node] = if root.name == b"object" {
        core::slice::from_ref(root)
    } else {
        &root.children
    };
    for node in nodes {
        if node.name != b"object" {
            continue;
        }
        let Some(definition) = parse_module(node, class) else {
            continue;
        };
        if !send(context, emit, 0, &definition.key, 0) {
            return false;
        }
        if definition.unsupported && !send(context, emit, 1, definition.reason, 0) {
            return false;
        }
        for requirement in definition.requirements {
            if !send(context, emit, 2, &requirement.race, 0) {
                return false;
            }
            for group in requirement.groups {
                if !send(context, emit, 3, b"", group) {
                    return false;
                }
            }
            for object in requirement.objects {
                if !send(
                    context,
                    emit,
                    4,
                    &object.odf,
                    i32::from(object.must_not_have),
                ) {
                    return false;
                }
            }
            for peer in requirement.peers {
                if !send(context, emit, 5, &peer.race, i32::from(peer.ally)) {
                    return false;
                }
                for object in peer.objects {
                    if !send(
                        context,
                        emit,
                        6,
                        &object.odf,
                        i32::from(object.must_not_have),
                    ) {
                        return false;
                    }
                }
                if !send(context, emit, 7, b"", 0) {
                    return false;
                }
            }
            if !send(context, emit, 8, b"", 0) {
                return false;
            }
        }
        if !send(context, emit, 9, b"", 0) {
            return false;
        }
    }
    true
}

/// # Safety
/// `xml` is readable for `size` bytes. Native CRT classifier and C++ result
/// emission callbacks never unwind through Rust; C++ captures exceptions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_parse(
    xml: *const u8,
    size: usize,
    class: Option<Classify>,
    context: *mut c_void,
    emit: Option<Emit>,
) -> bool {
    let (Some(class), Some(emit)) = (class, emit) else {
        return false;
    };
    if xml.is_null() && size != 0 {
        return false;
    }
    let bytes = if size == 0 {
        &[][..]
    } else {
        unsafe { core::slice::from_raw_parts(xml, size) }
    };
    let mut reader = Reader::new(bytes, class);
    let mut root = Node::new();
    if !reader.document(&mut root) {
        return send(context, emit, 10, reader.error, 0);
    }
    emit_definitions(&root, class, context, emit)
}

#[repr(C)]
pub struct EvalRequest {
    pub op: u32,
    pub requirement: usize,
    pub peer: usize,
    pub object: usize,
    pub team: i32,
    pub other: i32,
    pub ally: bool,
    pub text: Bytes,
}
pub type EvalStep = unsafe extern "C" fn(*mut c_void, *mut EvalRequest) -> i64;
pub type EvalDone = unsafe extern "C" fn(*mut c_void, bool, *const u8, usize) -> bool;
const OWN: usize = usize::MAX;
struct Engine {
    context: *mut c_void,
    step: EvalStep,
    class: Classify,
    owner: i32,
}
impl Engine {
    // Separate selectors preserve the callback's original nested read order.
    #[allow(clippy::too_many_arguments)]
    fn call(
        &self,
        op: u32,
        req: usize,
        peer: usize,
        obj: usize,
        team: i32,
        other: i32,
        ally: bool,
    ) -> Result<(i64, Vec<u8>), ()> {
        let mut request = EvalRequest {
            op,
            requirement: req,
            peer,
            object: obj,
            team,
            other,
            ally,
            text: Bytes {
                data: core::ptr::null(),
                size: 0,
            },
        };
        let value = unsafe { (self.step)(self.context, &raw mut request) };
        if value == i64::MIN {
            return Err(());
        }
        let text = if request.text.size == 0 {
            Vec::new()
        } else if request.text.data.is_null() {
            return Err(());
        } else {
            unsafe { core::slice::from_raw_parts(request.text.data, request.text.size) }.to_vec()
        };
        Ok((value, text))
    }
    #[allow(clippy::too_many_arguments)]
    fn value(
        &self,
        op: u32,
        req: usize,
        peer: usize,
        obj: usize,
        team: i32,
        other: i32,
        ally: bool,
    ) -> Result<i64, ()> {
        self.call(op, req, peer, obj, team, other, ally)
            .map(|(value, _)| value)
    }
    fn field(&self, op: u32, req: usize, peer: usize, obj: usize) -> Result<(i64, Vec<u8>), ()> {
        self.call(op, req, peer, obj, self.owner, 0, false)
    }
    fn object_label(
        &self,
        req: usize,
        peer: usize,
        obj: usize,
        odf: &[u8],
        must: bool,
    ) -> Result<Vec<u8>, ()> {
        let (_, mut label) = self.field(23, req, peer, obj)?;
        if label.is_empty() {
            label = normalize_odf(odf, self.class);
            if label.len() >= 4 {
                label.truncate(label.len() - 4);
            }
        }
        if must {
            let mut text = b"Not ".to_vec();
            text.extend_from_slice(&label);
            Ok(text)
        } else {
            Ok(label)
        }
    }
    fn missing_objects(&self, req: usize, peer: usize, team: i32) -> Result<Vec<Vec<u8>>, ()> {
        let op = if peer == OWN { 4 } else { 9 };
        let field = if peer == OWN { 5 } else { 10 };
        let count = self.value(op, req, peer, 0, team, 0, false)? as usize;
        let mut labels = Vec::new();
        for obj in 0..count {
            let (must, odf) = self.field(field, req, peer, obj)?;
            let count = self.value(20, req, peer, obj, team, 0, false)?;
            let met = count >= 0 && if must != 0 { count == 0 } else { count > 0 };
            if !met {
                labels.push(self.object_label(req, peer, obj, &odf, must != 0)?);
            }
        }
        Ok(labels)
    }
    fn requirement_text(&self, req: usize) -> Result<Vec<u8>, ()> {
        let (_, race) = self.field(3, req, OWN, 0)?;
        let mut missing = Vec::new();
        if !race.is_empty() && self.value(21, req, OWN, 0, self.owner, 0, false)? == 0 {
            let mut text = b"Race ".to_vec();
            text.extend_from_slice(&race);
            missing.push(text);
        }
        missing.extend(self.missing_objects(req, OWN, self.owner)?);
        let peers = self.value(6, req, 0, 0, self.owner, 0, false)? as usize;
        for peer in 0..peers {
            let ally = self.value(7, req, peer, 0, self.owner, 0, false)? != 0;
            let (_, peer_race) = self.field(8, req, peer, 0)?;
            let mut fulfilled = false;
            for other in 1..=8 {
                if other == self.owner
                    || self.value(22, req, peer, 0, self.owner, other, ally)? == 0
                {
                    continue;
                }
                if !peer_race.is_empty() && self.value(21, req, peer, 0, other, 0, false)? == 0 {
                    continue;
                }
                if self.missing_objects(req, peer, other)?.is_empty() {
                    fulfilled = true;
                    break;
                }
            }
            if fulfilled {
                continue;
            }
            let mut scope = if ally {
                b"Ally".to_vec()
            } else {
                b"Enemy".to_vec()
            };
            if !peer_race.is_empty() {
                scope.extend_from_slice(b" [");
                scope.extend_from_slice(&peer_race);
                scope.push(b']');
            }
            let count = self.value(9, req, peer, 0, self.owner, 0, false)? as usize;
            let mut labels = Vec::new();
            for obj in 0..count {
                let (must, odf) = self.field(10, req, peer, obj)?;
                labels.push(self.object_label(req, peer, obj, &odf, must != 0)?);
            }
            if !labels.is_empty() {
                scope.extend_from_slice(b": (");
                scope.extend_from_slice(&join(&labels, b" AND "));
                scope.push(b')');
            }
            missing.push(scope);
        }
        Ok(join(&missing, b" AND "))
    }
    fn evaluate(&self) -> Result<(bool, Vec<u8>), ()> {
        if self.value(0, 0, 0, 0, self.owner, 0, false)? != 0 {
            return Ok((false, b"unsupported MODULE technology rule".to_vec()));
        }
        if !(1..=8).contains(&self.owner) || self.value(1, 0, 0, 0, self.owner, 0, false)? == 0 {
            return Ok((false, b"unavailable requirement data".to_vec()));
        }
        let count = self.value(2, 0, 0, 0, self.owner, 0, false)? as usize;
        if count == 0 {
            return Ok((true, Vec::new()));
        }
        let mut groups = Vec::new();
        for req in 0..count {
            let length = self.value(11, req, 0, 0, self.owner, 0, false)? as usize;
            for index in 0..length {
                let id = self.value(12, req, 0, index, self.owner, 0, false)? as i32;
                if !groups.contains(&id) {
                    groups.push(id);
                }
            }
        }
        let mut alternatives = Vec::new();
        if groups.is_empty() {
            for req in 0..count {
                let missing = self.requirement_text(req)?;
                if missing.is_empty() {
                    return Ok((true, Vec::new()));
                }
                alternatives.push(missing);
            }
        } else {
            for id in groups {
                let mut parts = Vec::new();
                for req in 0..count {
                    let length = self.value(11, req, 0, 0, self.owner, 0, false)? as usize;
                    let mut member = false;
                    for index in 0..length {
                        if self.value(12, req, 0, index, self.owner, 0, false)? as i32 == id {
                            member = true;
                            break;
                        }
                    }
                    if !member {
                        continue;
                    }
                    let missing = self.requirement_text(req)?;
                    if !missing.is_empty() {
                        parts.push(missing);
                    }
                }
                if parts.is_empty() {
                    return Ok((true, Vec::new()));
                }
                alternatives.push(join(&parts, b" AND "));
            }
        }
        let mut expression = Vec::new();
        for alternative in &alternatives {
            if !expression.is_empty() {
                expression.extend_from_slice(b" OR ");
            }
            if alternatives.len() > 1 {
                expression.push(b'(');
            }
            expression.extend_from_slice(alternative);
            if alternatives.len() > 1 {
                expression.push(b')');
            }
        }
        Ok((false, expression))
    }
}
fn join(parts: &[Vec<u8>], separator: &[u8]) -> Vec<u8> {
    let mut output = Vec::new();
    for part in parts {
        if !output.is_empty() {
            output.extend_from_slice(separator);
        }
        output.extend_from_slice(part);
    }
    output
}

/// # Safety
/// Native callbacks read a stable C++ Definition and invoke game/count/label
/// operations. They capture all C++ exceptions and return i64::MIN on error.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_evaluate(
    context: *mut c_void,
    owner: i32,
    class: Option<Classify>,
    step: Option<EvalStep>,
    done: Option<EvalDone>,
) -> bool {
    let (Some(class), Some(step), Some(done)) = (class, step, done) else {
        return false;
    };
    let engine = Engine {
        context,
        step,
        class,
        owner,
    };
    let Ok((available, text)) = engine.evaluate() else {
        return false;
    };
    unsafe { done(context, available, text.as_ptr(), text.len()) }
}

/// # Safety
/// Nonempty keys are readable byte views from live native strings.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_merge(
    existing: Bytes,
    incoming: Bytes,
    unsupported: bool,
    reason_empty: bool,
) -> u8 {
    if (existing.data.is_null() && existing.size != 0)
        || (incoming.data.is_null() && incoming.size != 0)
    {
        return 0;
    }
    let left = if existing.size == 0 {
        &[][..]
    } else {
        unsafe { core::slice::from_raw_parts(existing.data, existing.size) }
    };
    let right = if incoming.size == 0 {
        &[][..]
    } else {
        unsafe { core::slice::from_raw_parts(incoming.data, incoming.size) }
    };
    if left != right {
        0
    } else if !unsupported {
        1
    } else if reason_empty {
        3
    } else {
        2
    }
}

pub type Step = unsafe extern "C" fn(*mut c_void, u32, usize) -> i64;

/// # Safety
/// Native callback owns the relocated byte buffer and exact PE arithmetic.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_relocated(
    context: *mut c_void,
    count: usize,
    size: usize,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    for index in 0..count {
        let offset = unsafe { step(context, 0, index) };
        if offset == i64::MIN || offset < 0 {
            return false;
        }
        if offset as u64 + 4 > size as u64 {
            return false;
        }
        if unsafe { step(context, 1, index) } == i64::MIN {
            return false;
        }
    }
    (unsafe { step(context, 2, 0) }) > 0
}

/// # Safety
/// Callback invokes the exact native signature checks and diagnostics.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_validate_team(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    let mut valid = true;
    for op in 0..5 {
        let value = unsafe { step(context, op, 0) };
        if value == i64::MIN {
            return false;
        }
        if value == 0 {
            valid = false;
            break;
        }
    }
    if unsafe { step(context, 5, usize::from(valid)) } == i64::MIN {
        return false;
    }
    valid
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_buildyard_native_team_admit(ready: bool, team: i32) -> bool {
    ready && (0..9).contains(&team)
}

/// # Safety
/// Callback owns native faction/Race objects and catches C++ exceptions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_race(
    context: *mut c_void,
    empty: bool,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if empty {
        return true;
    }
    if unsafe { step(context, 0, 0) } == i64::MIN {
        return false;
    }
    for op in 1..5 {
        let value = unsafe { step(context, op, 0) };
        if value == i64::MIN || value == 0 {
            return false;
        }
    }
    (unsafe { step(context, 5, 0) }) > 0
}

/// # Safety
/// Callback owns native player/relationship calls and catches exceptions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_peer(
    context: *mut c_void,
    ready: bool,
    owner: i32,
    other: i32,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if !ready || !(1..=8).contains(&owner) || !(1..=8).contains(&other) || owner == other {
        return false;
    }
    for op in 0..7 {
        let value = unsafe { step(context, op, 0) };
        if value == i64::MIN {
            return false;
        }
        if op >= 2 && value == 0 {
            return false;
        }
    }
    true
}

/// # Safety
/// Callback validates VirtualQuery regions and copies bounded native bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_copy_text(
    context: *mut c_void,
    maximum: usize,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if unsafe { step(context, 0, 0) } == i64::MIN {
        return false;
    }
    if unsafe { step(context, 1, 0) } <= 0 {
        return false;
    }
    let mut consumed = 0usize;
    while consumed < maximum {
        let region = unsafe { step(context, 2, consumed) };
        if region <= 0 {
            return false;
        }
        let count = unsafe { step(context, 3, consumed) };
        if count <= 0 {
            return false;
        }
        let found = unsafe { step(context, 4, count as usize) };
        if found == i64::MIN {
            return false;
        }
        if found != 0 {
            return true;
        }
        consumed += count as usize;
    }
    false
}

/// # Safety
/// C++ retains native strings, parser result, lock and exception boundary.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_capture(
    context: *mut c_void,
    team: i32,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if !(0..9).contains(&team) {
        return false;
    }
    let filename = unsafe { step(context, 0, 0) };
    if filename == i64::MIN {
        return false;
    }
    let text = if filename != 0 {
        unsafe { step(context, 1, 0) }
    } else {
        0
    };
    if text == i64::MIN {
        return false;
    }
    if filename == 0 || text == 0 {
        unsafe { step(context, 2, 0) };
        return true;
    }
    if unsafe { step(context, 3, 0) } == i64::MIN {
        return false;
    }
    if unsafe { step(context, 4, 0) } == i64::MIN {
        return false;
    }
    true
}

/// # Safety
/// Caller holds the C++ PolicyLockGuard for this entire Rust traversal.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_capture_publish(
    context: *mut c_void,
    count: usize,
    team: i32,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    let parsed = unsafe { step(context, 0, 0) };
    if parsed == i64::MIN {
        return false;
    }
    if parsed == 0 {
        unsafe { step(context, 1, 0) };
        return true;
    }
    for index in 0..count {
        let inserted = unsafe { step(context, 2, index) };
        if inserted == i64::MIN {
            return false;
        }
        if inserted == 0 {
            if unsafe { step(context, 4, 0) } == i64::MIN {
                return false;
            }
            let unsupported = unsafe { step(context, 5, 0) };
            if unsupported == i64::MIN {
                return false;
            }
            if unsupported != 0 && unsafe { step(context, 6, 0) } == i64::MIN {
                return false;
            }
        }
    }
    if count != 0 && team == 1 && unsafe { step(context, 3, 0) } == i64::MIN {
        return false;
    }
    true
}

/// # Safety
/// C++ catches and maps native exceptions as the original noexcept boundary.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_begin(
    context: *mut c_void,
    team: i32,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if !(0..9).contains(&team) {
        return false;
    }
    (unsafe { step(context, 0, 0) }) != i64::MIN
}

/// # Safety
/// Caller retains the PolicyLockGuard throughout this workflow.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_begin_locked(
    context: *mut c_void,
    team: i32,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    let first = if team == 0 { 0 } else { 1 };
    if unsafe { step(context, first, 0) } == i64::MIN {
        return false;
    }
    if unsafe { step(context, 2, 0) } == i64::MIN {
        return false;
    }
    if team == 1 && unsafe { step(context, 3, 0) } == i64::MIN {
        return false;
    }
    true
}

/// # Safety
/// Native callback checks exact bytes and performs exact patch calls.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_install_tree(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    for op in 0..6 {
        let valid = unsafe { step(context, op, 0) };
        if valid == i64::MIN {
            return false;
        }
        if valid == 0 {
            unsafe { step(context, 6, 0) };
            return false;
        }
    }
    if unsafe { step(context, 7, 0) } == i64::MIN {
        return false;
    }
    let text = unsafe { step(context, 8, 0) };
    if text == i64::MIN {
        return false;
    }
    let root = if text != 0 {
        unsafe { step(context, 9, 0) }
    } else {
        0
    };
    if root == i64::MIN {
        return false;
    }
    if text == 0 || root == 0 {
        unsafe { step(context, 10, 0) };
        return false;
    }
    (unsafe { step(context, 11, 0) }) != i64::MIN
}

/// # Safety
/// Native text call already happened; callback reads verified frame locals.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_tree_text(
    context: *mut c_void,
    ready: bool,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if !ready {
        return true;
    }
    (unsafe { step(context, 0, 0) }) != i64::MIN
}

/// # Safety
/// Callback owns the native lock and selected-tree storage.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_tree_commit(
    context: *mut c_void,
    team: i32,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if !(0..9).contains(&team) {
        return false;
    }
    (unsafe { step(context, 0, 0) }) != i64::MIN
}

pub type Lower = unsafe extern "C" fn(u8) -> u8;
#[repr(C)]
pub struct Span {
    pub first: usize,
    pub length: usize,
}
fn view(bytes: &Bytes) -> Option<&[u8]> {
    if bytes.data.is_null() {
        if bytes.size != 0 {
            return None;
        }
        return Some(&[]);
    }
    Some(unsafe { core::slice::from_raw_parts(bytes.data, bytes.size) })
}

/// # Safety
/// `bytes` is writable for `size`; native CRT lowering never unwinds.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_lower(
    bytes: *mut u8,
    size: usize,
    lower: Option<Lower>,
) {
    let Some(lower) = lower else { return };
    if bytes.is_null() {
        return;
    }
    for index in 0..size {
        let byte = unsafe { bytes.add(index) };
        unsafe { *byte = lower(*byte) };
    }
}

/// # Safety
/// Input is a borrowed native byte string; span is a writable POD output.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_trim(
    bytes: Bytes,
    class: Option<Classify>,
    span: *mut Span,
) -> bool {
    let Some(class) = class else { return false };
    let Some(input) = view(&bytes) else {
        return false;
    };
    if span.is_null() {
        return false;
    }
    let mut first = 0;
    let mut last = input.len();
    while first < last && unsafe { class(input[first], 0) } {
        first += 1;
    }
    while last > first && unsafe { class(input[last - 1], 0) } {
        last -= 1;
    }
    unsafe {
        *span = Span {
            first,
            length: last - first,
        }
    };
    true
}
fn ends(value: &[u8], suffix: &[u8], lower: Lower) -> bool {
    if value.len() < suffix.len() {
        return false;
    }
    let offset = value.len() - suffix.len();
    for index in 0..suffix.len() {
        let left = unsafe { lower(value[offset + index]) };
        let right = unsafe { lower(suffix[index]) };
        if left != right {
            return false;
        }
    }
    true
}

/// # Safety
/// Both views are readable for declared sizes; native CRT lowering is exact.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_ends(
    value: Bytes,
    suffix: Bytes,
    lower: Option<Lower>,
) -> bool {
    let Some(lower) = lower else { return false };
    let (Some(value), Some(suffix)) = (view(&value), view(&suffix)) else {
        return false;
    };
    ends(value, suffix, lower)
}

/// # Safety
/// Callback assigns a C++ string and captures exceptions before returning.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_normalize(
    bytes: Bytes,
    mode: u32,
    class: Option<Classify>,
    lower: Option<Lower>,
    context: *mut c_void,
    emit: Option<Emit>,
) -> bool {
    let (Some(class), Some(lower), Some(emit)) = (class, lower, emit) else {
        return false;
    };
    let Some(input) = view(&bytes) else {
        return false;
    };
    let mut value = if mode == 0 {
        let trimmed = trim(input, class).to_vec();
        let mut value = trimmed;
        for byte in &mut value {
            if *byte == b'/' {
                *byte = b'\\';
            }
        }
        if let Some(index) = value.iter().rposition(|&b| b == b'\\') {
            value.drain(..index + 1);
        }
        if !value.is_empty() && !ends(&value, b".odf", lower) {
            value.extend_from_slice(b".odf");
        }
        value
    } else {
        let mut value = input.to_vec();
        if let Some(index) = value.iter().rposition(|&b| b == b'\\' || b == b'/') {
            value.drain(..index + 1);
        }
        if ends(&value, b".odf", lower) {
            value.truncate(value.len() - 4);
        }
        value
    };
    if mode == 0 {
        for byte in &mut value {
            *byte = unsafe { lower(*byte) };
        }
    }
    send(context, emit, 0, &value, 0)
}

/// # Safety
/// Native callback reads one command byte per request and applies CRT
/// case/digit classification, preserving early rejection before later reads.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_parse_command(
    context: *mut c_void,
    maximum: usize,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    for (index, &byte) in b"Module".iter().enumerate() {
        if unsafe { step(context, 1, (index << 8) | usize::from(byte)) } != 1 {
            return false;
        }
    }
    let mut cursor = 6usize;
    if unsafe { step(context, 2, cursor) } != 1 {
        return false;
    }
    let mut index = 0usize;
    loop {
        let digit = unsafe { step(context, 0, cursor) };
        if digit == i64::MIN || digit < i64::from(b'0') || digit > i64::from(b'9') {
            return false;
        }
        let digit = (digit - i64::from(b'0')) as usize;
        if index > (maximum - digit) / 10 {
            return false;
        }
        index = index * 10 + digit;
        cursor += 1;
        if unsafe { step(context, 2, cursor) } != 1 {
            break;
        }
    }
    if index > maximum {
        return false;
    }
    for &byte in b"RequiredTechnology" {
        if unsafe { step(context, 1, (cursor << 8) | usize::from(byte)) } != 1 {
            return false;
        }
        cursor += 1;
    }
    if unsafe { step(context, 0, cursor) } != 0 {
        return false;
    }
    (unsafe { step(context, 3, index) }) != i64::MIN
}

/// # Safety
/// Native callback probes and reads exactly one Delphi byte per iteration.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_copy_delphi(
    context: *mut c_void,
    size: usize,
    step: Option<Step>,
) {
    let Some(step) = step else { return };
    if size == 0 {
        return;
    }
    if unsafe { step(context, 0, 0) } == i64::MIN {
        return;
    }
    if unsafe { step(context, 1, 0) } <= 0 {
        return;
    }
    for index in 0..size - 1 {
        if unsafe { step(context, 2, index) } <= 0 {
            break;
        }
        let byte = unsafe { step(context, 3, index) };
        if byte <= 0 || byte < 0x20 && byte != i64::from(b'\t') {
            break;
        }
        if unsafe { step(context, 4, index) } == i64::MIN {
            return;
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_buildyard_byte_truth(value: usize) -> bool {
    value & 0xff != 0
}

/// # Safety
/// Native callback owns snprintf, parameter DB calls and result storage.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_tech_entry(
    context: *mut c_void,
    capacity: usize,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    for alias in 0..2usize {
        if alias == 1 && unsafe { step(context, 3, 0) } == i64::MIN {
            return false;
        }
        let written = unsafe { step(context, 0, alias) };
        if written == i64::MIN {
            return false;
        }
        if written <= 0 || written as usize >= capacity {
            continue;
        }
        let found = unsafe { step(context, 1, 0) };
        if found == i64::MIN {
            return false;
        }
        if found == 0 {
            continue;
        }
        return unsafe { step(context, 2, alias) } != i64::MIN;
    }
    true
}

/// # Safety
/// Native callback owns snprintf, parameter DB call and tooltip-key string.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_tooltip_key(
    context: *mut c_void,
    capacity: usize,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    let written = unsafe { step(context, 0, 0) };
    if written == i64::MIN {
        return false;
    }
    if written <= 0 || written as usize >= capacity {
        return true;
    }
    let found = unsafe { step(context, 1, 0) };
    if found == i64::MIN {
        return false;
    }
    if found != 0 && unsafe { step(context, 2, 0) } == i64::MIN {
        return false;
    }
    true
}

/// # Safety
/// Native callback retains ifstream and C++ output storage across this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_read_text(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if unsafe { step(context, 0, 0) } <= 0 {
        return false;
    }
    if unsafe { step(context, 1, 0) } <= 0 {
        return false;
    }
    let size = unsafe { step(context, 2, 0) };
    if !(0..=32 * 1024 * 1024).contains(&size) {
        return false;
    }
    if unsafe { step(context, 3, 0) } == i64::MIN {
        return false;
    }
    (unsafe { step(context, 4, 0) }) > 0
}

/// # Safety
/// Caller owns the PolicyLockGuard and native definition map.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_register_definition(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    let overrides = unsafe { step(context, 0, 0) };
    if overrides == i64::MIN {
        return false;
    }
    if overrides != 0 && unsafe { step(context, 1, 0) } == i64::MIN {
        return false;
    }
    (unsafe { step(context, 2, 0) }) != i64::MIN
}

/// # Safety
/// Native callback owns text file, parser result and definition moves.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_parse_catalog(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if unsafe { step(context, 0, 0) } <= 0 {
        return false;
    }
    let parsed = unsafe { step(context, 1, 0) };
    if parsed == i64::MIN {
        return false;
    }
    if parsed == 0 {
        unsafe { step(context, 2, 0) };
        return false;
    }
    let count = unsafe { step(context, 3, 0) };
    if count == i64::MIN {
        return false;
    }
    for index in 0..count as usize {
        if unsafe { step(context, 4, index) } == i64::MIN {
            return false;
        }
    }
    true
}

/// # Safety
/// Native callback owns the FindFirst/Next handle and exact std::sort call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_enumerate_xml(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if unsafe { step(context, 0, 0) } <= 0 {
        return true;
    }
    loop {
        let directory = unsafe { step(context, 1, 0) };
        if directory == i64::MIN {
            return false;
        }
        if directory == 0 && unsafe { step(context, 2, 0) } == i64::MIN {
            return false;
        }
        let more = unsafe { step(context, 3, 0) };
        if more == i64::MIN {
            return false;
        }
        if more == 0 {
            break;
        }
    }
    if unsafe { step(context, 4, 0) } == i64::MIN {
        return false;
    }
    (unsafe { step(context, 5, 0) }) != i64::MIN
}

/// # Safety
/// Native callback reads extension roots and C++ file/vector storage live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_load_symbolic(
    context: *mut c_void,
    step: Option<Step>,
) -> usize {
    let Some(step) = step else { return 0 };
    let count = unsafe { step(context, 0, 0) };
    if count == i64::MIN || count < 0 {
        return 0;
    }
    let mut total = 0usize;
    for root in 0..count as usize {
        let accepted = unsafe { step(context, 1, root) };
        if accepted == i64::MIN {
            return total;
        }
        if accepted == 0 {
            continue;
        }
        let files = unsafe { step(context, 2, 0) };
        if files == i64::MIN {
            return total;
        }
        for file in 0..files as usize {
            let added = unsafe { step(context, 3, file) };
            if added == i64::MIN {
                return total;
            }
            total = total.wrapping_add(added as usize);
        }
    }
    total
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_c_buildyard_has_bytes(bytes: Bytes) -> bool {
    bytes.size != 0
}

/// # Safety
/// Native callback owns event field storage and the tooltip map lock.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_class_loaded(
    context: *mut c_void,
    count: u32,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if unsafe { step(context, 0, 0) } <= 0 {
        return false;
    }
    if unsafe { step(context, 1, 0) } <= 0 {
        return false;
    }
    for index in 0..count {
        let match_name = unsafe { step(context, 2, index as usize) };
        if match_name == i64::MIN {
            return false;
        }
        if match_name != 0 {
            break;
        }
    }
    if unsafe { step(context, 3, 0) } <= 0 {
        return false;
    }
    (unsafe { step(context, 4, 0) }) != i64::MIN
}

/// # Safety
/// Native callback owns the lookup lock and result string storage.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_display_name(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    let found = unsafe { step(context, 0, 0) };
    if found == i64::MIN {
        return false;
    }
    if found != 0 {
        return true;
    }
    (unsafe { step(context, 1, 0) }) != i64::MIN
}

/// # Safety
/// Native callback owns project-ID map, resolver, locks, and log storage.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_requirement_id(
    context: *mut c_void,
    step: Option<Step>,
) -> u32 {
    let Some(step) = step else { return 0 };
    let cached = unsafe { step(context, 0, 0) };
    if cached == i64::MIN {
        return 0;
    }
    if cached != 0 {
        return cached as u32;
    }
    let resolved = unsafe { step(context, 1, 0) };
    if resolved == i64::MIN {
        return 0;
    }
    if resolved != 0 {
        if unsafe { step(context, 2, 0) } == i64::MIN {
            return 0;
        }
        return resolved as u32;
    }
    let should_log = unsafe { step(context, 3, 0) };
    if should_log == i64::MIN {
        return 0;
    }
    if should_log != 0 {
        unsafe { step(context, 4, 0) };
    }
    0
}

/// # Safety
/// Native callback reads guarded tree/item slots and calls the exact register
/// ABI count helper only after Rust admission.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_count(
    context: *mut c_void,
    ready: bool,
    has_fleetops: bool,
    project: u32,
    team: usize,
    team_count: usize,
    step: Option<Step>,
) -> i32 {
    let Some(step) = step else { return -1 };
    if !ready || !has_fleetops || project == 0 || team >= team_count {
        return -1;
    }
    for op in 0..3 {
        if unsafe { step(context, op, 0) } <= 0 {
            return -1;
        }
    }
    let length = unsafe { step(context, 3, 0) };
    if length <= 0 || u64::from(project) > length as u64 {
        return -1;
    }
    if unsafe { step(context, 4, 0) } <= 0 {
        return -1;
    }
    if unsafe { step(context, 5, 0) } <= 0 {
        return -1;
    }
    let result = unsafe { step(context, 6, 0) };
    if result == i64::MIN {
        -1
    } else {
        result as i32
    }
}

/// # Safety
/// Native callback reads live FO tree pointers and invokes the exact Delphi
/// register-ABI routine; only the null/limit admission lives here.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_recursive_allow(
    context: *mut c_void,
    project: u32,
    team: usize,
    maximum: usize,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return true };
    if project == 0 || team > maximum {
        return true;
    }
    for op in 0..4 {
        if unsafe { step(context, op, 0) } <= 0 {
            return true;
        }
    }
    (unsafe { step(context, 4, 0) }) > 0
}

/// # Safety
/// Native callback holds/copies binding and definition under its C++ lock,
/// then performs native project admission and invokes the Rust rule evaluator.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_symbolic(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    let selected = unsafe { step(context, 0, 0) };
    if selected == i64::MIN {
        return false;
    }
    if selected == 0 {
        return true;
    }
    let required = unsafe { step(context, 1, 0) };
    if required == i64::MIN {
        return false;
    }
    let mut native_missing = false;
    if required != 0 {
        let present = unsafe { step(context, 4, 0) };
        if present == i64::MIN {
            return false;
        }
        native_missing = present == 0;
        if native_missing && unsafe { step(context, 5, 0) } == i64::MIN {
            return false;
        }
    }
    let unsupported = unsafe { step(context, 2, 0) };
    if unsupported == i64::MIN {
        return false;
    }
    if unsupported != 0 {
        if unsafe { step(context, 6, 0) } == i64::MIN {
            return false;
        }
        return true;
    }
    let symbolic = unsafe { step(context, 3, 0) };
    if symbolic == i64::MIN {
        return false;
    }
    (unsafe { step(context, 7, usize::from(symbolic != 0 && !native_missing)) }) != i64::MIN
}

/// # Safety
/// Caller holds the C++ PolicyLockGuard across the selection callbacks.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_symbolic_select(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    let binding = unsafe { step(context, 0, 0) };
    if binding == i64::MIN {
        return false;
    }
    if binding == 0 {
        unsafe { step(context, 5, 0) };
        return false;
    }
    if unsafe { step(context, 1, 0) } == i64::MIN {
        return false;
    }
    let active = unsafe { step(context, 2, 0) };
    if active == i64::MIN {
        return false;
    }
    if active != 0 {
        let ready = unsafe { step(context, 3, 0) };
        if ready == i64::MIN {
            return false;
        }
        if ready == 0 {
            unsafe { step(context, 5, 0) };
            return false;
        }
    }
    let found = unsafe { step(context, 4, 0) };
    if found == i64::MIN {
        return false;
    }
    if found == 0 {
        unsafe { step(context, 5, 0) };
        return false;
    }
    (unsafe { step(context, 6, 0) }) != i64::MIN
}

/// # Safety
/// Native callback owns the original gateway and all native invocation ABIs.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_technology_dispatch(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return true };
    let gateway = unsafe { step(context, 0, 0) };
    if gateway == i64::MIN {
        return false;
    }
    if gateway == 0 {
        return true;
    }
    let readable = unsafe { step(context, 1, 0) };
    if readable == i64::MIN {
        return false;
    }
    if readable == 0 {
        return (unsafe { step(context, 2, 0) }) > 0;
    }
    let symbolic = unsafe { step(context, 3, 0) };
    if symbolic == i64::MIN {
        return false;
    }
    if symbolic != 0 {
        return (unsafe { step(context, 4, 0) }) > 0;
    }
    let legacy = unsafe { step(context, 5, 0) };
    if legacy == i64::MIN {
        return false;
    }
    if legacy != 0 {
        return (unsafe { step(context, 6, 0) }) > 0;
    }
    (unsafe { step(context, 2, 0) }) > 0
}

/// # Safety
/// Caller holds the C++ policy lock and owns map/string storage.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_allocate_binding(
    context: *mut c_void,
    step: Option<Step>,
) -> u32 {
    let Some(step) = step else { return 0 };
    let reset = unsafe { step(context, 0, 0) };
    if reset == i64::MIN {
        return 0;
    }
    if reset != 0 && unsafe { step(context, 1, 0) } == i64::MIN {
        return 0;
    }
    let mut synthetic = unsafe { step(context, 2, 0) };
    if synthetic == i64::MIN {
        return 0;
    }
    loop {
        let occupied = unsafe { step(context, 3, synthetic as usize) };
        if occupied == i64::MIN {
            return 0;
        }
        if occupied == 0 {
            break;
        }
        synthetic = unsafe { step(context, 2, 0) };
        if synthetic == i64::MIN {
            return 0;
        }
    }
    if unsafe { step(context, 4, synthetic as usize) } == i64::MIN {
        return 0;
    }
    let has_tooltip = unsafe { step(context, 5, 0) };
    if has_tooltip == i64::MIN {
        return 0;
    }
    if has_tooltip == 0 {
        return synthetic as u32;
    }
    let missing = unsafe { step(context, 6, 0) };
    if missing == i64::MIN {
        return 0;
    }
    if missing != 0 {
        if unsafe { step(context, 7, synthetic as usize) } == i64::MIN {
            return 0;
        }
    } else {
        let different = unsafe { step(context, 8, 0) };
        if different == i64::MIN {
            return 0;
        }
        if different != 0 {
            let first = unsafe { step(context, 9, 0) };
            if first == i64::MIN {
                return 0;
            }
            if first != 0 && unsafe { step(context, 10, 0) } == i64::MIN {
                return 0;
            }
        }
    }
    synthetic as u32
}

/// # Safety
/// Caller has checked that `code` is readable for at least sixteen bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_localization_length(
    code: *const u8,
    length: usize,
) -> usize {
    if code.is_null() || length < 16 {
        return 0;
    }
    let bytes = unsafe { core::slice::from_raw_parts(code, 16) };
    if bytes[0] == 0x55 && bytes[1] == 0x8b && bytes[2] == 0xec {
        match bytes[3] {
            0x6a => return 5,
            0x83 => return 6,
            0x81 => return 9,
            0x50..=0x57 if (0x50..=0x57).contains(&bytes[4]) => return 5,
            0x8b | 0x89 if bytes[4] & 0xc0 == 0xc0 => return 5,
            _ => {}
        }
    }
    if (0x50..=0x57).contains(&bytes[0])
        && (0x50..=0x57).contains(&bytes[1])
        && (0x50..=0x57).contains(&bytes[2])
        && (bytes[3] == 0x8b || bytes[3] == 0x89)
        && bytes[4] & 0xc0 == 0xc0
    {
        return 5;
    }
    0
}

/// # Safety
/// Native callback checks each address before reading and retains C++ string
/// construction and its exception boundary.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_first_tooltip_line(
    context: *mut c_void,
    class: Option<Classify>,
    step: Option<Step>,
    emit: Option<Emit>,
) -> bool {
    let (Some(class), Some(step), Some(emit)) = (class, step, emit) else {
        return false;
    };
    let mut line = Vec::with_capacity(96);
    for index in 0..255 {
        if unsafe { step(context, 0, index) } == 0 {
            break;
        }
        let value = unsafe { step(context, 1, index) } as u8;
        if value == 0 || value == b'\r' || value == b'\n' {
            break;
        }
        line.push(value);
    }
    send(context, emit, 0, trim(&line, class), 0)
}

/// # Safety
/// Borrowed byte ranges must remain valid until the emitter returns. Native
/// CRT lowercase callback preserves the original suffix comparison.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_format_missing(
    available: bool,
    missing: bool,
    unsupported: bool,
    native_missing: bool,
    gate: Bytes,
    symbolic: Bytes,
    lower: Option<Lower>,
    context: *mut c_void,
    emit: Option<Emit>,
) -> bool {
    let (Some(lower), Some(emit)) = (lower, emit) else {
        return false;
    };
    let (Some(gate), Some(symbolic)) = (view(&gate), view(&symbolic)) else {
        return false;
    };
    let mut text = Vec::new();
    if !available {
        if missing {
            text.extend_from_slice(b"Requires: missing MODULE definition ");
            let mut basename = gate;
            if let Some(index) = gate.iter().rposition(|&b| b == b'\\' || b == b'/') {
                basename = &gate[index + 1..];
            }
            if ends(basename, b".odf", lower) {
                basename = &basename[..basename.len() - 4];
            }
            text.extend_from_slice(basename);
        } else if unsupported {
            text.extend_from_slice(b"Requires: unsupported MODULE technology rule");
        } else {
            if native_missing {
                if symbolic.is_empty() {
                    text.extend_from_slice(b"Required technology");
                } else {
                    text.extend_from_slice(b"Required technology AND (");
                    text.extend_from_slice(symbolic);
                    text.push(b')');
                }
            } else {
                text.extend_from_slice(symbolic);
            }
            if !text.is_empty() {
                let mut result = b"Requires: ".to_vec();
                result.extend_from_slice(&text);
                text = result;
            }
        }
    }
    send(context, emit, 0, &text, 0)
}

/// # Safety
/// Native callbacks retain Delphi calls, C++ storage and diagnostics. A
/// negative sentinel signals a caught C++ exception to the enclosing hook.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_project_capture(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    let found = unsafe { step(context, 0, 0) };
    if found <= 0 {
        return false;
    }
    let gate = unsafe { step(context, 1, 0) };
    if gate <= 0 {
        return false;
    }
    let defined = unsafe { step(context, 2, 0) };
    if defined < 0 {
        return false;
    }
    if defined != 0 {
        return unsafe { step(context, 3, 0) } > 0;
    }
    let mut project = unsafe { step(context, 4, 0) };
    if project < 0 {
        return false;
    }
    if project == 0 {
        project = unsafe { step(context, 5, 0) };
        if project < 0 {
            return false;
        }
    }
    if project != 0 {
        return unsafe { step(context, 6, 0) } > 0;
    }
    (unsafe { step(context, 7, 0) }) > 0
}

/// # Safety
/// Native callback owns signatures, locks, registration, hook patching and
/// diagnostics. It catches C++ exceptions before returning to Rust.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_initialize(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    for op in 0..4 {
        let match_result = unsafe { step(context, op, 0) };
        if match_result < 0 {
            return false;
        }
        if match_result == 0 {
            unsafe { step(context, 4, 0) };
            return false;
        }
    }
    for op in 5..9 {
        if unsafe { step(context, op, 0) } < 0 {
            return false;
        }
    }
    let tree = unsafe { step(context, 9, 0) };
    if tree < 0 {
        return false;
    }
    if tree == 0 {
        unsafe { step(context, 10, 0) };
        return false;
    }
    for op in 11..13 {
        if unsafe { step(context, op, 0) } < 0 {
            return false;
        }
    }
    let evaluator = unsafe { step(context, 13, 0) };
    if evaluator < 0 {
        return false;
    }
    let parser = unsafe { step(context, 14, 0) };
    if parser < 0 {
        return false;
    }
    if evaluator == 0 || parser == 0 {
        unsafe { step(context, 15, 0) };
        return false;
    }
    for op in 16..18 {
        if unsafe { step(context, op, 0) } < 0 {
            return false;
        }
    }
    true
}

/// # Safety
/// Native callback performs signatures, checked hook installations and logs.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_two_hooks(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    let normal = unsafe { step(context, 0, 0) };
    if normal < 0 {
        return false;
    }
    let verbose = unsafe { step(context, 1, 0) };
    if verbose < 0 {
        return false;
    }
    let installed_normal = if normal != 0 {
        let result = unsafe { step(context, 2, 0) };
        if result < 0 {
            return false;
        }
        result != 0
    } else {
        false
    };
    let installed_verbose = if verbose != 0 {
        let result = unsafe { step(context, 3, 0) };
        if result < 0 {
            return false;
        }
        result != 0
    } else {
        false
    };
    if normal == 0 && unsafe { step(context, 4, 0) } < 0 {
        return false;
    }
    if verbose == 0 && unsafe { step(context, 5, 0) } < 0 {
        return false;
    }
    installed_normal || installed_verbose
}

/// # Safety
/// Native callback checks bounds and reads the target bytes and IAT slot at
/// the exact decision point; all address arithmetic is 32-bit on game builds.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_resolve_target(
    target: usize,
    wanted: usize,
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    if target == 0 || wanted == 0 {
        return false;
    }
    if target == wanted {
        return true;
    }
    let Some(step) = step else { return false };
    if unsafe { step(context, 0, 0) } == 0 {
        return false;
    }
    if unsafe { step(context, 1, 0) } == 0xe9 {
        let relative = unsafe { step(context, 2, 0) } as i32;
        return target.wrapping_add(5).wrapping_add(relative as usize) == wanted;
    }
    if unsafe { step(context, 1, 0) } == 0xff && unsafe { step(context, 1, 1) } == 0x25 {
        let address = unsafe { step(context, 3, 0) } as u32 as usize;
        if unsafe { step(context, 4, address) } == 0 {
            return false;
        }
        return (unsafe { step(context, 5, address) }) as usize == wanted;
    }
    false
}

/// # Safety
/// Native callback owns PE layout reads and checked patch_call operations.
/// The section count is reread at each iteration as in the original loader.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_patch_calls(
    context: *mut c_void,
    step: Option<Step>,
) -> usize {
    let Some(step) = step else { return 0 };
    let mut patched = 0usize;
    let mut section = 0usize;
    loop {
        let count = unsafe { step(context, 0, 0) };
        if count < 0 || section >= count as usize {
            break;
        }
        let characteristics = unsafe { step(context, 1, section) };
        if characteristics < 0 {
            break;
        }
        if characteristics & 0x2000_0000 != 0 {
            let virtual_size = unsafe { step(context, 2, section) };
            if virtual_size < 0 {
                break;
            }
            let raw_size = unsafe { step(context, 3, section) };
            if raw_size < 0 {
                break;
            }
            let size = core::cmp::max(virtual_size, raw_size) as usize;
            if unsafe { step(context, 4, size) } < 0 {
                return patched;
            }
            if size >= 5 && unsafe { step(context, 8, size) } != 0 {
                let mut offset = 0usize;
                while offset + 5 <= size {
                    let opcode = unsafe { step(context, 5, offset) };
                    if opcode < 0 {
                        return patched;
                    }
                    if opcode == 0xe8 {
                        let resolves = unsafe { step(context, 6, offset) };
                        if resolves < 0 {
                            return patched;
                        }
                        if resolves != 0 {
                            let success = unsafe { step(context, 7, offset) };
                            if success < 0 {
                                return patched;
                            }
                            if success != 0 {
                                patched += 1;
                                offset += 5;
                                continue;
                            }
                        }
                    }
                    offset += 1;
                }
            }
        }
        section += 1;
    }
    patched
}

/// # Safety
/// Native callback checks mapped object/string addresses immediately before
/// each read, owns C++ strings, and catches allocation exceptions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_button_binding(
    context: *mut c_void,
    maximum: u32,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if unsafe { step(context, 0, 0) } == 0 {
        return false;
    }
    let configuration = unsafe { step(context, 1, 0) };
    if configuration == i64::MIN || configuration == 0 {
        return false;
    }
    let index = unsafe { step(context, 2, 0) };
    if index < 0 || index > i64::from(maximum) {
        return false;
    }
    if unsafe { step(context, 3, 0) } == 0 {
        return false;
    }
    if unsafe { step(context, 4, 0) } == 0 {
        return false;
    }
    if unsafe { step(context, 5, index as usize) } == 0 {
        return false;
    }
    if unsafe { step(context, 6, 0) } <= 0 {
        return false;
    }
    if unsafe { step(context, 7, 0) } == 0 {
        return false;
    }
    if unsafe { step(context, 8, 0) } == 0 {
        return false;
    }
    let mut length = 0usize;
    while length < 512 {
        if unsafe { step(context, 9, length) } == 0 {
            break;
        }
        if unsafe { step(context, 10, length) } == 0 {
            break;
        }
        length += 1;
    }
    if length == 0 || length >= 512 || unsafe { step(context, 9, length) } == 0 {
        return false;
    }
    if unsafe { step(context, 11, length) } == i64::MIN {
        return false;
    }
    let synthetic = unsafe { step(context, 12, 0) };
    if synthetic <= 0 {
        return false;
    }
    if unsafe { step(context, 13, 0) } == 0 {
        return false;
    }
    (unsafe { step(context, 14, 0) }) != i64::MIN
}

/// # Safety
/// Native callback owns tooltip fields, storage, locks and diagnostics.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_compose_generated(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    for op in 0..5 {
        if unsafe { step(context, op, 0) } <= 0 {
            return false;
        }
    }
    if unsafe { step(context, 5, 0) } != 0 {
        return false;
    }
    if unsafe { step(context, 6, 0) } == 0 {
        return false;
    }
    if unsafe { step(context, 7, 0) } != 0 {
        return false;
    }
    (unsafe { step(context, 8, 0) }) > 0
}

/// # Safety
/// Native callback owns C++ map storage, selected component and its pointer.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_compose_component(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if unsafe { step(context, 0, 0) } <= 0 {
        return false;
    }
    let valid = unsafe { step(context, 1, 0) };
    if valid <= 0 {
        if valid == 0 {
            unsafe { step(context, 2, 0) };
        }
        return false;
    }
    let matched = unsafe { step(context, 3, 0) };
    if matched <= 0 {
        if matched == 0 {
            unsafe { step(context, 2, 0) };
        }
        return false;
    }
    if unsafe { step(context, 4, 0) } <= 0 {
        return false;
    }
    let available = unsafe { step(context, 5, 0) };
    if available != 0 {
        if available > 0 {
            unsafe { step(context, 2, 0) };
        }
        return false;
    }
    if unsafe { step(context, 6, 0) } <= 0 {
        return false;
    }
    if unsafe { step(context, 7, 0) } != 0 {
        return false;
    }
    (unsafe { step(context, 8, 0) }) > 0
}

pub type Fetch = unsafe extern "C" fn(*mut c_void, usize, *mut Bytes) -> bool;
pub type Display = unsafe extern "C" fn(*mut c_void, Bytes, *mut Bytes) -> bool;
pub type Accept = unsafe extern "C" fn(*mut c_void, usize, Bytes) -> bool;
fn lower_runtime(bytes: &[u8], lower: Lower) -> Vec<u8> {
    bytes.iter().map(|&byte| unsafe { lower(byte) }).collect()
}
fn normalize_runtime(bytes: &[u8], class: Classify, lower: Lower) -> Vec<u8> {
    let mut value = trim(bytes, class).to_vec();
    for byte in &mut value {
        if *byte == b'/' {
            *byte = b'\\';
        }
    }
    if let Some(index) = value.iter().rposition(|&byte| byte == b'\\') {
        value.drain(..index + 1);
    }
    if !value.is_empty() && !ends(&value, b".odf", lower) {
        value.extend_from_slice(b".odf");
    }
    for byte in &mut value {
        *byte = unsafe { lower(*byte) };
    }
    value
}
fn equal_runtime(left: &[u8], right: &[u8], class: Classify, lower: Lower) -> bool {
    lower_runtime(trim(left, class), lower) == lower_runtime(trim(right, class), lower)
}

/// # Safety
/// Native callback keeps a stable C++ snapshot of tooltip bindings for this
/// call. Display bytes remain valid until the next native callback.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_match_tooltip(
    mode: u32,
    raw: Bytes,
    first: Bytes,
    count: usize,
    class: Option<Classify>,
    lower: Option<Lower>,
    context: *mut c_void,
    fetch: Option<Fetch>,
    display: Option<Display>,
    accept: Option<Accept>,
) -> bool {
    let (Some(class), Some(lower), Some(fetch), Some(display), Some(accept)) =
        (class, lower, fetch, display, accept)
    else {
        return false;
    };
    let (Some(raw), Some(first)) = (view(&raw), view(&first)) else {
        return false;
    };
    const PREFIX: &[u8] = b"autotooltip-";
    for index in 0..count {
        let mut borrowed = Bytes {
            data: core::ptr::null(),
            size: 0,
        };
        if !unsafe { fetch(context, index, &raw mut borrowed) } {
            return false;
        }
        let Some(key) = view(&borrowed) else {
            return false;
        };
        let key_lower = if mode == 0 {
            lower_runtime(key, lower)
        } else {
            lower_runtime(trim(key, class), lower)
        };
        if mode != 0 && raw == key_lower {
            let source = if key_lower.len() > PREFIX.len() && key_lower.starts_with(PREFIX) {
                normalize_runtime(&key[PREFIX.len()..], class, lower)
            } else {
                Vec::new()
            };
            return unsafe { accept(context, index, Bytes::from(&source)) };
        }
        if key_lower.len() <= PREFIX.len() || !key_lower.starts_with(PREFIX) {
            continue;
        }
        let source = normalize_runtime(&key[PREFIX.len()..], class, lower);
        if source.is_empty() {
            continue;
        }
        let mut name = Bytes {
            data: core::ptr::null(),
            size: 0,
        };
        if !unsafe { display(context, Bytes::from(&source), &raw mut name) } {
            return false;
        }
        let Some(name) = view(&name) else {
            return false;
        };
        if !name.is_empty() && !first.is_empty() && equal_runtime(first, name, class, lower) {
            return unsafe { accept(context, index, Bytes::from(&source)) };
        }
    }
    false
}

/// # Safety
/// Original provider gateway is invoked by C++ before this workflow. Native
/// callback owns C++ diagnostics, stream ABI and exception capture.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_provider(
    context: *mut c_void,
    team_count: i32,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if unsafe { step(context, 1, 0) } <= 0 {
        return false;
    }
    if unsafe { step(context, 2, 0) } < 0 {
        return false;
    }
    let team = unsafe { step(context, 3, 0) };
    if team == i64::MIN {
        return false;
    }
    let ready = unsafe { step(context, 4, 0) };
    if ready < 0 {
        return false;
    }
    if ready == 0 {
        if unsafe { step(context, 11, 0) } < 0 {
            return false;
        }
    } else if team < 0 || team >= i64::from(team_count) {
        if unsafe { step(context, 11, 1) } < 0 {
            return false;
        }
    } else {
        let count = unsafe { step(context, 5, 0) };
        if count < 0 {
            return false;
        }
        if count == 0 {
            if unsafe { step(context, 11, 2) } < 0 {
                return false;
            }
        } else {
            let stream = unsafe { step(context, 6, 0) };
            if stream < 0 {
                return false;
            }
            if stream == 0 {
                if unsafe { step(context, 11, 3) } < 0 {
                    return false;
                }
            } else {
                let available = unsafe { step(context, 7, 0) };
                if available < 0 {
                    return false;
                }
                if available != 0 {
                    if unsafe { step(context, 11, 4) } < 0 {
                        return false;
                    }
                } else {
                    let has_text = unsafe { step(context, 8, 0) };
                    if has_text < 0 {
                        return false;
                    }
                    if has_text == 0 {
                        if unsafe { step(context, 11, 5) } < 0 {
                            return false;
                        }
                    } else if unsafe { step(context, 9, 0) } < 0 {
                        return false;
                    }
                }
            }
        }
    }
    (unsafe { step(context, 12, 0) }) >= 0
}

/// # Safety
/// Native callback performs checked import reads and retains the C++ ABI
/// function pointer table; Rust sequences every lookup before validation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_bind_tooltip_api(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if unsafe { step(context, 0, 0) } <= 0 {
        return false;
    }
    for op in 1..5 {
        if unsafe { step(context, op, 0) } < 0 {
            return false;
        }
    }
    if unsafe { step(context, 5, 0) } <= 0 {
        return false;
    }
    if unsafe { step(context, 6, 0) } <= 0 {
        return false;
    }
    (unsafe { step(context, 7, 0) }) > 0
}

/// # Safety
/// Native callback owns relocated signature bytes and checked hook install.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_install_provider(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    let signature = unsafe { step(context, 0, 0) };
    if signature < 0 {
        return false;
    }
    if signature == 0 {
        unsafe { step(context, 1, 0) };
        return false;
    }
    if unsafe { step(context, 2, 0) } <= 0 {
        return false;
    }
    (unsafe { step(context, 3, 0) }) > 0
}

/// # Safety
/// Native callback checks/readbacks the live code and owns checked hook
/// installation. Unsupported prologues are logged at the original point.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_install_localization(
    context: *mut c_void,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    let length = unsafe { step(context, 0, 0) };
    if length < 0 {
        return false;
    }
    if length == 0 || length > 16 {
        unsafe { step(context, 1, 0) };
        return false;
    }
    (unsafe { step(context, 2, length as usize) }) > 0
}

/// # Safety
/// Native callback owns localization manager ABI, C++ map/string storage and
/// catches exceptions before returning to Rust.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_c_buildyard_localization_lookup(
    context: *mut c_void,
    team_max: i32,
    step: Option<Step>,
) -> bool {
    let Some(step) = step else { return false };
    if unsafe { step(context, 1, 0) } <= 0 {
        return false;
    }
    if unsafe { step(context, 2, 0) } <= 0 {
        return false;
    }
    let team = unsafe { step(context, 3, 0) };
    if team < 0 || team > i64::from(team_max) {
        return false;
    }
    if unsafe { step(context, 4, 0) } != 0 {
        return false;
    }
    if unsafe { step(context, 5, 0) } <= 0 {
        return false;
    }
    (unsafe { step(context, 6, 0) }) > 0
}
