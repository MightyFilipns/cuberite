project (NoiseSpeedTest)

# Set include paths to the used libraries:
include_directories(SYSTEM "../../lib")
include_directories("../../src")

# Include the shared files:
set(SHARED_SRC
	../../src/Logger.cpp
	../../src/LoggerListeners.cpp
	../../src/OSSupport/CriticalSection.cpp
	../../src/OSSupport/File.cpp
	../../src/OSSupport/StackTrace.cpp
	../../src/OSSupport/WinStackWalker.cpp
	../../src/Noise/Noise.cpp
	../../src/StringUtils.cpp
)

set(SHARED_HDR
	../../src/Noise/Noise.h
	../../src/Noise/OctavedNoise.h
	../../src/Noise/RidgedNoise.h
	../../src/OSSupport/CriticalSection.h
	../../src/OSSupport/File.h
	../../src/OSSupport/StackTrace.h
	../../src/OSSupport/WinStackWalker.h
	../../src/StringUtils.h
)


source_group("Shared" FILES ${SHARED_SRC} ${SHARED_HDR})




# Include the main source files:
set(SOURCES
	NoiseSpeedTest.cpp
)
set(HEADERS
	NoiseSpeedTest.h
	SimplexNoise.h
)

source_group("" FILES ${SOURCES} ${HEADERS})

add_executable(NoiseSpeedTest
	${SOURCES}
	${HEADERS}
	${SHARED_SRC}
	${SHARED_HDR}
)

if(NOT WIN32)
	target_compile_options(NoiseSpeedTest PRIVATE -Wno-switch)
	target_compile_options(NoiseSpeedTest PRIVATE -Wno-switch-default)
	target_compile_options(NoiseSpeedTest PRIVATE -Wno-nan-infinity-disabled)
	target_compile_options(NoiseSpeedTest PRIVATE -Wno-duplicate-enum)
	target_compile_options(NoiseSpeedTest PRIVATE -Wno-float-equal)
endif()

target_link_libraries(NoiseSpeedTest fmt::fmt mbedtls)

set_target_properties(
	NoiseSpeedTest
	PROPERTIES FOLDER Tools
)

include(../../SetFlags.cmake)
set_exe_flags(NoiseSpeedTest)
