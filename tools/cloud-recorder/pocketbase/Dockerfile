# PocketBase for the OTGW diagnostic collection point (TASK-1938).
# The binary comes from the PocketBase GitHub release and is checked against
# the release's own checksums.txt before it is unpacked.
FROM alpine:3.20

ARG PB_VERSION=0.40.6
# amd64 for Intel/AMD Synology models, arm64 for ARMv8 models.
ARG PB_ARCH=amd64

RUN apk add --no-cache ca-certificates unzip wget \
 && cd /tmp \
 && base="https://github.com/pocketbase/pocketbase/releases/download/v${PB_VERSION}" \
 && zip="pocketbase_${PB_VERSION}_linux_${PB_ARCH}.zip" \
 && wget -q "${base}/${zip}" "${base}/checksums.txt" \
 && grep " ${zip}\$" checksums.txt | sha256sum -c - \
 && mkdir -p /pb \
 && unzip -q "${zip}" pocketbase -d /pb \
 && rm -f /tmp/*.zip /tmp/checksums.txt \
 && apk del unzip wget

EXPOSE 8090
CMD ["/pb/pocketbase", "serve", "--http=0.0.0.0:8090", "--dir=/pb/pb_data", "--hooksDir=/pb/pb_hooks", "--migrationsDir=/pb/pb_migrations"]
