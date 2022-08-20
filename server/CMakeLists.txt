
aux_source_directory(./ SOURCE_DIR)
add_executable(server ${SOURCE_DIR})
target_link_libraries(server lib)
