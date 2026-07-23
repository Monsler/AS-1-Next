# Install script for directory: /home/monsler/Документы/AS-1 Next/build-release/_deps/juce-src

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Custom")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "0")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-build/tools/modules/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-build/tools/extras/Build/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/JUCE-8.0.15" TYPE FILE FILES
    "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-build/tools/JUCEConfigVersion.cmake"
    "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-build/tools/JUCEConfig.cmake"
    "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-src/extras/Build/CMake/FindCppwinrt.cmake"
    "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-src/extras/Build/CMake/FindWebView2.cmake"
    "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-src/extras/Build/CMake/FindWindowsMIDIServices.cmake"
    "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-src/extras/Build/CMake/JUCECheckAtomic.cmake"
    "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-src/extras/Build/CMake/JUCEHelperTargets.cmake"
    "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-src/extras/Build/CMake/JUCEModuleSupport.cmake"
    "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-src/extras/Build/CMake/JUCEUtils.cmake"
    "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-src/extras/Build/CMake/JuceLV2Defines.h.in"
    "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-src/extras/Build/CMake/LaunchScreen.storyboard"
    "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-src/extras/Build/CMake/PIPAudioProcessor.cpp.in"
    "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-src/extras/Build/CMake/PIPAudioProcessorWithARA.cpp.in"
    "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-src/extras/Build/CMake/PIPComponent.cpp.in"
    "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-src/extras/Build/CMake/PIPConsole.cpp.in"
    "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-src/extras/Build/CMake/RecentFilesMenuTemplate.nib"
    "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-src/extras/Build/CMake/UnityPluginGUIScript.cs.in"
    "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-src/extras/Build/CMake/checkBundleSigning.cmake"
    "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-src/extras/Build/CMake/copyDir.cmake"
    "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-src/extras/Build/CMake/juce_runtime_arch_detection.cpp"
    "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-src/extras/Build/CMake/juce_LinuxSubprocessHelper.cpp"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/JUCE-8.0.15" TYPE DIRECTORY FILES "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-src/extras/Build/CMake/juce_vst3_helper")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-build/tools/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/monsler/Документы/AS-1 Next/build-release/_deps/juce-build/tools/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
