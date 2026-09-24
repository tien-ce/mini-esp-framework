#include "built_in.h"
#include "core/ti_interpreter.h"

void builtins_init(void) {
    builtin_system_init();
    builtin_json_init();
    builtin_http_init();
    builtin_console_init();
    builtin_fs_init();
    builtin_web_init();
    builtin_nvs_init();
    builtin_script_init();
    tien_run_file("/init.ti");
}
