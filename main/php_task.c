/* php_task.c -- the reactor primitives: compile+run a PHP file, and the Arduino-style setup()/loop()
 * driver with the per-call exception handling + periodic GC. The php_task() function itself still
 * lives in main.c while its bootstrap is extracted into boot.c; it joins this file once that's done. */
#include <string.h>

#include "esp_log.h"
#include "esp_system.h"    /* esp_get_free_heap_size */

#include "php_embed.h"
#include "zend_API.h"
#include "zend_stream.h"
#include "zend_exceptions.h"

#include "php_task.h"

static const char *TAG = "php-esp32";

/* Compile and run a file (handles <?php ... ?>). Defines any functions in it. */
void run_php_file(const char *path)
{
    zend_file_handle file_handle;
    zend_stream_init_filename(&file_handle, path);
    if (!php_execute_script(&file_handle)) {
        ESP_LOGE(TAG, "php_execute_script(%s) failed", path);
    }
    zend_destroy_file_handle(&file_handle);
}

static zend_function *find_php_function(const char *name)
{
    return zend_hash_str_find_ptr(EG(function_table), name, strlen(name));
}

/*
 * If the loaded script defined loop(), drive it Arduino-style: setup() once, then
 * loop($tick) forever. delay() inside PHP maps to vTaskDelay(), which yields the
 * core so the watchdog stays happy; a loop() that never calls delay() will trip it.
 */
void run_setup_loop(void)
{
    zend_function *fn_loop = find_php_function("loop");
    if (!fn_loop) {
        return;   /* plain script; it already ran */
    }

    zval ret;

    zend_function *fn_setup = find_php_function("setup");
    if (fn_setup) {
        ZVAL_UNDEF(&ret);
        zend_call_known_function(fn_setup, NULL, NULL, &ret, 0, NULL, NULL);
        zval_ptr_dtor(&ret);
    }

    ESP_LOGI(TAG, "entering loop()");
    for (uint32_t tick = 0; ; tick++) {
        zval arg;
        ZVAL_LONG(&arg, tick);
        ZVAL_UNDEF(&ret);
        zend_call_known_function(fn_loop, NULL, NULL, &ret, 1, &arg, NULL);
        zval_ptr_dtor(&ret);

        if (EG(exception)) {
            zend_clear_exception();   /* an uncaught PHP exception: log-and-continue */
            ESP_LOGW(TAG, "uncaught exception in loop() at tick %u", (unsigned) tick);
        }

        /* Refcounting frees most garbage immediately; cycles need a periodic sweep,
         * or a long-running device accumulates them until it dies. Also a good spot
         * to watch that memory isn't creeping up. */
        if ((tick & 0xFF) == 0) {
            gc_collect_cycles();
            ESP_LOGI(TAG, "tick %u -- heap free: %u bytes",
                     (unsigned) tick, (unsigned) esp_get_free_heap_size());
        }
    }
}
