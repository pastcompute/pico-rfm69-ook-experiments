add_executable(
        app_ook-demod
        main.cpp
        )

target_link_libraries(
        app_ook-demod
        arduino-compat
        external-lib-radiohead
        )

pico_add_extra_outputs(app_ook-demod)
