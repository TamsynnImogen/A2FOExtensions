// Isolate the original bridge's symbols without changing its instructions.
#define _a2fo_refit_bridge _a2fo_builtin_camera_bridge
#define _a2fo_refit_value_bits _a2fo_builtin_camera_value_bits
#define _a2fo_refit_parser_visits _a2fo_builtin_camera_parser_visits
#define _a2fo_refit_data_a _a2fo_builtin_camera_data_a
#include "../modules/A2FOCameraControls/native_bridge.S"
