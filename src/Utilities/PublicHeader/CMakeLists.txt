add_library(Utility_PublicHeader
        String.cpp Network.cpp FdFunctions.cpp PublicHeader.cpp
        include/crane/String.h
        include/crane/Network.h
        include/crane/FdFunctions.h
        include/crane/PublicHeader.h
        include/crane/BoostInterprocessBarrier.h
        include/crane/Lock.h
        include/crane/Pointer.h
        include/crane/Logger.h
        )
target_include_directories(Utility_PublicHeader PUBLIC include)
target_link_libraries(Utility_PublicHeader PUBLIC
        Boost::boost # Boost added here for String.cpp
        spdlog::spdlog
        crane_proto_lib)

# This trimmed version is used for PAM module
# since -fPIC flag should be enabled for a dynamic library
# and we do not expect the performance to be affected by it.
add_library(Utility_PublicHeaderNoLogger
        include/crane/PublicHeader.h
        PublicHeader.cpp
        )
target_include_directories(Utility_PublicHeaderNoLogger PUBLIC include)
target_link_libraries(Utility_PublicHeaderNoLogger PUBLIC crane_proto_lib)
