if(NOT DEFINED DUMMY_MODEL)
    message(FATAL_ERROR "DUMMY_MODEL is required")
endif()

get_filename_component(dummy_model_dir "${DUMMY_MODEL}" DIRECTORY)
file(MAKE_DIRECTORY "${dummy_model_dir}")
file(WRITE "${DUMMY_MODEL}" "not a real model")

