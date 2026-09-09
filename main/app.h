#pragma once
#include "zend_API.h"   /* zval */

/* Cross-model shared state, defined in main.c and set once at boot in php_task:
 *   s_board_ip                 -- the board's IP for $_SERVER['SERVER_ADDR'] (web-server); "" if none.
 *   register_esp32_server_vars -- add the PHP_ESP32_* identity entries to a $_SERVER array (both models). */
extern char s_board_ip[16];
void register_esp32_server_vars(zval *srv);

/* The resolved entry script and its source-mount directory, published by php_task once known and
 * before it hands off to the selected model runner. A runner takes no arguments (§8.2
 * model_runner_t), so it reads these: the web-server resolves its init script under g_src_dir, the
 * init-loop only needs g_entry_script. Both are NULL until php_task sets them. */
extern const char *g_entry_script;
extern const char *g_src_dir;
