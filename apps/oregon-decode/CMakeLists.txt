add_executable(
        app_oregon-decode
        main.cpp
        )

target_link_libraries(
        app_oregon-decode
        arduino-compat
        external-lib-radiohead
        external-lib-ookdecoder
        )

pico_add_extra_outputs(app_oregon-decode)
