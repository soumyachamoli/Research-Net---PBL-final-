FROM debian:bookworm-slim AS build
RUN apt-get update \
 && apt-get install -y --no-install-recommends g++ libpq-dev ca-certificates \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY build.sh ./
COPY include include
COPY src src
COPY third_party third_party
COPY tests tests
RUN chmod +x build.sh && ./build.sh test && JOBS=1 ./build.sh

FROM debian:bookworm-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends libpq5 ca-certificates \
 && rm -rf /var/lib/apt/lists/* \
 && useradd --system --no-create-home researchnet
WORKDIR /app
COPY --from=build /app/researchnet ./researchnet
COPY web web
USER researchnet
ENV HOST=0.0.0.0 PORT=8080 COOKIE_SECURE=1 TRUST_PROXY=1
EXPOSE 8080
CMD ["./researchnet"]
