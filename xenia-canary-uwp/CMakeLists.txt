# The UWP/Xbox app target is a hand-maintained Visual Studio project --
# AppContainer packaging (Package.appxmanifest, cert signing, WinRT/C++
# component compilation) isn't something CMake's own target model
# expresses. Fold it into the generated solution as an external project so
# "Build Solution" and the IDE's project dependencies still cover it.
#
# xenia-canary-uwp.vcxproj links the dependency libraries below directly
# (AdditionalDependencies from build/obj/Windows/$(Configuration), not
# ProjectReference) so it isn't tied to CMake's per-regeneration project
# GUIDs; add_dependencies() below just orders the solution build.
if(WIN32 AND CMAKE_GENERATOR MATCHES "Visual Studio")
  include_external_msproject(xenia-canary-uwp
    ${CMAKE_CURRENT_SOURCE_DIR}/xenia-canary-uwp.vcxproj
    PLATFORM x64
  )
  add_dependencies(xenia-canary-uwp
    xenia-apu
    xenia-apu-nop
    xenia-apu-xaudio2
    xenia-base
    xenia-core
    xenia-cpu
    xenia-cpu-backend-x64
    xenia-debug-ui
    xenia-gpu
    xenia-gpu-d3d12
    xenia-gpu-null
    xenia-hid
    xenia-hid-nop
    xenia-hid-portal
    xenia-hid-xinput
    xenia-kernel
    xenia-patcher
    xenia-ui
    xenia-ui-d3d12
    xenia-vfs
    aes_128
    capstone
    discord-rpc
    dxbc
    fmt
    glslang-spirv
    imgui
    libavcodec
    libavformat
    libavutil
    libusb
    mspack
    pugixml
    snappy
    xxhash
    zarchive
    zlib-ng
    zstd
  )
endif()
