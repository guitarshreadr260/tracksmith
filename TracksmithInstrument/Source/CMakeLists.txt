cmake_minimum_required(VERSION 3.20)
project(TracksmithInstrument LANGUAGES CXX)
add_executable(TracksmithInstrument WIN32 main.cpp)
target_compile_features(TracksmithInstrument PRIVATE cxx_std_17)
target_compile_definitions(TracksmithInstrument PRIVATE UNICODE _UNICODE NOMINMAX)
target_link_libraries(TracksmithInstrument PRIVATE winmm comctl32 comdlg32)

