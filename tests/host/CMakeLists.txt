cmake_minimum_required(VERSION 3.22)
project(rubbish_sorting_host_tests C)

enable_testing()

set(HOST_TEST_SOURCES
    test_main.c
    stubs/fake_hal.c
    ../../App/Src/sort_task.c
    ../../App/Src/sort_sequence.c
    ../../BSP/Src/servo.c
    ../../BSP/Src/tb6600.c
    ../../BSP/Src/tb6600_timing.c
    ../../Motion/Src/stepper.c
    ../../Motion/Src/stepper_profile.c
    ../../Control/Src/pitch_axis.c
    ../../Control/Src/yaw_axis.c
    ../../Diagnostics/Src/debug_state.c
    ../../Protocol/Src/protocol.c
)

function(configure_host_test_target target debug_control_enabled raw_bench_enabled)
    add_executable(${target} ${HOST_TEST_SOURCES})
    target_include_directories(${target} PRIVATE
        stubs
        ../../App/Inc
        ../../BSP/Inc
        ../../Config
        ../../Control/Inc
        ../../Diagnostics/Inc
        ../../Motion/Inc
        ../../Protocol/Inc
    )
    target_compile_features(${target} PRIVATE c_std_11)
    target_compile_definitions(${target} PRIVATE
        DEBUG_CONTROL_ENABLE=${debug_control_enabled}
        RAW_BENCH_COMMANDS_ENABLE=${raw_bench_enabled}
        SORT_MECHANICAL_CALIBRATION_COMPLETE=1
        SORT_YAW_GROUP_13_MDEG=-45000L
        SORT_YAW_GROUP_24_MDEG=90000L
        SORT_PITCH_DUMP_ANGLE_MDEG=15000L
        SORT_BOX1_PITCH_DIRECTION=1
        SORT_BOX2_PITCH_DIRECTION=-1
        SORT_BOX3_PITCH_DIRECTION=-1
        SORT_BOX4_PITCH_DIRECTION=1
        SERVO_CALIBRATION_VALID=1
        YAW_AXIS_SCALE_VERIFIED=1
    )
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

configure_host_test_target(firmware_host_tests 1 1)
configure_host_test_target(firmware_host_tests_release_control 0 0)
configure_host_test_target(firmware_host_tests_limits 1 1)
configure_host_test_target(firmware_host_tests_fractional_center 1 1)
# Keep the legacy opt-out defined in this target to verify it cannot disable
# the mandatory cable limits after the configuration switch was removed.
target_compile_definitions(firmware_host_tests_limits PRIVATE
    YAW_SOFT_LIMIT_VALID=0
)
target_compile_definitions(firmware_host_tests_fractional_center PRIVATE
    PITCH_LEVEL_SERVO_MDEG=130500L
)

add_test(NAME firmware_host_tests COMMAND firmware_host_tests)
add_test(NAME firmware_host_tests_release_control COMMAND firmware_host_tests_release_control)
add_test(NAME firmware_host_tests_limits COMMAND firmware_host_tests_limits)
add_test(NAME firmware_host_tests_fractional_center COMMAND firmware_host_tests_fractional_center)

set(SORT_UNCONFIGURED_SOURCES ${HOST_TEST_SOURCES})
list(REMOVE_ITEM SORT_UNCONFIGURED_SOURCES test_main.c)
list(APPEND SORT_UNCONFIGURED_SOURCES test_sort_unconfigured.c)
add_executable(firmware_sort_unconfigured_tests ${SORT_UNCONFIGURED_SOURCES})
target_include_directories(firmware_sort_unconfigured_tests PRIVATE
    stubs
    ../../App/Inc
    ../../BSP/Inc
    ../../Config
    ../../Control/Inc
    ../../Diagnostics/Inc
    ../../Motion/Inc
    ../../Protocol/Inc
)
target_compile_features(firmware_sort_unconfigured_tests PRIVATE c_std_11)
target_compile_definitions(firmware_sort_unconfigured_tests PRIVATE
    DEBUG_CONTROL_ENABLE=0
    RAW_BENCH_COMMANDS_ENABLE=0
)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(firmware_sort_unconfigured_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME firmware_sort_unconfigured_tests COMMAND firmware_sort_unconfigured_tests)
