# Event bus C core. Portable C; the test/ sources build on the host with gcc, so they are not part
# of the component build. heap_caps (used by pool.c on target) is part of the always-available heap
# component, so no explicit REQUIRES is needed.
idf_component_register(
    SRCS "src/pool.c" "src/dispatch.c"
    INCLUDE_DIRS "include"
)
