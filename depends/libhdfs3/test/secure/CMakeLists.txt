CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

AUTO_SOURCES(secure_SOURCES "*.cpp" "RECURSE" "${CMAKE_CURRENT_SOURCE_DIR}")

PROTOBUF_GENERATE_CPP(libhdfs3_PROTO_SOURCES libhdfs3_PROTO_HEADERS ${libhdfs3_PROTO_FILES})

IF(ENABLE_DEBUG STREQUAL ON)
    SET(libhdfs3_SOURCES ${libhdfs3_SOURCES} ${libhdfs3_MOCK_SOURCES})
ENDIF(ENABLE_DEBUG STREQUAL ON)

IF(NOT HDFS_SUPERUSER)
    SET(HDFS_SUPERUSER $ENV{USER})
ENDIF(NOT HDFS_SUPERUSER)

ADD_DEFINITIONS(-DHDFS_SUPERUSER="${HDFS_SUPERUSER}")
ADD_DEFINITIONS(-DUSER="$ENV{USER}")

ADD_EXECUTABLE(secure EXCLUDE_FROM_ALL
    ${gtest_SOURCES}
    ${gmock_SOURCES}
    ${libhdfs3_SOURCES} 
    ${libhdfs3_PROTO_SOURCES} 
    ${libhdfs3_PROTO_HEADERS}
    ${secure_SOURCES}
)

TARGET_INCLUDE_DIRECTORIES(secure PRIVATE ${libhdfs3_ROOT_SOURCES_DIR})
TARGET_INCLUDE_DIRECTORIES(secure PRIVATE ${libhdfs3_COMMON_SOURCES_DIR})
TARGET_INCLUDE_DIRECTORIES(secure PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
TARGET_INCLUDE_DIRECTORIES(secure PRIVATE ${libhdfs3_PLATFORM_HEADER_DIR})
TARGET_INCLUDE_DIRECTORIES(secure PRIVATE ${CMAKE_SOURCE_DIR}/mock)

TARGET_LINK_LIBRARIES(secure pthread)

TARGET_LINK_LIBRARIES(secure Boost::boost)
IF(NEED_BOOST)
    TARGET_LINK_LIBRARIES(secure Boost::thread)
    TARGET_LINK_LIBRARIES(secure Boost::chrono)
    TARGET_LINK_LIBRARIES(secure Boost::system)
    TARGET_LINK_LIBRARIES(secure Boost::atomic)
    TARGET_LINK_LIBRARIES(secure Boost::iostreams)
ENDIF(NEED_BOOST)

IF(NEED_GCCEH)
	TARGET_LINK_LIBRARIES(secure gcc_eh)
ENDIF(NEED_GCCEH)

IF(OS_LINUX)
	TARGET_LINK_LIBRARIES(secure LibUUID::LibUUID)
ENDIF(OS_LINUX)

TARGET_LINK_LIBRARIES(secure protobuf::libprotobuf)
TARGET_LINK_LIBRARIES(secure LibXml2::LibXml2)
TARGET_LINK_LIBRARIES(secure KERBEROS::KERBEROS)
TARGET_LINK_LIBRARIES(secure GSASL::GSASL)
TARGET_LINK_LIBRARIES(secure GoogleTest::GoogleTest)

SET(secure_SOURCES ${secure_SOURCES} PARENT_SCOPE)

