# Image du serveur seul (aucune dépendance graphique)
FROM debian:bookworm-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends g++ cmake make && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt .
COPY src ./src
COPY tests ./tests
RUN cmake -S . -B build -DBUILD_CLIENT=OFF -DCMAKE_BUILD_TYPE=Release && cmake --build build -j && ./build/impact_tests

FROM debian:bookworm-slim
COPY --from=build /src/build/impact_server /usr/local/bin/impact_server
EXPOSE 7777/udp
ENTRYPOINT ["impact_server"]
CMD ["7777"]
