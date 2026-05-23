cmake_minimum_required(VERSION 3.28)
project(pcm-audio-manager)

set (PCM_AUDIO_MANAGER_VERSION_MAJOR 1)
set (PCM_AUDIO_MANAGER_VERSION_MINOR 0)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -fmessage-length=0 -Wall -g -pthread")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -Wall -g -pthread")

find_package(ALSA REQUIRED)

find_path(ALSA_INCLUDE_DIR NAMES asoundlib.h
          PATH_SUFFIXES alsa
          DOC "The ALSA (asound) include directory"
)

find_library(ALSA_LIBRARY NAMES asound
          DOC "The ALSA (asound) library"
)


set(SOURCE_FILES  src/pcm_audio_manager.c)

add_definitions(-DPIC)

include_directories(ALSA_INCLUDE_DIR)

add_library(asound_module_pcm_audio_manager SHARED ${SOURCE_FILES})
target_link_libraries(asound_module_pcm_audio_manager asound)


# install
if (${ALSA_LIBRARY} MATCHES "/usr/lib/x86_64-linux-gnu/libasound.so")
  set(INSTALL_DIR "/usr/lib/x86_64-linux-gnu/alsa-lib/")
else()
  set(INSTALL_DIR ${CMAKE_INSTALL_PREFIX}"/alsa-lib/")
endif()

install(TARGETS asound_module_pcm_audio_manager DESTINATION ${INSTALL_DIR})


# message
message(STATUS "
----------------------------------------------------------------------
${PROJECT_NAME} version ${PROJECT_VERSION}
----------------------------------------------------------------------

   Configured on host '${HOSTNAME}'
     host type                       = ${CMAKE_SYSTEM_NAME}
     host operating system           = ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION}
     host CPU architecture           = ${CMAKE_SYSTEM_PROCESSOR}

   General build flags:
     C COMPILER                      = ${CMAKE_C_COMPILER}
     CMAKE_C_FLAGS                   = ${CMAKE_C_FLAGS}

   Library configuration:
     Alsa include directory          = ${ALSA_INCLUDE_DIR}
     Alsa library                    = ${ALSA_LIBRARY}

   Other configuration settings:
     Installation directory          = ${INSTALL_DIR}
")
