
aux_source_directory(./ SOURCE_DIR)
add_executable(Server ${SOURCE_DIR})
target_link_libraries(Server lib)
