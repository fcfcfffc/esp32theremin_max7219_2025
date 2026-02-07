# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "D:/esp/v5.4.1/esp-idf/components/bootloader/subproject")
  file(MAKE_DIRECTORY "D:/esp/v5.4.1/esp-idf/components/bootloader/subproject")
endif()
file(MAKE_DIRECTORY
<<<<<<< HEAD
  "E:/PlatformIO/Projects/esp32theremin/build/bootloader"
  "E:/PlatformIO/Projects/esp32theremin/build/bootloader-prefix"
  "E:/PlatformIO/Projects/esp32theremin/build/bootloader-prefix/tmp"
  "E:/PlatformIO/Projects/esp32theremin/build/bootloader-prefix/src/bootloader-stamp"
  "E:/PlatformIO/Projects/esp32theremin/build/bootloader-prefix/src"
  "E:/PlatformIO/Projects/esp32theremin/build/bootloader-prefix/src/bootloader-stamp"
=======
  "C:/Users/fcfcf/Documents/PlatformIO/Projects/esp32theremin/build/bootloader"
  "C:/Users/fcfcf/Documents/PlatformIO/Projects/esp32theremin/build/bootloader-prefix"
  "C:/Users/fcfcf/Documents/PlatformIO/Projects/esp32theremin/build/bootloader-prefix/tmp"
  "C:/Users/fcfcf/Documents/PlatformIO/Projects/esp32theremin/build/bootloader-prefix/src/bootloader-stamp"
  "C:/Users/fcfcf/Documents/PlatformIO/Projects/esp32theremin/build/bootloader-prefix/src"
  "C:/Users/fcfcf/Documents/PlatformIO/Projects/esp32theremin/build/bootloader-prefix/src/bootloader-stamp"
>>>>>>> c1f70764133a0778c4c36cfeb24ff696bc39affb
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
<<<<<<< HEAD
    file(MAKE_DIRECTORY "E:/PlatformIO/Projects/esp32theremin/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "E:/PlatformIO/Projects/esp32theremin/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
=======
    file(MAKE_DIRECTORY "C:/Users/fcfcf/Documents/PlatformIO/Projects/esp32theremin/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/fcfcf/Documents/PlatformIO/Projects/esp32theremin/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
>>>>>>> c1f70764133a0778c4c36cfeb24ff696bc39affb
endif()
