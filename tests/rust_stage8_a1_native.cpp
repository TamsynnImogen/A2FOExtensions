// Actual whole A1 module; native engine/GUI paths are uncalled and trapped.
#include A1_SOURCE
#include <new>
#include <cstdlib>
#include <exception>
static int allocation_budget=-1;
__attribute__((noinline)) void* operator new(std::size_t n){
    if(allocation_budget==0){allocation_budget=-1;throw std::bad_alloc();}
    if(allocation_budget>0)--allocation_budget;
    if(void* p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
__attribute__((noinline)) void operator delete(void* p)noexcept{std::free(p);}
__attribute__((noinline)) void operator delete(void* p,std::size_t)noexcept{std::free(p);}
__attribute__((noinline)) void* operator new[](std::size_t n){return ::operator new(n);}
__attribute__((noinline)) void operator delete[](void* p)noexcept{::operator delete(p);}
__attribute__((noinline)) void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
static void hex(const std::string& value){for(unsigned char ch:value)std::printf("%02x",ch);}
static std::uint64_t digest=14695981039346656037ull;
static void hash(const std::string& value){for(unsigned char ch:value){digest^=ch;digest*=1099511628211ull;}digest^=0x100;digest*=1099511628211ull;}
static const char* fixture_roots[2]{};
static std::uint32_t fixture_root_count{};
static std::uint32_t fixture_root_reads{};
static std::uint32_t A2FO_CALL fixture_count(){return fixture_root_count;}
static const char* A2FO_CALL fixture_root(std::uint32_t index){++fixture_root_reads;return fixture_roots[index];}
int main(int argc,char**argv){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    if(argc==2&&std::strcmp(argv[1],"bool-terminate")==0){
        const std::string text(90,'x');
        std::set_terminate(+[](){
            const auto error=std::current_exception();
            try{if(error)std::rethrow_exception(error);}catch(const std::bad_alloc&){std::puts("terminate:bad_alloc");std::fflush(stdout);ExitProcess(77);}catch(...){}
            std::puts("terminate:other");std::fflush(stdout);ExitProcess(78);
        });
        bool output=false;allocation_budget=0;parse_bool_config_value(text,output);return 79;
    }
    const std::vector<std::string> samples={"","a","// a\nb","/* a */b","/*x\r\ny","a//b\rc","a/*/b","/**/","/* */ //end","\"//literal\"",std::string("a\0/*x*/b",9)};
    for(const auto& input:samples){std::printf("comments:");hex(input);std::putchar(':');hex(strip_c_comments(input));std::puts("");}
    const char alphabet[]={'/','*','\n','\r','x','"','\0'};
    std::size_t cases=0;
    for(unsigned len=0;len<=5;++len){std::size_t limit=1;for(unsigned i=0;i<len;++i)limit*=sizeof(alphabet);
        for(std::size_t row=0;row<limit;++row){std::string input(len,' ');auto value=row;for(unsigned i=0;i<len;++i){input[i]=alphabet[value%sizeof(alphabet)];value/=sizeof(alphabet);}hash(strip_c_comments(input));++cases;}}
    std::printf("exhaustive-comments:%zu:%016llx\n",cases,static_cast<unsigned long long>(digest));
    const std::vector<std::string> tokens={""," "," A_B ","\" TRUE \"","'ON'","' no '"," \'x\" ","foo = 3","@a_b !!!=","=","a.b = ",std::string("A\0B",3),std::string(140,'Q')};
    for(const auto& input:tokens){
        for(std::size_t capacity=0;capacity<12;++capacity){char out[16];std::memset(out,'~',sizeof(out));normalize_command_token(input.c_str(),out,capacity);std::printf("token:%zu:",capacity);hex(std::string(out,sizeof(out)));std::puts("");}
        std::string alias=input;alias.resize(alias.size()+16,'~');normalize_command_token(alias.c_str(),alias.data(),alias.size());std::printf("alias:");hex(alias);std::puts("");
        for(std::size_t at=0;at<=input.size();++at){std::printf("assignment:%zu:",at);hex(assignment_identifier(input,at));std::puts("");}
    }
    char nullout[8];std::memset(nullout,'~',sizeof(nullout));normalize_command_token(nullptr,nullout,sizeof(nullout));std::printf("null-token:");hex(std::string(nullout,sizeof(nullout)));std::puts("");
    normalize_command_token(nullptr,nullptr,9);normalize_command_token("text",nullptr,0);
    for(const char* word:{"1","true","TRUE","yes","ON","enabled","0","false","NO","off","disabled","x","true x","","  \t\r\n"})for(const char* pre:{""," \t"})for(const char* post:{"","\r\n "})for(bool seed:{false,true}){
        const std::string input=std::string(pre)+word+post;bool output=seed;const bool ok=parse_bool_config_value(input,output);std::printf("bool:");hex(input);std::printf(":%u:%u:%u\n",seed,ok,output);
    }
    const std::string big=std::string(80,'a')+"/* hidden */"+std::string(80,'b');
    for(int budget=0;budget<8;++budget){bool threw=false;std::string output;allocation_budget=budget;try{output=strip_c_comments(big);}catch(const std::bad_alloc&){threw=true;}allocation_budget=-1;std::printf("alloc:%d:%u:",budget,threw);hex(output);std::puts("");}
    SYSTEM_INFO info{};GetSystemInfo(&info);auto* region=static_cast<char*>(VirtualAlloc(nullptr,info.dwPageSize*2,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!region)return 3;
    DWORD old=0;
    if(!VirtualProtect(region+info.dwPageSize,info.dwPageSize,PAGE_NOACCESS,&old))return 4;
    auto* edge=region+info.dwPageSize-1;*edge=0;normalize_command_token(edge,nullout,sizeof(nullout));std::printf("guard-empty:");hex(std::string(nullout,sizeof(nullout)));std::puts("");
    *edge='X';normalize_command_token(edge,edge,1);std::printf("guard-alias:%u\n",static_cast<unsigned char>(*edge));VirtualFree(region,0,MEM_RELEASE);

    for(const auto& input:tokens){
        for(std::size_t cap=0;cap<12;++cap){char output[16];std::memset(output,'~',sizeof(output));copy_printable_string(input.c_str(),output,cap);std::printf("printable:%zu:",cap);hex(std::string(output,sizeof(output)));std::puts("");}
        std::array<char,64> race{};race.fill('~');normalize_race_name(input.c_str(),&race);std::printf("race:");hex(std::string(race.data(),race.size()));std::puts("");
        race.fill('~');std::memcpy(race.data(),input.data(),std::min(input.size(),race.size()));normalize_race_name(race.data(),&race);std::printf("race-alias:");hex(std::string(race.data(),race.size()));std::puts("");
    }
    for(unsigned ch=0;ch<256;++ch){const char input[]={32,static_cast<char>(ch),32,0};std::array<char,64> race{};normalize_race_name(input,&race);char output[8];std::memset(output,'~',sizeof(output));copy_printable_string(input,output,sizeof(output));std::printf("byte-normalize:%u:",ch);hex(std::string(race.data(),race.size()));std::putchar(':');hex(std::string(output,sizeof(output)));std::puts("");}
    normalize_race_name(reinterpret_cast<const char*>(1),nullptr);
    copy_printable_string(reinterpret_cast<const char*>(1),nullptr,SIZE_MAX);
    std::array<char,64> nullrace{};nullrace.fill('~');normalize_race_name(nullptr,&nullrace);std::printf("race-null:");hex(std::string(nullrace.data(),nullrace.size()));std::puts("");
    {
        auto* memory=static_cast<char*>(VirtualAlloc(nullptr,info.dwPageSize*2,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        if(!memory)return 5;
        DWORD previous=0;if(!VirtualProtect(memory+info.dwPageSize,info.dwPageSize,PAGE_NOACCESS,&previous))return 6;
        auto* edge=memory+info.dwPageSize-1;*edge='X';char output[8];std::memset(output,'~',sizeof(output));copy_printable_string(edge,output,sizeof(output));std::printf("printable-guard:");hex(std::string(output,sizeof(output)));std::puts("");VirtualFree(memory,0,MEM_RELEASE);
    }
    for(const char* word:{static_cast<const char*>(nullptr),"","norace","NoRace.ODF","foo/norace.odf","C:\\x\\NORACE","norace.odfx","/"})
        std::printf("neutral:%s:%u\n",word?word:"(null)",is_neutral_race_odf(word));
    for(const char* word:{static_cast<const char*>(nullptr),"","scout.odf","C:\\x\\SCOUT.ODF","scout.odfx","a/scout.odf/"})
        std::printf("scout:%s:%u\n",word?word:"(null)",command_info_path_is_scout(word));
    {
        auto* memory=static_cast<char*>(VirtualAlloc(nullptr,info.dwPageSize*2,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        if(!memory)return 7;
        DWORD previous=0;if(!VirtualProtect(memory+info.dwPageSize,info.dwPageSize,PAGE_NOACCESS,&previous))return 8;
        auto* edge=memory+info.dwPageSize-260;std::memset(edge,'x',260);
        std::printf("scout-guard-no-nul:%u\n",command_info_path_is_scout(edge));
        edge[259]=0;std::printf("scout-guard-tail-nul:%u\n",command_info_path_is_scout(edge));
        VirtualFree(memory,0,MEM_RELEASE);
    }
    for(const char* left:{static_cast<const char*>(nullptr),"","A","AbC","a/b"})for(const char* right:{static_cast<const char*>(nullptr),"","a","aBc","A/B"})
        std::printf("ascii:%s:%s:%u\n",left?left:"(null)",right?right:"(null)",ascii_equals_ignore_case(left,right));
    for(const char* source:{static_cast<const char*>(nullptr),"","AlphaConstructionRig","zClonDominion","abc"})
        for(const char* token:{static_cast<const char*>(nullptr),"","alpha","RIG","dominion","zzz"})
            std::printf("contains:%s:%s:%u\n",source?source:"(null)",token?token:"(null)",contains_ci_substring(source,token));
    char alias[]="DoMiNiOn";std::printf("contains-alias:%u\n",contains_ci_substring(alias,alias));
    const std::string long_name(140,'M');
    for(const char* name:{static_cast<const char*>(nullptr),"","mdmoon","MDMOON12.ODF","x/mdmoon001.odf","mdmoon1a.odf",long_name.c_str()})
        for(const char* family:{static_cast<const char*>(nullptr),"","mdmoon","mmooninf"})
            std::printf("numbered:%s:%s:%u\n",name?name:"(null)",family?family:"(null)",matches_numbered_odf_family(name,family));
    char numbered_alias[]="mdmoon22.odf";
    std::printf("numbered-alias:%u\n",matches_numbered_odf_family(numbered_alias,numbered_alias));
    for(const char* root:{static_cast<const char*>(nullptr),"","C:\\mods","C:\\mods\\","/tmp/root/"})
        for(const char* name:{"a1compat.ini","","x\\y"}){
            const auto joined=join_path(root,name);std::printf("join:%s:%s:",root?root:"(null)",name);hex(joined);std::puts("");
        }
    char path_alias[]="D:\\foo";
    std::printf("join-alias:");hex(join_path(path_alias,path_alias));std::puts("");
    {
        const std::string long_root(90,'x');bool threw=false;allocation_budget=0;
        try{(void)join_path(long_root.c_str(),"a1compat.ini");}catch(const std::bad_alloc&){threw=true;}
        allocation_budget=-1;std::printf("join-alloc:%u\n",threw);
    }
    std::printf("chain-null:%u\n",enabled_in_mod_chain());
    A2FO_ModuleApi api{};api.extension_root_count=&fixture_count;api.extension_root=&fixture_root;
    g_api=&api;fixture_root_count=0;fixture_root_reads=0;
    const bool empty_chain=enabled_in_mod_chain();
    std::printf("chain-empty:%u:%u\n",empty_chain,fixture_root_reads);
    fixture_root_count=4097;fixture_root_reads=0;
    const bool capped=enabled_in_mod_chain();
    std::printf("chain-cap:%u:%u\n",capped,fixture_root_reads);
    char temp[MAX_PATH]{};char directory[MAX_PATH]{};
    if(!GetTempPathA(sizeof(temp),temp)||!GetTempFileNameA(temp,"a1t",0,directory))return 9;
    DeleteFileA(directory);if(!CreateDirectoryA(directory,nullptr))return 10;
    const auto marker=join_path(directory,"a1compat.ini");
    HANDLE file=CreateFileA(marker.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return 11;
    CloseHandle(file);
    fixture_roots[0]="Z:\\missing-a1-root";fixture_roots[1]=directory;
    fixture_root_count=2;fixture_root_reads=0;
    const bool found=enabled_in_mod_chain();
    std::printf("chain-found:%u:%u\n",found,fixture_root_reads);
    DeleteFileA(marker.c_str());RemoveDirectoryA(directory);g_api=nullptr;
    std::puts("A1 text policy: PASS");
}
