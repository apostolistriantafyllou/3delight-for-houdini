# Get the list of the files
file(GLOB OSL_SRCS "*.osl")

# This will get the destination directory for the installation.
if(NOT DEFINED INSTALL_DIR)
	houdini_get_default_install_dir( INSTALL_DIR )
ENDIF()

# Compile and install each file
foreach(osl_file ${OSL_SRCS})

    # Get filename without extension and path
    get_filename_component( osl_name ${osl_file} NAME_WE )

    # Compile command
    add_custom_command( OUTPUT ${osl_name}.oso
        COMMAND ${DELIGHT}/bin/oslc -I${OSL_SHADERS_DIR} -D_3DFH=1 -q ${osl_file}
        DEPENDS ${osl_file}
        COMMENT "Compiling ${osl_name}.osl" )

    add_custom_target( ${osl_name} DEPENDS ${osl_name}.oso ${osl_file} )

	add_dependencies( 3Delight_for_Houdini ${osl_name} )

	install(
		FILES ${CMAKE_CURRENT_BINARY_DIR}/${osl_name}.oso
		DESTINATION ${INSTALL_DIR}/osl)

endforeach(osl_file)
