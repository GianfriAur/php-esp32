#pragma once

/* 64 KB: a smaller stack resets the board on trivial scripts -- the PHP compiler recurses heavily
 * and zend_bailout relies on setjmp/longjmp over still-live frames. Bytes, as xTaskCreate expects. */
#define PHP_TASK_STACK_BYTES (64 * 1024)

/* Pin the tasks to opposite cores (both the ESP32-S3 and the ESP32-P4 are dual-core): the PHP
 * reactor on core 0, httpd on core 1. Without pinning the scheduler may migrate php_task onto the
 * core the WiFi/lwIP tasks already sit on; opposite cores also let a request's static-file I/O on
 * httpd overlap with PHP work instead of contending with it. */
#define PHP_TASK_CORE   0
#define HTTPD_TASK_CORE 1

/* The PHP reactor task, defined in main.c. app_main() (boot.c) creates it, pinned. */
void php_task(void *arg);
