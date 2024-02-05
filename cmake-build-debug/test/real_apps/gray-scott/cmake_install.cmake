# Install script for directory: /home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/test/real_apps/gray-scott

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
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/adios2-gray-scott" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/adios2-gray-scott")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/adios2-gray-scott"
         RPATH "/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/hermes_shm-master-lagagphskmh4kdu75me4di6w7bgs3oe7/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/hermes-dev-1.1-kmd7tpnxdge6wxhny4sxq7ivgtrzm3wu/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/adios2-2.8.3-7rark635o4stmbd5d5as2q5l54m7q2nv/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mpich-3.3.2-ckeytik2v4h4opvpxupznbyjsdig7ybq/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/yaml-cpp-0.7.0-ilml5ydu247xw63eeoscufdhwnvbr3nc/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mochi-margo-0.10-yz6ktxq5dukvdog7rckunhldoccfeay3/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mercury-2.1.0-cioky3lb7vjzpdwqnamgiezvwic6tsfc/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/argobots-1.1-55wbecmg5firhbezcdwgnibfy4dss4gh/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/json-c-0.16-3xtkooho3jkc6h4kniu7h52r4iubasic/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/boost-1.80.0-fsjd7e73bkecdkp56o6l4srmimwcvyln/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/sqlite-3.39.4-qvvuyhgy3ecf3qxeunximov5bljwisuh/lib")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/cmake-build-debug/bin/adios2-gray-scott")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/adios2-gray-scott" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/adios2-gray-scott")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/adios2-gray-scott"
         OLD_RPATH "/home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/cmake-build-debug/bin:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/hermes_shm-master-lagagphskmh4kdu75me4di6w7bgs3oe7/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/hermes-dev-1.1-kmd7tpnxdge6wxhny4sxq7ivgtrzm3wu/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/adios2-2.8.3-7rark635o4stmbd5d5as2q5l54m7q2nv/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mpich-3.3.2-ckeytik2v4h4opvpxupznbyjsdig7ybq/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/yaml-cpp-0.7.0-ilml5ydu247xw63eeoscufdhwnvbr3nc/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mochi-margo-0.10-yz6ktxq5dukvdog7rckunhldoccfeay3/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mercury-2.1.0-cioky3lb7vjzpdwqnamgiezvwic6tsfc/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/argobots-1.1-55wbecmg5firhbezcdwgnibfy4dss4gh/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/json-c-0.16-3xtkooho3jkc6h4kniu7h52r4iubasic/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/boost-1.80.0-fsjd7e73bkecdkp56o6l4srmimwcvyln/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/sqlite-3.39.4-qvvuyhgy3ecf3qxeunximov5bljwisuh/lib:"
         NEW_RPATH "/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/hermes_shm-master-lagagphskmh4kdu75me4di6w7bgs3oe7/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/hermes-dev-1.1-kmd7tpnxdge6wxhny4sxq7ivgtrzm3wu/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/adios2-2.8.3-7rark635o4stmbd5d5as2q5l54m7q2nv/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mpich-3.3.2-ckeytik2v4h4opvpxupznbyjsdig7ybq/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/yaml-cpp-0.7.0-ilml5ydu247xw63eeoscufdhwnvbr3nc/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mochi-margo-0.10-yz6ktxq5dukvdog7rckunhldoccfeay3/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mercury-2.1.0-cioky3lb7vjzpdwqnamgiezvwic6tsfc/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/argobots-1.1-55wbecmg5firhbezcdwgnibfy4dss4gh/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/json-c-0.16-3xtkooho3jkc6h4kniu7h52r4iubasic/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/boost-1.80.0-fsjd7e73bkecdkp56o6l4srmimwcvyln/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/sqlite-3.39.4-qvvuyhgy3ecf3qxeunximov5bljwisuh/lib")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/adios2-gray-scott")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/adios2-pdf-calc" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/adios2-pdf-calc")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/adios2-pdf-calc"
         RPATH "/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/hermes_shm-master-lagagphskmh4kdu75me4di6w7bgs3oe7/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/hermes-dev-1.1-kmd7tpnxdge6wxhny4sxq7ivgtrzm3wu/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/adios2-2.8.3-7rark635o4stmbd5d5as2q5l54m7q2nv/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mpich-3.3.2-ckeytik2v4h4opvpxupznbyjsdig7ybq/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/yaml-cpp-0.7.0-ilml5ydu247xw63eeoscufdhwnvbr3nc/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mochi-margo-0.10-yz6ktxq5dukvdog7rckunhldoccfeay3/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mercury-2.1.0-cioky3lb7vjzpdwqnamgiezvwic6tsfc/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/argobots-1.1-55wbecmg5firhbezcdwgnibfy4dss4gh/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/json-c-0.16-3xtkooho3jkc6h4kniu7h52r4iubasic/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/boost-1.80.0-fsjd7e73bkecdkp56o6l4srmimwcvyln/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/sqlite-3.39.4-qvvuyhgy3ecf3qxeunximov5bljwisuh/lib")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/cmake-build-debug/bin/adios2-pdf-calc")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/adios2-pdf-calc" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/adios2-pdf-calc")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/adios2-pdf-calc"
         OLD_RPATH "/home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/cmake-build-debug/bin:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/hermes_shm-master-lagagphskmh4kdu75me4di6w7bgs3oe7/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/hermes-dev-1.1-kmd7tpnxdge6wxhny4sxq7ivgtrzm3wu/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/adios2-2.8.3-7rark635o4stmbd5d5as2q5l54m7q2nv/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mpich-3.3.2-ckeytik2v4h4opvpxupznbyjsdig7ybq/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/yaml-cpp-0.7.0-ilml5ydu247xw63eeoscufdhwnvbr3nc/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mochi-margo-0.10-yz6ktxq5dukvdog7rckunhldoccfeay3/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mercury-2.1.0-cioky3lb7vjzpdwqnamgiezvwic6tsfc/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/argobots-1.1-55wbecmg5firhbezcdwgnibfy4dss4gh/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/json-c-0.16-3xtkooho3jkc6h4kniu7h52r4iubasic/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/boost-1.80.0-fsjd7e73bkecdkp56o6l4srmimwcvyln/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/sqlite-3.39.4-qvvuyhgy3ecf3qxeunximov5bljwisuh/lib:"
         NEW_RPATH "/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/hermes_shm-master-lagagphskmh4kdu75me4di6w7bgs3oe7/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/hermes-dev-1.1-kmd7tpnxdge6wxhny4sxq7ivgtrzm3wu/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/adios2-2.8.3-7rark635o4stmbd5d5as2q5l54m7q2nv/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mpich-3.3.2-ckeytik2v4h4opvpxupznbyjsdig7ybq/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/yaml-cpp-0.7.0-ilml5ydu247xw63eeoscufdhwnvbr3nc/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mochi-margo-0.10-yz6ktxq5dukvdog7rckunhldoccfeay3/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/mercury-2.1.0-cioky3lb7vjzpdwqnamgiezvwic6tsfc/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/argobots-1.1-55wbecmg5firhbezcdwgnibfy4dss4gh/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/json-c-0.16-3xtkooho3jkc6h4kniu7h52r4iubasic/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/boost-1.80.0-fsjd7e73bkecdkp56o6l4srmimwcvyln/lib:/home/lukemartinlogan/Documents/Projects/PhD/spack/opt/spack/linux-linuxmint20-zen2/gcc-9.4.0/sqlite-3.39.4-qvvuyhgy3ecf3qxeunximov5bljwisuh/lib")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/adios2-pdf-calc")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/usr/local/share/adios2-examples/gray-scott/adios2.xml;/usr/local/share/adios2-examples/gray-scott/adios2-inline-plugin.xml;/usr/local/share/adios2-examples/gray-scott/adios2-fides-staging.xml;/usr/local/share/adios2-examples/gray-scott/visit-bp4.session;/usr/local/share/adios2-examples/gray-scott/visit-bp4.session.gui;/usr/local/share/adios2-examples/gray-scott/visit-sst.session;/usr/local/share/adios2-examples/gray-scott/visit-sst.session.gui;/usr/local/share/adios2-examples/gray-scott/settings-files.json;/usr/local/share/adios2-examples/gray-scott/settings-staging.json;/usr/local/share/adios2-examples/gray-scott/settings-inline.json;/usr/local/share/adios2-examples/gray-scott/decomp.py;/usr/local/share/adios2-examples/gray-scott/gsplot.py;/usr/local/share/adios2-examples/gray-scott/pdfplot.py;/usr/local/share/adios2-examples/gray-scott/README.md")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/usr/local/share/adios2-examples/gray-scott" TYPE FILE FILES
    "/home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/test/real_apps/gray-scott/adios2.xml"
    "/home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/test/real_apps/gray-scott/adios2-inline-plugin.xml"
    "/home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/test/real_apps/gray-scott/adios2-fides-staging.xml"
    "/home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/test/real_apps/gray-scott/visit-bp4.session"
    "/home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/test/real_apps/gray-scott/visit-bp4.session.gui"
    "/home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/test/real_apps/gray-scott/visit-sst.session"
    "/home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/test/real_apps/gray-scott/visit-sst.session.gui"
    "/home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/test/real_apps/gray-scott/simulation/settings-files.json"
    "/home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/test/real_apps/gray-scott/simulation/settings-staging.json"
    "/home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/test/real_apps/gray-scott/simulation/settings-inline.json"
    "/home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/test/real_apps/gray-scott/plot/decomp.py"
    "/home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/test/real_apps/gray-scott/plot/gsplot.py"
    "/home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/test/real_apps/gray-scott/plot/pdfplot.py"
    "/home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/test/real_apps/gray-scott/README.md"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/usr/local/share/adios2-examples/gray-scott/catalyst")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/usr/local/share/adios2-examples/gray-scott" TYPE DIRECTORY FILES "/home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/test/real_apps/gray-scott/catalyst")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/usr/local/share/adios2-examples/gray-scott/cleanup.sh")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/usr/local/share/adios2-examples/gray-scott" TYPE PROGRAM FILES "/home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/test/real_apps/gray-scott/cleanup.sh")
endif()

