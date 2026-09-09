#pragma once

/* The web-server execution model (defined in web_server.c, built only for PHP_PROJECT_WEB_SERVER):
 * start the HTTP server and loop in php_task serving one request at a time. Never returns. */
void run_web_server(const char *script, const char *init_script);
