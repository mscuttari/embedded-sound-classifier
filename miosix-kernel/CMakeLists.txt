##
## Makefile for Miosix embedded OS
##

set(PROJECT_NAME "NeuralNetwork")

## Path to miosix toolchain directory
set(TOOLCHAIN_PATH "C:/arm-miosix-eabi")

## Path to legacy toolchain directory.
## Only needed in particular situtation, such as in case of usage of ST's AI plugin, in order
## to compile the source code with the previously set toolchain but link it with the standard linker
## and pass the seucirty checks.
set(ARM_NONE_EABI_PATH "D:/arm-none-eabi")

## Path to kernel directory
set(KPATH "miosix")

## List here additional static libraries
set(LIBS "lib/*.a")

## List here additional include directories
set(INCLUDE_DIRS "")

## List here your source files (both .s, .c and .cpp)
set(SOURCES "src/*.c" "src/*.cpp")



##############################################################################
## You should not need to modify anything below                             ##
##############################################################################

project(${PROJECT_NAME} C CXX ASM)
cmake_minimum_required(VERSION 3.7)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_VERBOSE_MAKEFILE ON)


## Toolchain
if (NOT DEFINED TOOLCHAIN_PATH OR "${TOOLCHAIN_PATH}" STREQUAL "")
    message(WARNING "Toolchain not set")
elseif(NOT EXISTS ${TOOLCHAIN_PATH})
    message(FATAL_ERROR "Toolchain not found at path \"${TOOLCHAIN_PATH}\"")
endif()

file(GLOB TOOLCHAIN_PATH ${TOOLCHAIN_PATH})


## Kernel
if (NOT DEFINED KPATH)
    message(WARNING "Kernel path not set")
elseif(NOT EXISTS ${KPATH})
    message(FATAL_ERROR "Kernel not found at path \"${KPATH}\"")
endif()

set(KPATH_REL ${KPATH})
file(GLOB KPATH ${KPATH})
file(GLOB CONFPATH ${KPATH}/config)


## Sources
file(GLOB_RECURSE SOURCES "src/*.c" "src/*.cpp")

if ("${SOURCES}" STREQUAL "")
    message(WARNING "No sources specified")
endif()


## Include the kernel and its configuration
add_subdirectory(${KPATH})
include(${CONFPATH}/CMakeLists.txt)


## Boot file
if (NOT DEFINED BOOT_FILE)
    message(WARNING "Boot file not set")
elseif(NOT EXISTS ${BOOT_FILE})
    message(FATAL_ERROR "Boot file \"${BOOT_FILE}\" not found")
endif()


## Linker script
if (NOT DEFINED LINKER_SCRIPT)
    message(WARNING "Linker script not set")
elseif(NOT EXISTS ${LINKER_SCRIPT})
    message(FATAL_ERROR "Linker script \"${LINKER_SCRIPT}\" not found")
endif()


## Directories containing the header files
include_directories(
        ${TOOLCHAIN_PATH}/arm-miosix-eabi/include
        ${TOOLCHAIN_PATH}/lib/gcc/arm-miosix-eabi/4.7.3/include
        ${TOOLCHAIN_PATH}/lib/gcc/arm-miosix-eabi/4.7.3/include-fixed
        ${TOOLCHAIN_PATH}/arm-miosix-eabi/include/c++/4.7.3/
        ${TOOLCHAIN_PATH}/arm-miosix-eabi/include/c++/4.7.3/arm-miosix-eabi
        ${TOOLCHAIN_PATH}/arm-miosix-eabi/include/c++/4.7.3/backward
        ${CONFPATH}
        ${CONFPATH}/${BOARD_INC}
        ${KPATH}
        ${KPATH}/arch/common
        ${KPATH}/${ARCH_INC}
        ${KPATH}/${BOARD_INC}
        ${INCLUDE_DIRS}
)


## Compilers
set(DFLAGS "-MMD -MP")
set(CMAKE_C_FLAGS "${DFLAGS} ${CFLAGS_BASE}")
set(CMAKE_CXX_FLAGS "${DFLAGS} ${CXXFLAGS_BASE}")
set(CMAKE_ASM_FLAGS ${AFLAGS_BASE})


## Linker
file(GLOB_RECURSE LIBS ${LIBS})
string (REPLACE ";" " " LIBS "${LIBS}")

set(LFLAGS ${LFLAGS_BASE})

# set(LINK_LIBS "${LIBS} -L${CMAKE_CURRENT_BINARY_DIR}/${KPATH_REL} -Wl,--start-group -lmiosix -lstdc++ -lc -lm -lgcc -Wl,--end-group")
set(LINK_LIBS "${LIBS} -L${CMAKE_CURRENT_BINARY_DIR}/${KPATH_REL} -Wl,--start-group -lmiosix ${ARCH_LIBS} -Wl,--end-group")

if (NOT DEFINED ARM_NONE_EABI_PATH)
    set(C_LINKER ${CMAKE_C_COMPILER})
    set(CXX_LINKER ${CMAKE_CXX_COMPILER})

else()
    if (NOT EXISTS ${ARM_NONE_EABI_PATH})
        message(FATAL_ERROR "Legacy toolchain toolchain not found at path \"${ARM_NONE_EABI}\"")
    endif()

    file(GLOB ARM_NONE_EABI_PATH ${ARM_NONE_EABI_PATH})

    set(C_LINKER ${ARM_NONE_EABI_PATH}/bin/arm-none-eabi-gcc)
    set(CXX_LINKER ${ARM_NONE_EABI_PATH}/bin/arm-none-eabi-g++)
endif()

set(CMAKE_C_LINK_EXECUTABLE "${C_LINKER} ${LFLAGS} -o ${PROJECT_SOURCE_DIR}/main.elf <OBJECTS> ${LINK_LIBS}")
set(CMAKE_CXX_LINK_EXECUTABLE "${CXX_LINKER} ${LFLAGS} -o ${PROJECT_SOURCE_DIR}/main.elf <OBJECTS> ${LINK_LIBS}")


## Target
add_executable(${PROJECT_NAME} ${SOURCES} ${LINKER_SCRIPT})


## Generate .hex and .bin files
set(HEX_FILE ${PROJECT_SOURCE_DIR}/main.hex)
set(BIN_FILE ${PROJECT_SOURCE_DIR}/main.bin)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex ${PROJECT_SOURCE_DIR}/main.elf ${HEX_FILE}
        COMMAND ${CMAKE_OBJCOPY} -O binary ${PROJECT_SOURCE_DIR}/main.elf ${BIN_FILE}
        COMMENT "Building ${HEX_FILE}"
        COMMENT "Building ${BIN_FILE}")


## Compile the kernel first
add_dependencies(${PROJECT_NAME} Miosix)
