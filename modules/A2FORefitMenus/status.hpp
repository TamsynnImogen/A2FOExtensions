#pragma once
#include <cstdint>
namespace a2fo::refit_menu {
struct Status {
    std::uint32_t struct_size, version, installed, active, shown, actions;
};
}
