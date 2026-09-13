#pragma once

/* Reactor primitives (defined in php_task.c):
 *   run_php_file()   -- compile and run a PHP file, defining its functions.
 *   run_setup_loop() -- if the script defined loop(), drive setup() once then loop($tick) forever,
 *                       with the per-call zend_try/catch handling + periodic GC.
 * Used by the model dispatch in main.c (and, later, the model runners). */
void run_php_file(const char *path);
void run_setup_loop(void);
