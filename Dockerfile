# syntax=docker/dockerfile:1
#
# Builds gwlbtun from source inside an amazonlinux:2023 container so the
# linked glibc matches the Amazon Linux hosts this handler runs on, then
# copies just the binary into a clean runtime image.
#
# Built via `docker buildx build --platform linux/amd64,linux/arm64 ...`;
# each stage's base image and RUN steps execute natively or under QEMU
# emulation for the target platform, so no cross-compilation setup is needed.
FROM amazonlinux:2023 AS builder

ARG BOOST_VERSION=1.83.0

# Note: amazonlinux:2023 ships curl-minimal (which provides the `curl` command);
# installing the full `curl` package conflicts with it, so we don't request it.
RUN dnf install -y \
        cmake \
        gcc gcc-c++ \
        make \
        tar \
        gzip \
        findutils \
    && dnf clean all

WORKDIR /workspace
COPY . .

RUN set -eux; \
    BOOST_UNDERSCORE=$(echo "${BOOST_VERSION}" | tr . _); \
    mkdir -p /tmp/boost; \
    curl -fsSL "https://archives.boost.io/release/${BOOST_VERSION}/source/boost_${BOOST_UNDERSCORE}.tar.gz" \
        | tar xz -C /tmp/boost --strip-components=1; \
    cmake -S . -B /tmp/build -DCMAKE_BUILD_TYPE=Release -DBoost_INCLUDE_DIR=/tmp/boost; \
    cmake --build /tmp/build -j"$(nproc)"; \
    install -m 0755 /tmp/build/gwlbtun /usr/local/bin/gwlbtun

# Runtime base matches the builder so the linked glibc/libstdc++ versions
# line up with what the binary was compiled against.
FROM amazonlinux:2023

COPY --from=builder /usr/local/bin/gwlbtun /usr/local/bin/gwlbtun

ENTRYPOINT ["/usr/local/bin/gwlbtun"]
