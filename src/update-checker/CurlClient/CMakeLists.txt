add_library(CurlClient STATIC CurlClient.cpp)
target_link_libraries(CurlClient PRIVATE OBS::libobs plugin-support CURL::libcurl)
target_include_directories(CurlClient PRIVATE "${CMAKE_SOURCE_DIR}/src")
