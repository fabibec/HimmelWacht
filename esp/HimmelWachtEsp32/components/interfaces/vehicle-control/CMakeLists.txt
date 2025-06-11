idf_component_register(
    SRCS "vehicle-control.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES "ds4-driver" "platform-control" "fire-control" "espressif__iqmath" "diff-drive"
)

# Loggin: ENABLE_DEBUG_LOGS
# IQMath: GLOBAL_IQ=%value%, 24 default if not set
target_compile_definitions(${COMPONENT_LIB} PRIVATE GLOBAL_IQ=24)