FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y build-essential cmake curl libasio-dev ca-certificates

RUN curl -fL -o /usr/local/include/crow_all.h https://github.com/CrowCpp/Crow/releases/download/v1.3.4/crow_all.h

WORKDIR /app
COPY CMakeLists.txt .
COPY src src

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build

EXPOSE 8080
CMD ["./build/search_server"]