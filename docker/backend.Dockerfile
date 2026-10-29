# Safar backend: C++20 REST server.
# Build from the repo root:  docker build -f docker/backend.Dockerfile -t safar-backend .

# ---- 1. Build: compile with CMake + Ninja and run the unit tests ----
FROM debian:trixie AS build

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build ca-certificates python3 \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
# Tests read the processed data, the fare samples and scripts/fit_fares.py.
COPY backend/ backend/
COPY data/processed/ data/processed/
COPY data/fare_samples.csv data/
COPY scripts/ scripts/

# Parallel compile jobs. Kept low so a laptop build does not max out the CPU.
ARG BUILD_JOBS=2

RUN cmake -S backend -B backend/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
 && cmake --build backend/build --parallel "${BUILD_JOBS}" \
 && ctest --test-dir backend/build --output-on-failure --parallel "${BUILD_JOBS}" \
 && strip backend/build/safar_server

# ---- 2. Runtime: only the server binary, its config and the processed data ----
FROM debian:trixie-slim AS runtime

# curl is only for the container healthcheck.
RUN apt-get update \
 && apt-get install -y --no-install-recommends curl \
 && rm -rf /var/lib/apt/lists/* \
 && useradd --system --uid 10001 --no-create-home --shell /usr/sbin/nologin safar

WORKDIR /app
COPY --from=build /src/backend/build/safar_server /usr/local/bin/safar_server
COPY backend/config/ backend/config/
COPY data/processed/ data/processed/

# server.json resolves data_dir ("../../data/processed") against its own folder,
# so the layout above is all it needs. Any key can be overridden with SAFAR_* env vars.
ENV SAFAR_HOST=0.0.0.0 \
    SAFAR_PORT=8080 \
    SAFAR_CORS_ORIGIN=http://localhost:8080 \
    SAFAR_LOG_LEVEL=info

USER safar
EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=3s --start-period=10s --retries=3 \
  CMD curl -fsS "http://127.0.0.1:${SAFAR_PORT}/api/v1/health" > /dev/null || exit 1

CMD ["safar_server", "/app/backend/config/server.json"]
