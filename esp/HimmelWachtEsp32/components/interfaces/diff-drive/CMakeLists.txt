idf_component_register(SRCS "diff-drive.c"
                       INCLUDE_DIRS "include"
                       REQUIRES motor-driver freertos driver utils espressif__iqmath)

# Loggin: ENABLE_DEBUG_LOGS
# IQMath: GLOBAL_IQ=%value%, 24 default if not set
target_compile_definitions(${COMPONENT_LIB} PRIVATE GLOBAL_IQ=15)