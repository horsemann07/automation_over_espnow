
set(srcs "")
set(requires "")
set(include_dirs "")

list(APPEND srcs         "posix_compat/clock_esp.c"
                         "posix_compat/semaphore.c")

list(APPEND include_dirs "include" "logging")

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES ${requires}
)