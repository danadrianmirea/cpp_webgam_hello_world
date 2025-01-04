#=======================================================================================================================
       add_executable(cpp_webcam_hw)
#=======================================================================================================================
       target_sources(cpp_webcam_hw PRIVATE main.cpp)
#=======================================================================================================================
set_target_properties(cpp_webcam_hw PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/build"
                                      RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/build")
set_target_properties(cpp_webcam_hw PROPERTIES WIN32_EXECUTABLE YES)                                      
#=======================================================================================================================