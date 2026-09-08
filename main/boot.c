/* boot.c -- firmware entry point. app_main() brings up the PHP reactor task, pinned to
 * PHP_TASK_CORE. The bootstrap that runs inside the task (mounts, the project env, the SAPI hooks,
 * php_embed_init and the extension registration) still lives in main.c's php_task for now; it moves
 * here in the following refactor steps. */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "boot.h"

static const char *TAG = "php-esp32";

void app_main(void)
{
    ESP_LOGI(TAG, "starting PHP runtime");
    xTaskCreatePinnedToCore(php_task, "php", PHP_TASK_STACK_BYTES, NULL, 5, NULL, PHP_TASK_CORE);
}
