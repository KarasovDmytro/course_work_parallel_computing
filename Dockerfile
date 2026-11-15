FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y build-essential cmake curl libasio-dev libcurl4-openssl-dev nlohmann-json3-dev ca-certificates

RUN curl -fL -o /usr/local/include/crow_all.h https://github.com/CrowCpp/Crow/releases/download/v1.3.4/crow_all.h

WORKDIR /app
COPY CMakeLists.txt .
COPY src src
COPY clients clients

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
RUN ln -s /app/build/search /usr/local/bin/search

EXPOSE 8080
CMD ["./build/search_server"]