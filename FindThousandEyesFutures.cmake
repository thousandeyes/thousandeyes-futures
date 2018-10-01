# Look for the header files.
find_path(thousandeyes-futures_INCLUDE_DIR
          NAMES thousandeyes/futures/then.h
          HINTS "${thousandeyes-futures_ROOT}/include")
mark_as_advanced(thousandeyes-futures_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(thousandeyes-futures
                                  REQUIRED_VARS thousandeyes-futures_INCLUDE_DIR)

if (thousandeyes-futures_FOUND)
    set(thousandeyes-futures_INCLUDE_DIRS ${thousandeyes-futures_INCLUDE_DIR})

    # Define imported targets
    add_library(thousandeyes::futures INTERFACE IMPORTED)
    set_target_properties(thousandeyes::futures PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${thousandeyes-futures_INCLUDE_DIRS}")
endif()
