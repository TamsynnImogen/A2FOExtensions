#include "hybrid_production.hpp"
#include "build_submenu_config.hpp"
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace a2fo;
namespace submenu = a2fo::build_submenu;

static std::string hex(const std::string& s) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (unsigned char b : s) {
        out += digits[b >> 4];
        out += digits[b & 15];
    }
    return out;
}
static void print_job(const ProductionJob* job) {
    if (!job) { std::cout << "-"; return; }
    auto bits = [](float value) {
        std::uint32_t result;
        std::memcpy(&result, &value, sizeof result);
        return result;
    };
    std::cout << job->queue_id << ',' << job->project_id << ','
              << unsigned(job->method) << ',' << job->active << ','
              << job->placement.present << ',' << bits(job->placement.x) << ','
              << bits(job->placement.y) << ',' << bits(job->placement.z) << ','
              << bits(job->placement.facing);
}
static void queue_state(const HybridProductionQueue& q) {
    std::cout << " state " << q.size() << ',' << q.capacity() << ','
              << q.empty() << ',' << q.evolution_barrier() << ','
              << q.accepts_new_jobs() << " active=";
    print_job(q.active_job());
    for (const auto& job : q.jobs()) {
        std::cout << " job=";
        print_job(&job);
    }
    std::cout << '\n';
}
static void commands() {
    const std::vector<std::string> commands = {
        "", "buildItem0", "BuIlDiTeM56", "buildItem57", "constructItem000",
        "yardItem1", "researchItem56", "evolveItem2", "constructItem",
        "buildItem+1", "buildItem-1", "buildItem 1", "buildItem18446744073709551616",
        std::string("buildItem1\0junk", 15), std::string("buildItem\xff", 10),
        "buildItem00000000000000000000000000000056", "BUILDitem01",
        "constructItem57", "buildItem56x"
    };
    for (const auto& s : commands) {
        auto method = ProductionMethod::evolve;
        std::size_t index = 999;
        bool ok = parse_production_command(s, method, index);
        std::cout << "command " << hex(s) << ' ' << ok << ' '
                  << unsigned(method) << ' ' << index << '\n';
    }
    for (int m = 0; m <= 6; ++m)
        for (std::size_t i : {std::size_t(0), std::size_t(56), std::size_t(57),
                              std::size_t(999)})
            std::cout << "format " << m << ',' << i << ' '
                      << hex(production_command(static_cast<ProductionMethod>(m), i))
                      << ' ' << hex(production_method_name(static_cast<ProductionMethod>(m)))
                      << '\n';
}
static void lists() {
    HybridBuildLists list;
    auto action = [&](ProductionMethod m, std::size_t i, std::uint32_t project) {
        auto result = list.add(m, i, project);
        std::cout << "list-add " << unsigned(m) << ',' << i << ',' << project
                  << '=' << unsigned(result) << " count=" << list.entries().size()
                  << " empty=" << list.empty() << '\n';
    };
    action(ProductionMethod::legacy, 0, 1);
    action(ProductionMethod::construct, 57, 1);
    action(ProductionMethod::construct, 0, 0);
    action(ProductionMethod::construct, 0, 1);
    action(ProductionMethod::construct, 0, 2);
    action(ProductionMethod::yard, 0, 1);
    action(ProductionMethod::yard, 0, 2);
    for (int m = 0; m <= 6; ++m)
        for (std::size_t i : {std::size_t(0), std::size_t(56), std::size_t(-1)}) {
            const auto* e = list.at(static_cast<ProductionMethod>(m), i);
            std::cout << "list-at " << m << ',' << i << '='
                      << (e ? e->project_id : 0) << '\n';
        }
    for (std::uint32_t p : {0u, 1u, 2u, 3u}) {
        const auto* e = list.explicit_entry_for(p);
        std::cout << "list-project " << p << '=' << (e ? unsigned(e->method) : 255)
                  << ',' << (e ? e->index : 999) << ','
                  << unsigned(list.resolve(p, ProductionMethod::research)) << '\n';
    }
    for (auto& e : list.entries())
        std::cout << "list-entry " << unsigned(e.method) << ',' << e.index
                  << ',' << e.project_id << '\n';
    std::mt19937 rng(9134);
    HybridBuildLists random_list;
    for (int step = 0; step < 500; ++step) {
        auto method = static_cast<ProductionMethod>(rng() % 7);
        auto index = std::size_t(rng() % 60);
        auto project = std::uint32_t(rng() % 75);
        auto result = random_list.add(method, index, project);
        const auto* at = random_list.at(method, index);
        const auto* found = random_list.explicit_entry_for(project);
        std::cout << "list-random " << step << ',' << unsigned(method) << ','
                  << index << ',' << project << '=' << unsigned(result) << ','
                  << (at ? at->project_id : 0) << ','
                  << (found ? unsigned(found->method) : 255) << ','
                  << unsigned(random_list.resolve(project, ProductionMethod::legacy))
                  << ',' << random_list.entries().size() << '\n';
    }
}
static void queues() {
    {
        HybridProductionQueue q(1);
        ProductionJob evolve{};
        evolve.queue_id = 1;
        evolve.project_id = 1;
        evolve.method = ProductionMethod::evolve;
        std::cout << "queue-barrier " << unsigned(q.enqueue(evolve)) << ',';
        evolve.queue_id = 2;
        std::cout << unsigned(q.enqueue(evolve)) << ',';
        evolve.queue_id = 1;
        std::cout << unsigned(q.enqueue(evolve)) << '\n';
        print_job(q.start_next());
        std::cout << ',' << q.finish_active();
        queue_state(q);
    }
    for (std::size_t capacity : {std::size_t(0), std::size_t(1), std::size_t(10),
                                 std::size_t(11)}) {
        HybridProductionQueue q(capacity);
        std::mt19937 rng(1245);
        for (int t = 0; t < 180; ++t) {
            unsigned op = rng() % 6;
            std::uint32_t id = rng() % 13;
            ProductionJob job{};
            job.queue_id = id;
            job.project_id = rng() % 13;
            job.method = static_cast<ProductionMethod>(rng() % 7);
            job.placement = {true, float(t), float(-t), 0.25f, 1.5f};
            job.active = true;
            std::cout << "queue " << capacity << ',' << t << ',' << op << ',' << id
                      << ',' << job.project_id << ',' << unsigned(job.method) << '=';
            switch (op) {
                case 0: std::cout << unsigned(q.enqueue(job)); break;
                case 1: print_job(q.start_next()); break;
                case 2: std::cout << q.finish_active(); break;
                case 3: std::cout << q.cancel(id); break;
                case 4: q.clear(); std::cout << "clear"; break;
                case 5: print_job(q.find(id)); break;
            }
            queue_state(q);
        }
    }
}
static void submenus() {
    const std::vector<std::string> values = {
        "", "  Foo.ODF  ", "folder/Ship.ODF", "folder\\SHIP.odf",
        ".odf", "a.odf", "  /  ", std::string("A\0B", 3),
        std::string("High\xff.ODF", 9)
    };
    for (const auto& value : values)
        std::cout << "normalize " << hex(value) << '='
                  << hex(submenu::normalize_object_name(value)) << '\n';
    const std::vector<std::string> configs = {
        "", "buildItem0Refit0 = foo.odf\n",
        "buildItem56Refit56 = \"X/Y/Z.ODF\";\r\n",
        "buildItem57Refit0 = x\nbuildItem0Refit57=x\n",
        "buildItem00Refit000 = first\nbuildItem0Refit0=SECOND\n",
        "buildItem0Refit0=\"a//b.odf\" // hi\n"
        "/* buildItem1Refit1=ignored */ buildItem2Refit2 = final\n",
        "buildItem0Refit0 = \"Escaped\\\\Path\\\"Name.odf\"\n",
        "buildItem0Refit0 = \"unterminated\n",
        std::string("buildItem0Refit0 = Hi\xff.ODF\n", 28),
        "buildItem999999999999999999999999Refit0=x\n",
        "buildItem0Refit0 = x;\rbuildItem0Refit0 = ;\n"
    };
    for (const auto& source : configs) {
        submenu::Config config;
        std::string error = "preexisting";
        auto ok = submenu::parse_config(source, &config, &error);
        std::cout << "config " << hex(source) << ' ' << ok << ' '
                  << hex(error) << ' ' << config.empty();
        for (std::size_t p = 0; p < submenu::kBuildItemCount; ++p)
            for (std::size_t c = 0; c < submenu::kBuildItemCount; ++c)
                if (!config.pages[p].children[c].empty())
                    std::cout << ' ' << p << ',' << c << '='
                              << hex(config.pages[p].children[c]);
        std::cout << '\n';
    }
    std::mt19937 rng(5521);
    const std::string alphabet = "abcABC01257Refit/=; \\\"'\r\n*";
    for (int trial = 0; trial < 100; ++trial) {
        std::string source;
        for (int i = 0, count = int(rng() % 120); i < count; ++i)
            source += alphabet[rng() % alphabet.size()];
        submenu::Config config;
        std::string error;
        bool ok = submenu::parse_config(source, &config, &error);
        std::cout << "config-random " << trial << ' ' << hex(source) << ' '
                  << ok << ' ' << hex(error) << ' ' << config.empty();
        for (std::size_t p = 0; p < submenu::kBuildItemCount; ++p)
            for (std::size_t c = 0; c < submenu::kBuildItemCount; ++c)
                if (!config.pages[p].children[c].empty())
                    std::cout << ' ' << p << ',' << c << '='
                              << hex(config.pages[p].children[c]);
        std::cout << '\n';
    }
    std::string error = "preexisting";
    bool ok = submenu::parse_config("x", nullptr, &error);
    std::cout << "config-null " << ok << ' ' << hex(error) << '\n';
}
int main() {
    commands();
    lists();
    queues();
    submenus();
}
