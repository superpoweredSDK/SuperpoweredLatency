cmake_minimum_required(VERSION 3.4.1)

set(
	CACHE STRING ""
)

file(GLOB CPP_FILES "*.cpp")

# Building two native libs from the same code, one with and one without AAudio.

add_library(
	WithoutAAudio
	SHARED
	${CPP_FILES}
)

add_library(
	WithAAudio
	SHARED
	${CPP_FILES}
)

include_directories(src/main/jni)

target_compile_definitions(WithoutAAudio PUBLIC HAS_AAUDIO=0)

target_link_libraries(
    WithoutAAudio
    log
    android
    OpenSLES
)

target_compile_definitions(WithAAudio PUBLIC HAS_AAUDIO=1)

target_link_libraries(
    WithAAudio
    log
    android
    OpenSLES
    aaudio
)
