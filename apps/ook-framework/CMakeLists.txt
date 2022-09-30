add_executable(
        app_ook-framework
        main.cpp
        )

target_link_libraries(
        app_ook-framework
        arduino-compat
        external-lib-radiohead
        external-lib-ookdecoder
        )

pico_add_extra_outputs(app_ook-framework)
