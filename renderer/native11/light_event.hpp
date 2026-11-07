#pragma once
#include "light_config.hpp"
#include "../../sdk/include/a2fo_module_api.h"
namespace a2fo::native11 {
// Consumes callback-scoped copied ODF views, never ParameterDB or engine fields.
// Handler registration and class/object lifetime remain the host's responsibility.
bool copy_light_config(const A2FO_GameObjectClassLoadedEvent&,LightConfig&,
                       std::string& error) noexcept;
}
