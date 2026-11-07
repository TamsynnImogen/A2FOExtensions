// Included inside the BuildYard policy's private namespace. Native ABI ownership
// stays here; the portable XML/predicate policy has no engine addresses.
struct ActiveModuleTree {
    bool ready = false;
    bool failed = false;
    std::unordered_map<std::string, ModuleTechnologyDefinition> definitions;
};
std::array<ActiveModuleTree, 9> g_active_trees;
bool g_require_active_tree = false;
bool g_team_predicates_ready = false;
bool g_native_tree_observers_ready = false;

// Match entire verified helpers, rebasing only operands named by PE relocations.
template <std::size_t N, std::size_t R>
bool relocated_signature(HMODULE module, std::uintptr_t rva,
                         const std::array<std::uint8_t, N>& bytes,
                         const std::array<std::uint32_t, R>& relocations,
                         std::uintptr_t preferred_base) {
    auto expected = bytes;
    struct Context {
        HMODULE module;
        std::uintptr_t rva;
        std::array<std::uint8_t, N>& expected;
        const std::array<std::uint32_t, R>& relocations;
        std::uintptr_t preferred_base;
        std::exception_ptr error{};
    } context{module,rva,expected,relocations,preferred_base};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t index) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try { switch(operation) {
        case 0:return c.relocations[index];
        case 1:{const auto offset=c.relocations[index];
            std::uint32_t value;
            std::memcpy(&value,c.expected.data()+offset,4);
            value+=reinterpret_cast<std::uintptr_t>(c.module)-c.preferred_base;
            std::memcpy(c.expected.data()+offset,&value,4);
            return 0;}
        case 2:return signature_matches(c.module,c.rva,c.expected);
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    const bool result=a2fo_rs_c_buildyard_relocated(&context,R,N,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

bool validate_team_predicates() {
    using namespace buildyard_tree_signatures;
    struct Context { std::exception_ptr error{}; } context{};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t valid) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation){
        case 0:return relocated_signature(g_armada,kTeamRva,kTeamBytes,
            kTeamRelocations,0x400000);
        case 1:return relocated_signature(g_armada,kRaceRva,kRaceBytes,
            kRaceRelocations,0x400000);
        case 2:return relocated_signature(g_armada,kAllyRva,kAllyBytes,
            kAllyRelocations,0x400000);
        case 3:return relocated_signature(g_armada,kEnemyRva,kEnemyBytes,
            kEnemyRelocations,0x400000);
        case 4:return relocated_signature(g_fleet_ops,kPeerActiveRva,
            kPeerActiveBytes,kPeerActiveRelocations,0x5a800000);
        case 5:log_line(valid
            ? "MODULE team predicates verified: Armada+96340 cdecl(team)->Team*, +8B100 cdecl(name)->Race*, +97250/+972D0 thiscall(Team*,other)->AL; FO+1DF1D8 EAX=team->AL active"
            : "MODULE team predicate signature mismatch; race/ally/enemy conditions fail closed");
            return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    const bool result=a2fo_rs_c_buildyard_validate_team(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

void* module_native_team(int team) noexcept {
    if (!a2fo_rs_c_buildyard_native_team_admit(g_team_predicates_ready,team))
        return nullptr;
    using GetTeam = void* (__cdecl *)(int);
    return reinterpret_cast<GetTeam>(at(g_armada,
        buildyard_tree_signatures::kTeamRva))(team);
}

bool module_race_matches(std::string_view race, int team) {
    struct Context {
        std::string_view race;
        int team;
        void* player=nullptr;
        void* faction=nullptr;
        void* required=nullptr;
        std::string name{};
        std::exception_ptr error{};
    } context{race,team};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation){
        case 0:c.player=module_native_team(c.team);return c.player!=nullptr;
        case 1:c.faction=read_live_at<void*>(c.player,0x244,nullptr);
            return c.faction!=nullptr;
        case 2:return readable_range(static_cast<std::uint8_t*>(c.faction)+0x448,4);
        case 3:{using GetRace=void* (__cdecl *)(const char*);
            c.name=std::string(c.race);
            c.required=reinterpret_cast<GetRace>(at(g_armada,
                buildyard_tree_signatures::kRaceRva))(c.name.c_str());
            return c.required!=nullptr;}
        case 4:return readable_range(static_cast<std::uint8_t*>(c.required)+0x448,4);
        case 5:return read_live_at<int>(c.faction,0x448,-1)==
                      read_live_at<int>(c.required,0x448,-2);
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    const bool result=a2fo_rs_c_buildyard_race(&context,race.empty(),step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

bool module_eligible_peer(int owner, int other, bool ally) noexcept {
    struct Context {
        int owner;
        int other;
        bool ally;
        void* self=nullptr;
        void* peer=nullptr;
        std::exception_ptr error{};
    } context{owner,other,ally};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation){
        case 0:c.self=module_native_team(c.owner);return 0;
        case 1:c.peer=module_native_team(c.other);return 0;
        case 2:return readable_range(c.self,0x194);
        case 3:return readable_range(c.peer,0x194);
        case 4:return read_live_at<int>(c.self,0x16c,-1)==c.owner;
        case 5:{using Relation=bool (__attribute__((thiscall)) *)(void*,int);
            const auto relation=reinterpret_cast<Relation>(at(g_armada,c.ally
                ? buildyard_tree_signatures::kAllyRva
                : buildyard_tree_signatures::kEnemyRva));
            return relation(c.self,c.other);}
        case 6:{using Active=bool (__attribute__((regparm(1))) *)(int);
            return reinterpret_cast<Active>(at(g_fleet_ops,
                buildyard_tree_signatures::kPeerActiveRva))(c.other);}
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    const bool result=a2fo_rs_c_buildyard_peer(&context,
        g_team_predicates_ready,owner,other,step);
    if(context.error)std::terminate();
    return result;
}

bool copy_native_text(const char* source,std::size_t maximum,
                      std::string& output) {
    struct Context {
        const char* source;
        std::size_t maximum;
        std::string& output;
        const char* current=nullptr;
        MEMORY_BASIC_INFORMATION info{};
        std::size_t count=0;
        std::exception_ptr error{};
    } context{source,maximum,output};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t arg) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation){
        case 0:c.output.clear();return 0;
        case 1:return c.source!=nullptr;
        case 2:c.current=c.source+arg;c.info={};
            return VirtualQuery(c.current,&c.info,sizeof(c.info))==
                sizeof(c.info)&&c.info.State==MEM_COMMIT&&
                !(c.info.Protect&(PAGE_GUARD|PAGE_NOACCESS));
        case 3:{const auto end=reinterpret_cast<std::uintptr_t>(c.info.BaseAddress)
                +c.info.RegionSize;
            c.count=std::min(c.maximum-arg,static_cast<std::size_t>(
                end-reinterpret_cast<std::uintptr_t>(c.current)));
            return c.count;}
        case 4:{const char* nul=static_cast<const char*>(
                std::memchr(c.current,0,c.count));
            c.output.append(c.current,nul?static_cast<std::size_t>(nul-c.current)
                :c.count);
            return nul!=nullptr;}
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    const bool result=a2fo_rs_c_buildyard_copy_text(&context,maximum,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

// Called only for bytes the native XML reader has actually obtained through
// FOFS. Native recursion determines include/parentInclude/archive selection.
void capture_module_document(void* tree,const char* filename,
                             const char* text,int parent_depth) noexcept {
    const int team=read_live_at<int>(tree,8,-1);
    if(team<0||team>=9)return;
    try {
        struct Context {
            int team;
            const char* filename;
            const char* text;
            int parent_depth;
            std::string source{};
            std::string xml{};
            std::optional<module_technology_policy::ParseResult> parsed{};
            std::exception_ptr error{};
        } context{team,filename,text,parent_depth};
        const auto step=+[](void* opaque,std::uint32_t operation,
                            std::size_t) noexcept ->std::int64_t {
            auto& c=*static_cast<Context*>(opaque);
            try {switch(operation){
            case 0:return copy_native_text(c.filename,4096,c.source);
            case 1:return copy_native_text(c.text,32u*1024u*1024u,c.xml);
            case 2:{PolicyLockGuard lock;
                g_active_trees[c.team].failed=true;
                log_line("MODULE active XML capture failed: unreadable or over-limit text; team="+
                    std::to_string(c.team));
                return 0;}
            case 3:c.parsed.emplace(module_technology_policy::parse_document(
                c.xml,c.source));return 0;
            case 4:{PolicyLockGuard lock;
                struct Publish {
                    Context& outer;
                    std::unordered_map<std::string,ModuleTechnologyDefinition>::iterator existing{};
                    std::size_t index=0;
                    std::exception_ptr error{};
                } publish{c};
                const auto publish_step=+[](void* value,std::uint32_t op,
                                            std::size_t arg) noexcept ->std::int64_t {
                    auto& state=*static_cast<Publish*>(value);
                    auto& c=state.outer;
                    try {switch(op){
                    case 0:return static_cast<bool>(*c.parsed);
                    case 1:g_active_trees[c.team].failed=true;
                        log_line("MODULE active XML rejected: "+c.source+": "+
                            c.parsed->error);return 0;
                    case 2:{state.index=arg;
                        auto& definition=c.parsed->definitions[arg];
                        g_module_technologies[definition.key]=definition;
                        auto result=g_active_trees[c.team].definitions.emplace(
                            definition.key,definition);
                        state.existing=result.first;
                        return result.second;}
                    case 3:log_line("MODULE native XML captured: "+c.source+
                        " parentDepth="+std::to_string(c.parent_depth)+
                        " definitions="+std::to_string(c.parsed->definitions.size()));
                        return 0;
                    case 4:{auto& definition=c.parsed->definitions[state.index];
                        auto& existing=state.existing->second;
                        existing.requirements.insert(existing.requirements.end(),
                            definition.requirements.begin(),
                            definition.requirements.end());return 0;}
                    case 5:return c.parsed->definitions[state.index].unsupported;
                    case 6:{auto& definition=c.parsed->definitions[state.index];
                        auto& existing=state.existing->second;
                        existing.unsupported=true;
                        existing.reason=definition.reason;return 0;}
                    default:return 0;
                    }}catch(...){state.error=std::current_exception();return INT64_MIN;}
                };
                a2fo_rs_c_buildyard_capture_publish(&publish,
                    c.parsed->definitions.size(),c.team,publish_step);
                if(publish.error)std::rethrow_exception(publish.error);
                return 0;}
            default:return 0;
            }}catch(...){c.error=std::current_exception();return INT64_MIN;}
        };
        a2fo_rs_c_buildyard_capture(&context,team,step);
        if(context.error)std::rethrow_exception(context.error);
    } catch (...) {
        PolicyLockGuard lock;
        g_active_trees[team].failed=true;
        log_line("MODULE active XML capture failed: C++ exception");
    }
}

extern "C" const char* __cdecl a2fo_buildyard_tree_text_handler(void* file, void* frame) noexcept {
    using NativeText = const char* (__attribute__((regparm(1))) *)(void*);
    const char* text = reinterpret_cast<NativeText>(
        at(g_fleet_ops, buildyard_tree_signatures::kFileTextRva))(file);
    struct Context {
        void* frame;
        const char* text;
        std::exception_ptr error{};
    } context{frame,text};
    const auto step=+[](void* opaque,std::uint32_t,
                        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            // Full XML-reader signature verifies these EBP-relative locals.
            auto* base=static_cast<std::uint8_t*>(c.frame);
            capture_module_document(*reinterpret_cast<void**>(base-8),
                *reinterpret_cast<const char**>(base-0x1c),c.text,
                *reinterpret_cast<int*>(base-0x20));
            return 0;
        }catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    a2fo_rs_c_buildyard_tree_text(&context,g_native_tree_observers_ready,step);
    if(context.error)std::terminate();
    return text; // borrowed native storage; no mutation, ownership or delayed use
}
extern "C" void a2fo_buildyard_tree_text_bridge();

extern "C" {
void* g_a2fo_buildyard_tree_dispatch = nullptr;
void __attribute__((regparm(3))) a2fo_buildyard_tree_root_bridge(void*, const char*, int);
void __cdecl a2fo_buildyard_tree_begin(void* tree,
    const char* filename,int) noexcept {
    const int team=read_live_at<int>(tree,8,-1);
    if(team<0||team>=9)return;
    try {
        struct Context {
            int team;
            const char* filename;
            std::exception_ptr error{};
        } context{team,filename};
        const auto step=+[](void* opaque,std::uint32_t operation,
                            std::size_t) noexcept ->std::int64_t {
            auto& c=*static_cast<Context*>(opaque);
            try {if(operation==0){
                PolicyLockGuard lock;
                const auto locked_step=+[](void* value,std::uint32_t op,
                                           std::size_t) noexcept ->std::int64_t {
                    auto& c=*static_cast<Context*>(value);
                    try {switch(op){
                    case 0:g_active_trees={};return 0;
                    case 1:g_active_trees[c.team]=ActiveModuleTree{};return 0;
                    case 2:g_logged_tooltip_appends.clear();return 0;
                    case 3:{std::string name;
                        if(!copy_native_text(c.filename,4096,name))name="<unreadable>";
                        log_line("MODULE selected native tree loading: "+name+
                            "; previous definitions cleared");return 0;}
                    default:return 0;
                    }}catch(...){c.error=std::current_exception();return INT64_MIN;}
                };
                a2fo_rs_c_buildyard_begin_locked(&c,c.team,locked_step);
                if(c.error)std::rethrow_exception(c.error);
            }
            return 0;
            }catch(...){c.error=std::current_exception();return INT64_MIN;}
        };
        a2fo_rs_c_buildyard_begin(&context,team,step);
        if(context.error)std::rethrow_exception(context.error);
    }catch(...){
        PolicyLockGuard lock;
        g_active_trees[team].failed=true;
    }
}

void __cdecl a2fo_buildyard_tree_commit(void* tree) noexcept {
    const int team = read_live_at<int>(tree, 8, -1);
    struct Context {int team;std::exception_ptr error{};} context{team};
    const auto step=+[](void* opaque,std::uint32_t,
                        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {PolicyLockGuard lock;g_active_trees[c.team].ready=true;return 0;}
        catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    a2fo_rs_c_buildyard_tree_commit(&context,team,step);
    if(context.error)std::terminate();
}
}

bool install_native_tree_observers() {
    using namespace buildyard_tree_signatures;
    constexpr std::array<std::uint8_t, 5> root_call{
        0xe8, 0x86, 0xeb, 0xff, 0xff};
    constexpr std::array<std::uint8_t, 5> text_call{
        0xe8, 0x77, 0x39, 0x0c, 0x00};
    struct Context {
        const std::array<std::uint8_t,5>& root_call;
        const std::array<std::uint8_t,5>& text_call;
        std::exception_ptr error{};
    } context{root_call,text_call};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation){
        case 0:return relocated_signature(g_fleet_ops,kTreeRootRva,
            kTreeRootBytes,kTreeRootRelocations,0x5a800000);
        case 1:return relocated_signature(g_fleet_ops,kTreeDispatchRva,
            kTreeDispatchBytes,kTreeDispatchRelocations,0x5a800000);
        case 2:return relocated_signature(g_fleet_ops,kXmlReaderRva,
            kXmlReaderBytes,kXmlReaderRelocations,0x5a800000);
        case 3:return relocated_signature(g_fleet_ops,kFileTextRva,
            kFileTextBytes,kFileTextRelocations,0x5a800000);
        case 4:return signature_matches(g_fleet_ops,0x1206f9,c.root_call);
        case 5:return signature_matches(g_fleet_ops,0x11fc20,c.text_call);
        case 6:log_line("MODULE native tree reader signature mismatch; observers not installed");
            return 0;
        case 7:g_a2fo_buildyard_tree_dispatch=at(g_fleet_ops,kTreeDispatchRva);
            return 0;
        case 8:return g_api->patch_call(at(g_fleet_ops,0x11fc20),
            reinterpret_cast<void*>(&a2fo_buildyard_tree_text_bridge),
            c.text_call.data(),c.text_call.size());
        case 9:return g_api->patch_call(at(g_fleet_ops,0x1206f9),
            reinterpret_cast<void*>(&a2fo_buildyard_tree_root_bridge),
            c.root_call.data(),c.root_call.size());
        case 10:log_line("MODULE native tree call patch failed; partial observer is pass-through, gameplay hooks not installed");
            return 0;
        case 11:g_native_tree_observers_ready=true;
            log_line("MODULE selected-tree observers enabled: FO+1206F9 EAX=tree EDX=Delphi filename ECX=parentDepth; FO+11FC20 native borrowed XML text capture");
            return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    const bool result=a2fo_rs_c_buildyard_install_tree(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}
