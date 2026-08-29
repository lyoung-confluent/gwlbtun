# AWS Gateway Load Balancer (GWLB) Handler - gwlbtun
This is a fork of [aws-samples/aws-gateway-load-balancer-tunnel-handler](https://github.com/aws-samples/aws-gateway-load-balancer-tunnel-handler), with a number of fixes/improvements that aren't yet upstream:

- [#36 fix: close tun queue fds and raw socket on ENI teardown](https://github.com/aws-samples/aws-gateway-load-balancer-tunnel-handler/pull/36)
- [#35 feat: register shutdown handler for SIGTERM](https://github.com/aws-samples/aws-gateway-load-balancer-tunnel-handler/pull/35)
- [#34 fix: GeneveHandler::healthy is never updated, so it stays permanently true](https://github.com/aws-samples/aws-gateway-load-balancer-tunnel-handler/pull/34)
- [#33 fix: compiler warnings under -Wall -Wextra](https://github.com/aws-samples/aws-gateway-load-balancer-tunnel-handler/pull/33)
- [#32 fix: unchecked and incorrect socket/write return-value handling](https://github.com/aws-samples/aws-gateway-load-balancer-tunnel-handler/pull/32)
- [#31 fix: add per-option bounds check to Geneve options parser](https://github.com/aws-samples/aws-gateway-load-balancer-tunnel-handler/pull/31)
- [#30 fix: unlikely stack buffer overflow in sendUdp](https://github.com/aws-samples/aws-gateway-load-balancer-tunnel-handler/pull/30)
- [#29 fix: uninitialized healthSocket can cause busy-loop when -p is not supplied](https://github.com/aws-samples/aws-gateway-load-balancer-tunnel-handler/pull/29)
- [#28 fix: sendUdp leaks stack data via UDP checksum](https://github.com/aws-samples/aws-gateway-load-balancer-tunnel-handler/pull/28)
- [#27 feat(metrics): add Prometheus text exposition format](https://github.com/aws-samples/aws-gateway-load-balancer-tunnel-handler/pull/27)
- [#26 fix(metrics): malformed JSON from FlowCacheHealthCheck::output_json()](https://github.com/aws-samples/aws-gateway-load-balancer-tunnel-handler/pull/26)
- [#24 fix(cmake): remove hardcoded Boost include path](https://github.com/aws-samples/aws-gateway-load-balancer-tunnel-handler/pull/24)

For build instructions, usage, and everything else, see the [upstream README](https://github.com/aws-samples/aws-gateway-load-balancer-tunnel-handler#readme) — this fork doesn't change any of that.

## Docker/OCI Image
This fork has GitHub Actions pipeline ([.github/workflows/build.yml](.github/workflows/build.yml)) that builds `gwlbtun` natively for `amd64`/`arm64` on AL2023. Every push to `main` publishes a multi-arch Docker image [to GHCR](https://github.com/lyoung-confluent/gwlbtun/pkgs/container/gwlbtun):

```bash
docker run --rm --cap-add=NET_ADMIN --device=/dev/net/tun -p 80:80 \
  ghcr.io/lyoung-confluent/gwlbtun:latest -p 80
```

`gwlbtun` needs `CAP_NET_ADMIN` and access to `/dev/net/tun` to create tunnel interfaces; pass any of its normal CLI options after the image name.
