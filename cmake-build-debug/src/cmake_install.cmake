# Install script for directory: /home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/src

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
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
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
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/usr/local/lib/libhermes_engine.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/usr/local/lib/libhermes_engine.so")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/usr/local/lib/libhermes_engine.so"
         RPATH "/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/hermes_shm-master-lagagphskmh4kdu75me4di6w7bgs3oe7/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/hermes-dev-1.1-kmd7tpnxdge6wxhny4sxq7ivgtrzm3wu/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/sqlite-3.39.4-qvvuyhgy3ecf3qxeunximov5bljwisuh/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/adios2-2.8.3-7rark635o4stmbd5d5as2q5l54m7q2nv/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mpich-3.3.2-ckeytik2v4h4opvpxupznbyjsdig7ybq/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/yaml-cpp-0.7.0-ilml5ydu247xw63eeoscufdhwnvbr3nc/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mochi-margo-0.10-yz6ktxq5dukvdog7rckunhldoccfeay3/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mercury-2.1.0-cioky3lb7vjzpdwqnamgiezvwic6tsfc/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/argobots-1.1-55wbecmg5firhbezcdwgnibfy4dss4gh/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/json-c-0.16-3xtkooho3jkc6h4kniu7h52r4iubasic/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/boost-1.80.0-fsjd7e73bkecdkp56o6l4srmimwcvyln/lib")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/usr/local/lib/libhermes_engine.so")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/usr/local/lib" TYPE SHARED_LIBRARY FILES "/home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/cmake-build-debug/bin/libhermes_engine.so")
  if(EXISTS "$ENV{DESTDIR}/usr/local/lib/libhermes_engine.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/usr/local/lib/libhermes_engine.so")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}/usr/local/lib/libhermes_engine.so"
         OLD_RPATH "/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/hermes_shm-master-lagagphskmh4kdu75me4di6w7bgs3oe7/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/hermes-dev-1.1-kmd7tpnxdge6wxhny4sxq7ivgtrzm3wu/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/sqlite-3.39.4-qvvuyhgy3ecf3qxeunximov5bljwisuh/lib:/home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/cmake-build-debug/bin:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/adios2-2.8.3-7rark635o4stmbd5d5as2q5l54m7q2nv/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mpich-3.3.2-ckeytik2v4h4opvpxupznbyjsdig7ybq/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/yaml-cpp-0.7.0-ilml5ydu247xw63eeoscufdhwnvbr3nc/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mochi-margo-0.10-yz6ktxq5dukvdog7rckunhldoccfeay3/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mercury-2.1.0-cioky3lb7vjzpdwqnamgiezvwic6tsfc/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/argobots-1.1-55wbecmg5firhbezcdwgnibfy4dss4gh/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/json-c-0.16-3xtkooho3jkc6h4kniu7h52r4iubasic/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/boost-1.80.0-fsjd7e73bkecdkp56o6l4srmimwcvyln/lib:"
         NEW_RPATH "/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/hermes_shm-master-lagagphskmh4kdu75me4di6w7bgs3oe7/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/hermes-dev-1.1-kmd7tpnxdge6wxhny4sxq7ivgtrzm3wu/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/sqlite-3.39.4-qvvuyhgy3ecf3qxeunximov5bljwisuh/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/adios2-2.8.3-7rark635o4stmbd5d5as2q5l54m7q2nv/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mpich-3.3.2-ckeytik2v4h4opvpxupznbyjsdig7ybq/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/yaml-cpp-0.7.0-ilml5ydu247xw63eeoscufdhwnvbr3nc/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mochi-margo-0.10-yz6ktxq5dukvdog7rckunhldoccfeay3/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mercury-2.1.0-cioky3lb7vjzpdwqnamgiezvwic6tsfc/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/argobots-1.1-55wbecmg5firhbezcdwgnibfy4dss4gh/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/json-c-0.16-3xtkooho3jkc6h4kniu7h52r4iubasic/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/boost-1.80.0-fsjd7e73bkecdkp56o6l4srmimwcvyln/lib")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}/usr/local/lib/libhermes_engine.so")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
endif()

