idf_component_register(SRCS "main.cpp" "application.cpp" "tag_scanner.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES audio_player buttons pn532 sd_card)
