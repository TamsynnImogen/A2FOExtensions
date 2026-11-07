// Isolate the original bridge's symbols without changing its instructions.
#define _a2fo_refit_bridge _a2fo_builtin_view_bridge
#define _a2fo_refit_value_bits _a2fo_builtin_view_value_bits
#define _a2fo_refit_parser_visits _a2fo_builtin_view_parser_visits
#define _a2fo_refit_data_a _a2fo_builtin_view_data_a
#define _a2fo_refit_data_b _a2fo_builtin_view_data_b
#include "../modules/A2FOViewDistance/native_bridge.S"
