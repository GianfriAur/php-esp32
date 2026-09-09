#pragma once

/* The init-loop (run-once) execution model, defined in init_loop.c: run the script top-level, then
 * drive setup()/loop() if defined. Used when PHP_PROJECT_WEB_SERVER is off. */
void run_init_loop(const char *script);
