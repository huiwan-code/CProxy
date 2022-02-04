aux_source_directory(./ SOURCE_DIR)
add_executable(Client ${SOURCE_DIR})
target_link_libraries(Client lib)