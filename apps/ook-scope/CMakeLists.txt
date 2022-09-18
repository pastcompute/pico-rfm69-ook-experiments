add_executable(
        app_ook-scope
        main.cpp
        )

target_link_libraries(
        app_ook-scope
        arduino-compat
        external-lib-radiohead
        )

pico_add_extra_outputs(app_ook-scope)
