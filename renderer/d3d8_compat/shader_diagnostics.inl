// Private observer diagnostics. Included after Call; no native ownership changes.
struct ShaderTokens {
    const char* status = "absent";
    std::vector<std::uint32_t> words;
};
ShaderTokens read_shader_tokens(const DWORD* pointer, bool declaration) {
    ShaderTokens captured;
    if (!pointer) return captured;
    captured.status = "word_limit";
    constexpr size_t limit = 4096;
    const auto start = reinterpret_cast<std::uintptr_t>(pointer);
    // Read only one additional word when the parser requests more input. In
    // particular, do not read beyond a valid END at the edge of a guard page.
    for (size_t index = 0; index < limit; ++index) {
        if (index > (UINTPTR_MAX - start) / sizeof(DWORD)) {
            captured.status = "unreadable"; break;
        }
        DWORD word = 0;
        if (!copy_value(reinterpret_cast<const DWORD*>(start + index * sizeof(DWORD)), word)) {
            captured.status = "unreadable"; break;
        }
        captured.words.push_back(static_cast<std::uint32_t>(word));
        const auto parsed = declaration
            ? shaders::parse_d3d8_vertex_declaration(captured.words.data(), captured.words.size())
            : shaders::parse_d3d8_shader_bytecode(captured.words.data(), captured.words.size());
        switch (parsed.status) {
            case shaders::TokenParseStatus::complete:
                captured.status = "complete";
                captured.words.resize(parsed.consumed_words);
                return captured;
            case shaders::TokenParseStatus::truncated: break;
            case shaders::TokenParseStatus::unsupported:
                captured.status = "unsupported"; captured.words.clear(); return captured;
            case shaders::TokenParseStatus::invalid:
                captured.status = "invalid"; captured.words.clear(); return captured;
        }
    }
    // Partial payloads must never be mistaken for complete shader evidence.
    captured.words.clear();
    return captured;
}
void append_shader_tokens(std::string& text, const ShaderTokens& tokens) {
    text += "{\"status\":\""; text += tokens.status; text += "\",\"words\":[";
    char word[16];
    for (size_t index = 0; index < tokens.words.size(); ++index) {
        if (index) text += ',';
        std::snprintf(word,sizeof(word),"\"0x%08lx\"",static_cast<unsigned long>(tokens.words[index]));
        text += word;
    }
    text += "]}";
}
void capture_shader(const std::shared_ptr<Record>& device, const DWORD* declaration,
                    const DWORD* byte_code, const DWORD* shader, DWORD usage,
                    bool vertex) noexcept {
    if (!runtime->trace) return;
    try {
        unsigned capture = 0, frame = 0;
        {
            Lock lock;
            if (runtime->shader_captures >= 64) {
                if (!runtime->shader_limit_reported) {
                    open_trace_locked();
                    emit_locked("{\"event\":\"shader_capture_limit\",\"limit\":64}\n");
                    runtime->shader_limit_reported = true;
                }
                return;
            }
            capture = ++runtime->shader_captures;
            frame = runtime->frames;
        }
        DWORD handle = 0;
        if (!copy_value(shader, handle)) return;
        const auto decl = vertex ? read_shader_tokens(declaration,true) : ShaderTokens{};
        auto code = read_shader_tokens(byte_code,false);
        if (!code.words.empty() && ((code.words.front() >> 16) != (vertex ? 0xfffeu : 0xffffu))) {
            code.status = "wrong_stage"; code.words.clear();
        }
        char header[300];
        std::snprintf(header,sizeof(header),"{\"event\":\"shader\",\"capture\":%u,\"frame\":%u,\"object\":%u,\"stage\":\"%s\",\"handle\":%lu,\"usage\":%lu,\"declaration\":",
            capture,frame,device ? device->id : 0,vertex ? "vertex" : "pixel",handle,usage);
        std::string text = header;
        append_shader_tokens(text,decl);
        text += ",\"function\":"; append_shader_tokens(text,code); text += "}\n";
        Lock lock;
        open_trace_locked();
        emit_record_locked(text.data(),text.size());
        if (runtime->file) std::fflush(runtime->file);
    } catch (...) {} // Parsing, allocation and I/O never replace native results.
}
