add_executable(
        app_ook-pio
        main.cpp
        )

pico_generate_pio_header(app_ook-pio ${CMAKE_CURRENT_LIST_DIR}/sample.pio)

target_link_libraries(
        app_ook-pio
        arduino-compat
        hardware_pio
        external-lib-radiohead
        external-lib-ookdecoder
        )

pico_add_extra_outputs(app_ook-pio)
