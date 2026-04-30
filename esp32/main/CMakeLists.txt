set(SPIDERBOT_COMMON_DIR "${CMAKE_CURRENT_LIST_DIR}/../../common")

set(SPIDERBOT_MAIN_SRCS
    "main.cpp"
    "display_manager.cpp"
    "robot_controller.cpp"
    "robot_fsm.cpp"
    "tripod_walk_fsm.cpp"
    "leg_fsm.cpp"
    "servo_controller.cpp"
    "input_manager.cpp"
    "bluepad_manager.cpp"
    "leg_kinematics.cpp"
    "locomotion_fsm.cpp"
    "esp32_runtime_wiring.cpp"
    "${SPIDERBOT_COMMON_DIR}/protocol.cpp")

idf_component_register(
    SRCS ${SPIDERBOT_MAIN_SRCS}
    INCLUDE_DIRS "." "${SPIDERBOT_COMMON_DIR}"
    REQUIRES arduino)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
