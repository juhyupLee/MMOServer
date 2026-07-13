# syntax=docker/dockerfile:1.7

FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        g++-14 \
        libboost-dev \
        ninja-build \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B /build -G Ninja \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_CXX_COMPILER=g++-14 \
        -DBUILD_TESTING=ON \
    && cmake --build /build --parallel 4 \
    && ctest --test-dir /build --output-on-failure

FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        bash \
        binutils \
        ca-certificates \
        gdb \
        iproute2 \
        libgcc-s1 \
        libstdc++6 \
        procps \
        python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/mmo
COPY --from=build /build/PlayServer/PlayServer ./bin/PlayServer
COPY --from=build /build/SoakClient/SoakClient ./bin/SoakClient
COPY --from=build /build/SoakClient/ProtocolProbe ./bin/ProtocolProbe
COPY scripts ./scripts
COPY troubleshoot.md AGENTS.md ./

RUN chmod +x ./bin/* ./scripts/*.sh

EXPOSE 7777
CMD ["./bin/PlayServer", "--port", "7777"]
