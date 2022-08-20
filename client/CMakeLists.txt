aux_source_directory(./ SOURCE_DIR)
add_executable(client ${SOURCE_DIR})
target_link_libraries(client lib)