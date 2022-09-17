add_executable(
        app_ook-timing
        main.cpp
        )

target_link_libraries(
        app_ook-timing
        arduino-compat
        external-lib-radiohead
        )

pico_add_extra_outputs(app_ook-timing)
