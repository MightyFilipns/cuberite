cmake_minimum_required(VERSION 3.13)
project (MCADefrag)
find_package(Threads REQUIRED)

# Set include paths to the used libraries:
include_directories(SYSTEM "../../lib")
include_directories("../../src")
include_directories("../../lib/mbedtls/include")

function(flatten_files arg1)
	set(res "")
	foreach(f ${${arg1}})
		get_filename_component(f ${f} ABSOLUTE)
		list(APPEND res ${f})
	endforeach()
	set(${arg1} "${res}" PARENT_SCOPE)
endfunction()


# Include the shared files:
set(SHARED_SRC
	../../src/StringCompression.cpp
	../../src/StringUtils.cpp
	../../src/LoggerListeners.cpp
	../../src/Logger.cpp
)
set(SHARED_HDR
	../../src/ByteBuffer.h
	../../src/StringUtils.h
)

flatten_files(SHARED_SRC)
flatten_files(SHARED_HDR)
source_group("Shared" FILES ${SHARED_SRC} ${SHARED_HDR})

set(SHARED_OSS_SRC
	../../src/OSSupport/CriticalSection.cpp
	../../src/OSSupport/Event.cpp
	../../src/OSSupport/File.cpp
	../../src/OSSupport/IsThread.cpp
	../../src/OSSupport/StackTrace.cpp
	../../src/OSSupport/WinStackWalker.cpp
)

set(SHARED_OSS_HDR
	../../src/OSSupport/CriticalSection.h
	../../src/OSSupport/Event.h
	../../src/OSSupport/File.h
	../../src/OSSupport/IsThread.h
	../../src/OSSupport/StackTrace.h
	../../src/OSSupport/WinStackWalker.h
)


flatten_files(SHARED_OSS_SRC)
flatten_files(SHARED_OSS_HDR)

source_group("Shared\\OSSupport" FILES ${SHARED_OSS_SRC} ${SHARED_OSS_HDR})



# Include the main source files:
set(SOURCES
	MCADefrag.cpp
)
set(HEADERS
	MCADefrag.h
)

source_group("" FILES ${SOURCES} ${HEADERS})

add_executable(MCADefrag
	${SOURCES}
	${HEADERS}
	${SHARED_SRC}
	${SHARED_HDR}
	${SHARED_OSS_SRC}
	${SHARED_OSS_HDR}
)

if(NOT WIN32)
	target_compile_options(MCADefrag PRIVATE -Wno-switch)
	target_compile_options(MCADefrag PRIVATE -Wno-switch-default)
	target_compile_options(MCADefrag PRIVATE -Wno-nan-infinity-disabled)
	target_compile_options(MCADefrag PRIVATE -Wno-duplicate-enum)
	target_compile_options(MCADefrag PRIVATE -Wno-float-equal)
endif()

target_link_libraries(MCADefrag fmt::fmt libdeflate Threads::Threads mbedtls)

include(../../SetFlags.cmake)
set_exe_flags(MCADefrag)
