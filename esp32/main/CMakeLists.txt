set(SPIDERBOT_COMMON_DIR "${CMAKE_CURRENT_LIST_DIR}/../../common")

set(SPIDERBOT_MAIN_SRCS
    "main.cpp"
    "display_manager.cpp"
    "menu_input.cpp"
    "ble_controller.cpp")

idf_component_register(
    SRCS ${SPIDERBOT_MAIN_SRCS}
    INCLUDE_DIRS "." "${SPIDERBOT_COMMON_DIR}"
    REQUIRES esp_system esp_adc esp_timer driver bt nvs_flash)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
