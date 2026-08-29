// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include "PrometheusExporter.h"
#include <sstream>
#include <vector>

using namespace std::string_literals;

// Prometheus label values must have backslash, double quote and newline backslash-escaped.
static std::string promEscape(const std::string &s)
{
    std::string ret;
    ret.reserve(s.size());
    for(char c : s)
    {
        switch(c)
        {
            case '\\': ret += "\\\\"; break;
            case '"':  ret += "\\\""; break;
            case '\n': ret += "\\n"; break;
            default:   ret += c;
        }
    }
    return ret;
}

static std::string label(const std::string &name, const std::string &value)
{
    return name + "=\"" + promEscape(value) + "\"";
}

// Emits one metric family (HELP/TYPE header plus all its sample lines). All samples for a metric name must be
// grouped together per the Prometheus text format spec, so this is only called once per metric name, after all
// samples for it have been collected.
static void emitFamily(std::stringstream &out, const std::string &name, const std::string &help, const std::string &type, const std::vector<std::string> &samples)
{
    if(samples.empty())
        return;

    out << "# HELP " << name << " " << help << "\n";
    out << "# TYPE " << name << " " << type << "\n";
    for(auto &s : samples)
        out << s << "\n";
}

std::string healthCheckToPrometheus(json &ghhc, bool overallHealthy)
{
    std::stringstream out;

    emitFamily(out, "gwlbtun_up", "Whether gwlbtun considers itself healthy (1) or not (0).", "gauge",
               {"gwlbtun_up "s + (overallHealthy ? "1" : "0")});

    if(ghhc.contains("udp") && ghhc["udp"].contains("UDPPacketReceiver"))
    {
        auto &udp = ghhc["udp"]["UDPPacketReceiver"];
        std::string port = std::to_string(udp.value("portNumber", 0));

        std::vector<std::string> up, pktsIn, bytesIn, secsSince;
        for(auto &t : udp.value("threads", json::array()))
        {
            std::string labels = "{"s + label("port", port) + ","s + label("thread", std::to_string(t.value("threadNumber", 0))) + "}"s;
            up.push_back("gwlbtun_udp_receiver_up"s + labels + " "s + (t.value("healthy", false) ? "1" : "0"));
            pktsIn.push_back("gwlbtun_udp_receiver_packets_in_total"s + labels + " "s + std::to_string(t.value("pktsIn", (uint64_t)0)));
            bytesIn.push_back("gwlbtun_udp_receiver_bytes_in_total"s + labels + " "s + std::to_string(t.value("bytesIn", (uint64_t)0)));
            secsSince.push_back("gwlbtun_udp_receiver_seconds_since_last_packet"s + labels + " "s + std::to_string(t.value("secsSinceLastPacket", 0.0)));
        }
        emitFamily(out, "gwlbtun_udp_receiver_up", "Whether the UDP receiver thread is healthy (1) or not (0).", "gauge", up);
        emitFamily(out, "gwlbtun_udp_receiver_packets_in_total", "Packets received by the UDP receiver thread.", "counter", pktsIn);
        emitFamily(out, "gwlbtun_udp_receiver_bytes_in_total", "Bytes received by the UDP receiver thread.", "counter", bytesIn);
        emitFamily(out, "gwlbtun_udp_receiver_seconds_since_last_packet", "Seconds since the UDP receiver thread last saw a packet.", "gauge", secsSince);
    }

    std::vector<std::string> tunPkts, tunBytes, tunSecs, tunThreadUp, tunThreadPkts, tunThreadBytes, tunThreadSecs;
    std::vector<std::string> cacheEntries, cacheTimedOut, cacheIdleTimeout;

    for(auto &eni : ghhc.value("enis", json::array()))
    {
        std::string eniStr = eni.value("eniStr", ""s);

        for(auto &dir : {"tunnelIn"s, "tunnelOut"s})
        {
            if(!eni.contains(dir))
                continue;

            auto &tun = eni[dir];
            std::string direction = (dir == "tunnelIn"s) ? "in"s : "out"s;
            std::string labels = "{"s + label("eni", eniStr) + ","s + label("direction", direction) + "}"s;

            tunPkts.push_back("gwlbtun_tunnel_packets_total"s + labels + " "s + std::to_string(tun.value("pktsOut", (uint64_t)0)));
            tunBytes.push_back("gwlbtun_tunnel_bytes_total"s + labels + " "s + std::to_string(tun.value("bytesOut", (uint64_t)0)));
            tunSecs.push_back("gwlbtun_tunnel_seconds_since_last_packet"s + labels + " "s + std::to_string(tun.value("secsSincelastPacket", 0.0)));

            for(auto &t : tun.value("threads", json::array()))
            {
                std::string tlabels = "{"s + label("eni", eniStr) + ","s + label("direction", direction) + ","s + label("thread", std::to_string(t.value("threadNumber", 0))) + "}"s;
                tunThreadUp.push_back("gwlbtun_tunnel_thread_up"s + tlabels + " "s + (t.value("healthy", false) ? "1" : "0"));
                tunThreadPkts.push_back("gwlbtun_tunnel_thread_packets_total"s + tlabels + " "s + std::to_string(t.value("pktsIn", (uint64_t)0)));
                tunThreadBytes.push_back("gwlbtun_tunnel_thread_bytes_total"s + tlabels + " "s + std::to_string(t.value("bytesIn", (uint64_t)0)));
                tunThreadSecs.push_back("gwlbtun_tunnel_thread_seconds_since_last_packet"s + tlabels + " "s + std::to_string(t.value("secsSincelastPacket", 0.0)));
            }
        }

        for(auto &cache : {"v4FlowCache"s, "v6FlowCache"s})
        {
            if(!eni.contains(cache))
                continue;

            auto &fc = eni[cache];
            std::string cacheLabel = (cache == "v4FlowCache"s) ? "v4"s : "v6"s;
            std::string labels = "{"s + label("eni", eniStr) + ","s + label("cache", cacheLabel) + "}"s;

            cacheEntries.push_back("gwlbtun_flowcache_entries"s + labels + " "s + std::to_string(fc.value("size", (uint64_t)0)));
            cacheTimedOut.push_back("gwlbtun_flowcache_evicted_last_check"s + labels + " "s + std::to_string(fc.value("timedOut", (uint64_t)0)));
            cacheIdleTimeout.push_back("gwlbtun_flowcache_idle_timeout_seconds"s + labels + " "s + std::to_string(fc.value("idleTimeoutSecs", 0)));
        }
    }

    emitFamily(out, "gwlbtun_tunnel_packets_total", "Packets sent out to the OS via the tunnel interface.", "counter", tunPkts);
    emitFamily(out, "gwlbtun_tunnel_bytes_total", "Bytes sent out to the OS via the tunnel interface.", "counter", tunBytes);
    emitFamily(out, "gwlbtun_tunnel_seconds_since_last_packet", "Seconds since the tunnel interface last saw a packet.", "gauge", tunSecs);
    emitFamily(out, "gwlbtun_tunnel_thread_up", "Whether the tunnel handler thread is healthy (1) or not (0).", "gauge", tunThreadUp);
    emitFamily(out, "gwlbtun_tunnel_thread_packets_total", "Packets read from the OS by the tunnel handler thread.", "counter", tunThreadPkts);
    emitFamily(out, "gwlbtun_tunnel_thread_bytes_total", "Bytes read from the OS by the tunnel handler thread.", "counter", tunThreadBytes);
    emitFamily(out, "gwlbtun_tunnel_thread_seconds_since_last_packet", "Seconds since the tunnel handler thread last saw a packet.", "gauge", tunThreadSecs);
    emitFamily(out, "gwlbtun_flowcache_entries", "Number of flows currently tracked in the flow cache.", "gauge", cacheEntries);
    emitFamily(out, "gwlbtun_flowcache_evicted_last_check", "Flows evicted from the flow cache for being idle during the most recent health check pass. Not cumulative: resets every time the health check runs, so it must not be scraped as a counter.", "gauge", cacheTimedOut);
    emitFamily(out, "gwlbtun_flowcache_idle_timeout_seconds", "Idle timeout configured for the flow cache, in seconds.", "gauge", cacheIdleTimeout);

    return out.str();
}
