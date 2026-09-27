cmake_minimum_required(VERSION 3.22)
project(rubbish_sorting_host_tests C)

enable_testing()

set(HOST_TEST_SOURCES
    test_main.c
    stubs/fake_hal.c
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
    )
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

configure_host_test_target(firmware_host_tests 1 1)
configure_host_test_target(firmware_host_tests_release_control 0 0)
configure_host_test_target(firmware_host_tests_limits 1 1)
configure_host_test_target(firmware_host_tests_fractional_center 1 1)
target_compile_definitions(firmware_host_tests_limits PRIVATE
    AXIS_LIMIT_TESTS=1
    YAW_SOFT_LIMIT_VALID=1
    YAW_SOFT_MIN_MDEG=-90000L
    YAW_SOFT_MAX_MDEG=90000L
)
target_compile_definitions(firmware_host_tests_fractional_center PRIVATE
    PITCH_LEVEL_SERVO_MDEG=130500L
)

add_test(NAME firmware_host_tests COMMAND firmware_host_tests)
add_test(NAME firmware_host_tests_release_control COMMAND firmware_host_tests_release_control)
add_test(NAME firmware_host_tests_limits COMMAND firmware_host_tests_limits)
add_test(NAME firmware_host_tests_fractional_center COMMAND firmware_host_tests_fractional_center)
