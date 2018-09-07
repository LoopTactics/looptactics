option(DOXYGEN "Build documentation" ON)

if (DOXYGEN)
  find_package(Doxygen)
  if (DOXYGEN_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
                   ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)

    add_custom_target(docs ALL
      COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Building API documentation"
      VERBATIM )
  else (DOXYGEN_FOUND)
    message("Doxygen not found")
  endif (DOXYGEN_FOUND)
endif(DOXYGEN)
