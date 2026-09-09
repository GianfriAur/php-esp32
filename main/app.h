#pragma once
#include "zend_API.h"   /* zval */

/* Cross-model shared state, defined in main.c and set once at boot in php_task:
 *   s_board_ip                 -- the board's IP for $_SERVER['SERVER_ADDR'] (web-server); "" if none.
 *   register_esp32_server_vars -- add the PHP_ESP32_* identity entries to a $_SERVER array (both models). */
extern char s_board_ip[16];
void register_esp32_server_vars(zval *srv);
