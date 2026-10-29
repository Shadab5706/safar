# Safar frontend: the Vite build served by nginx, which also proxies /api to the backend.
# Build from the repo root:  docker build -f docker/frontend.Dockerfile -t safar-web .

# ---- 1. Build the static site ----
FROM node:22-trixie-slim AS build

WORKDIR /app
COPY frontend/package.json frontend/package-lock.json ./
RUN npm ci --no-audit --no-fund
COPY frontend/ ./
RUN npm run build

# ---- 2. Serve it (nginx as a non-root user, listening on 8080) ----
FROM nginxinc/nginx-unprivileged:1.30-alpine AS runtime

COPY docker/nginx/default.conf.template /etc/nginx/templates/default.conf.template
COPY --from=build /app/dist /usr/share/nginx/html

# SAFAR_API_URL: where nginx sends /api requests (scheme + host, no path).
# NGINX_ENTRYPOINT_LOCAL_RESOLVERS makes the entrypoint fill ${NGINX_LOCAL_RESOLVERS}
# from /etc/resolv.conf.
ENV SAFAR_API_URL=http://backend:8080 \
    NGINX_ENTRYPOINT_LOCAL_RESOLVERS=1

EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
  CMD wget -qO /dev/null http://127.0.0.1:8080/healthz || exit 1
