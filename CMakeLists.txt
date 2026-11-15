cmake_minimum_required(VERSION 3.16)
project(search_server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

include_directories(src)

add_executable(search_server
    src/app/main.cpp
    src/common/config.cpp
    src/common/jsonResponse.cpp
    src/common/fileReader.cpp
    src/concurrency/threadPool.cpp
    src/text/stemmer.cpp
    src/text/tokenizer.cpp
    src/index/postingList.cpp
    src/index/termHash.cpp
    src/index/shard.cpp
    src/index/invertedIndex.cpp
    src/indexing/indexBuilder.cpp
    src/indexing/indexingController.cpp
    src/search/searchService.cpp
    src/search/searchController.cpp
    src/search/searchJson.cpp
    src/search/wsSearchController.cpp
    src/llm/ollamaClient.cpp
    src/llm/queryExpander.cpp
    src/health/healthController.cpp
)
target_link_libraries(search_server Threads::Threads CURL::libcurl nlohmann_json::nlohmann_json)