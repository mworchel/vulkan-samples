function(add_shader_compile_target TARGET_NAME SHADER_FILES)
	set(GLSLANG_VALIDATOR "glslangValidator")

	foreach(SHADER_FILE ${SHADER_FILES})
		get_filename_component(FILE_NAME ${SHADER_FILE} NAME)
		get_filename_component(FILE_DIR ${SHADER_FILE} DIRECTORY)
		set(SPIRV_FILE "${CMAKE_CURRENT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
		add_custom_command(
			OUTPUT ${SPIRV_FILE}
		    COMMAND ${GLSLANG_VALIDATOR} -V -D ${SHADER_FILE} -o ${SPIRV_FILE} -e main
		    DEPENDS ${SHADER_FILE})
		list(APPEND SPIRV_BINARY_FILES ${SPIRV_FILE})

		get_filename_component(FILE_NAME_PLAIN ${SHADER_FILE} NAME_WLE)
		string(TOUPPER ${FILE_NAME_PLAIN} FILE_NAME_PLAIN)
		string(REPLACE "-" "_" FILE_NAME_PLAIN ${FILE_NAME_PLAIN})
		get_filename_component(FILE_EXT ${SHADER_FILE} LAST_EXT)
		string(SUBSTRING ${FILE_EXT} 1 -1 FILE_EXT)
		string(TOUPPER ${FILE_EXT} FILE_EXT)
		list(APPEND SHADER_COMPILE_DEFINITIONS -DPATH_${FILE_NAME_PLAIN}_${FILE_EXT}="${SPIRV_FILE}")
	endforeach(SHADER_FILE)

	# Add custom target for the shaders
	add_custom_target(
    ${TARGET_NAME}-shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
    )

	add_dependencies(${TARGET_NAME} ${TARGET_NAME}-shaders)

	foreach(SHADER_COMPILE_DEFINITION ${SHADER_COMPILE_DEFINITIONS})
		target_compile_definitions(${TARGET_NAME} PRIVATE ${SHADER_COMPILE_DEFINITION})
	endforeach(SHADER_COMPILE_DEFINITION)
endfunction()