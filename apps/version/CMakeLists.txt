add_executable(
        app_version
        main.cpp
        )

target_link_libraries(
        app_version
        arduino-compat
        external-lib-radiohead
        )

pico_add_extra_outputs(app_version)
