cmake_minimum_required(VERSION 3.20)
project(Nebula10Native VERSION 2.1.0 LANGUAGES CXX)
add_compile_definitions(_WIN32_WINNT=0x0A00 WINVER=0x0A00)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Release)
foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  string(TOUPPER ${cfg} CFG)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG} ${CMAKE_BINARY_DIR}/Release)
endforeach()
if(MINGW)
  add_compile_options(-Wall -Wextra -Wpedantic)
  add_link_options(-static -static-libgcc -static-libstdc++)
  function(nebula_console target)
    target_link_options(${target} PRIVATE -municode)
  endfunction()
else()
  function(nebula_console target)
  endfunction()
endif()
add_executable(n10ver src/n10ver.cpp)
nebula_console(n10ver)
add_custom_command(TARGET n10ver POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/verinfo.bin $<TARGET_FILE_DIR:n10ver>/verinfo.bin)
add_executable(n10toolbox src/n10toolbox.cpp)
nebula_console(n10toolbox)
target_link_libraries(n10toolbox PRIVATE shell32 advapi32)
add_executable(n10forceown src/n10forceown.cpp)
nebula_console(n10forceown)
target_link_libraries(n10forceown PRIVATE shell32 advapi32)
add_executable(n10themes src/n10themes.cpp)
nebula_console(n10themes)
target_link_libraries(n10themes PRIVATE shell32 advapi32)
add_library(NebulaForceOwnShell SHARED src/forceown_shell.cpp)
set_target_properties(NebulaForceOwnShell PROPERTIES PREFIX "" OUTPUT_NAME "NebulaForceOwnShell")
target_link_libraries(NebulaForceOwnShell PRIVATE shell32 ole32 uuid)
set(N10STORE_PAYLOAD "${CMAKE_SOURCE_DIR}/payload/N10Store.exe")
set(N10STORE_OUTPUT "${CMAKE_BINARY_DIR}/Release/N10Store.exe")
add_custom_command(
  OUTPUT "${N10STORE_OUTPUT}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/Release"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${N10STORE_PAYLOAD}" "${N10STORE_OUTPUT}"
  DEPENDS "${N10STORE_PAYLOAD}"
  COMMENT "Copying preserved N10Store.exe payload")
add_custom_target(N10Store ALL DEPENDS "${N10STORE_OUTPUT}")
add_executable(NebulaUserAuth src/user_auth.cpp)
nebula_console(NebulaUserAuth)
target_link_libraries(NebulaUserAuth PRIVATE advapi32)
add_executable(NebulaUserAuthService src/auth_service.cpp)
nebula_console(NebulaUserAuthService)
target_link_libraries(NebulaUserAuthService PRIVATE advapi32)
add_executable(NebulaSetup src/setup.cpp)
nebula_console(NebulaSetup)
target_link_libraries(NebulaSetup PRIVATE shell32 ole32 oleaut32 uuid advapi32 wbemuuid)
add_custom_command(TARGET NebulaSetup POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:NebulaSetup>/Tools
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/../tools/choco" "$<TARGET_FILE_DIR:NebulaSetup>/Tools/choco"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/../tools/Mem Reduct" "$<TARGET_FILE_DIR:NebulaSetup>/Tools/Mem Reduct"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/../tools/OpenShell" "$<TARGET_FILE_DIR:NebulaSetup>/Tools/OpenShell"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/../tools/WinXShell" "$<TARGET_FILE_DIR:NebulaSetup>/Tools/WinXShell"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/../tools/dwmblurglass" "$<TARGET_FILE_DIR:NebulaSetup>/Tools/dwmblurglass"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/../tools/Explorer++.exe" "$<TARGET_FILE_DIR:NebulaSetup>/Tools/Explorer++.exe"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/../tools/ShutUp10.exe" "$<TARGET_FILE_DIR:NebulaSetup>/Tools/ShutUp10.exe"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/../tools/neofetch.exe" "$<TARGET_FILE_DIR:NebulaSetup>/Tools/neofetch.exe"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/theme_catalog/scripts" "$<TARGET_FILE_DIR:NebulaSetup>/ThemeUpdater"
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:NebulaSetup>/OfficialThemes/Wallpapers/Nebula-Official"
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:NebulaSetup>/OfficialThemes/Icons/Nebula-Official"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets/Wallpapers" "$<TARGET_FILE_DIR:NebulaSetup>/OfficialThemes/Wallpapers/Nebula-Official"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/assets/Themes/Icons/Nebula Official/README.txt" "$<TARGET_FILE_DIR:NebulaSetup>/OfficialThemes/Icons/Nebula-Official/README.txt")
enable_testing()
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
 add_test(NAME policy_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/policy_tests.py)
 add_test(NAME smoke_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/smoke_tests.py ${CMAKE_BINARY_DIR}/Release)
 add_test(NAME feature_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/feature_tests.py ${CMAKE_BINARY_DIR}/Release)

 add_test(NAME store_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/store_tests.py ${CMAKE_BINARY_DIR}/Release)
 add_test(NAME theme_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/theme_tests.py ${CMAKE_BINARY_DIR}/Release)
 add_test(NAME simplification_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/simplification_tests.py)
endif()
install(TARGETS n10ver n10toolbox n10forceown n10themes NebulaForceOwnShell NebulaUserAuth NebulaUserAuthService NebulaSetup RUNTIME DESTINATION . LIBRARY DESTINATION .)
install(FILES "${N10STORE_PAYLOAD}" DESTINATION .)
install(FILES README.md SECURITY.md LICENSES.md verinfo.bin DESTINATION .)
