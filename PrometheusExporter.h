// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

// Converts the health check JSON tree (as produced by GeneveHandlerHealthCheck::output_json()) into Prometheus
// text exposition format.
#ifndef GWLBTUN_PROMETHEUSEXPORTER_H
#define GWLBTUN_PROMETHEUSEXPORTER_H

#include <string>
#include "json.hpp"
using json = nlohmann::json;

/**
 * Converts the JSON tree produced by GeneveHandlerHealthCheck::output_json() into Prometheus text exposition
 * format (https://prometheus.io/docs/instrumenting/exposition_formats/).
 *
 * @param ghhc JSON as produced by GeneveHandlerHealthCheck::output_json().
 * @param overallHealthy Overall health status, as reported separately via GeneveHandler::healthy.
 * @return Prometheus text exposition format, ready to be used as the body of a /metrics scrape response.
 */
std::string healthCheckToPrometheus(json &ghhc, bool overallHealthy);

#endif //GWLBTUN_PROMETHEUSEXPORTER_H
