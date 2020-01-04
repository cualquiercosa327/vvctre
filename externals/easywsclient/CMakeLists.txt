add_library(easywsclient
    source/easywsclient.cpp
    include/easywsclient.hpp
)

target_include_directories(easywsclient PUBLIC ./include)
