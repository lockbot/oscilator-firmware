cmake_minimum_required(VERSION 3.10)
project(sofia_firmware C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

option(USE_FREERTOS "Compilar com FreeRTOS+POSIX" OFF)
option(USE_POSIX    "Compilar com POSIX pthreads direto" ON)

# ── Caminhos do FreeRTOS Kernel ──────────────────────────────────────────────
set(FREERTOS_KERNEL_DIR ${CMAKE_SOURCE_DIR}/third_party/FreeRTOS-Kernel)
set(FREERTOS_PORT_DIR   ${FREERTOS_KERNEL_DIR}/portable/ThirdParty/GCC/Posix)
set(FREERTOS_HEAP       ${FREERTOS_KERNEL_DIR}/portable/MemMang/heap_4.c)

# ── Includes globais ─────────────────────────────────────────────────────────
include_directories(
        src/osal
        src/hal
        src/core
        src/app
)

# ── Fontes da aplicação ───────────────────────────────────────────────────────
set(SOURCES
        src/app/main.c
        src/core/oscillators.c
        src/hal/drivers/rpi/hal_pcf8591.c
)

# ── Backend OSAL ──────────────────────────────────────────────────────────────
if(USE_FREERTOS)
    message(STATUS "Backend: FreeRTOS+POSIX")

    list(APPEND SOURCES
            src/osal/freertos/osal_thread_freertos.c
            src/osal/freertos/osal_input_freertos.c

            # Kernel FreeRTOS
            ${FREERTOS_KERNEL_DIR}/tasks.c
            ${FREERTOS_KERNEL_DIR}/queue.c
            ${FREERTOS_KERNEL_DIR}/list.c
            ${FREERTOS_KERNEL_DIR}/timers.c
            ${FREERTOS_KERNEL_DIR}/event_groups.c

            # Port POSIX
            ${FREERTOS_PORT_DIR}/port.c
            ${FREERTOS_PORT_DIR}/utils/wait_for_event.c

            # Gerenciador de heap
            ${FREERTOS_HEAP}
    )

    include_directories(
            ${FREERTOS_KERNEL_DIR}/include
            ${FREERTOS_PORT_DIR}
            ${FREERTOS_PORT_DIR}/utils
            src/osal/freertos          # <- Onde está o FreeRTOSConfig.h
    )

    find_package(Threads REQUIRED)

elseif(USE_POSIX)
    message(STATUS "Backend: POSIX pthreads")
    list(APPEND SOURCES
            src/osal/linux/osal_thread_posix.c
            src/osal/linux/osal_input_linux.c
    )
    find_package(Threads REQUIRED)
endif()

# ── Executável ────────────────────────────────────────────────────────────────
add_executable(sofia_firmware ${SOURCES})

if(USE_FREERTOS OR USE_POSIX)
    target_link_libraries(sofia_firmware Threads::Threads m)
endif()