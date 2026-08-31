// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef GWLBTUN_NETNAMESPACE_H
#define GWLBTUN_NETNAMESPACE_H

#include <string>

/**
 * RAII helper that moves the calling thread into a persistent, named network namespace for its
 * scope, creating the namespace natively (unshare + bind mount onto /var/run/netns/<name>) if it
 * doesn't already exist, or joining it if it does (e.g. a leftover from a prior crash/restart
 * reusing the same name).
 *
 * Network namespaces in Linux are per-thread (part of struct nsproxy on the task), not
 * per-process: unshare()/setns() on one thread never affects sibling threads, and a socket's
 * namespace is fixed at socket(2) time, not re-evaluated later. NetnsScope relies on both facts -
 * it only ever moves the thread that constructs it, and any resource that must stay in the root
 * namespace (e.g. a socket used to reach the real host NIC) must be created only after the
 * NetnsScope guarding it has gone out of scope.
 *
 * Any thread cloned from within a NetnsScope's lifetime (e.g. TunInterface's per-queue reader
 * threads) inherits that namespace permanently, since it is never setns'd back - only the thread
 * that constructed/destructed this NetnsScope is restored to its original namespace.
 */
class NetnsScope {
public:
    explicit NetnsScope(const std::string& name);   // May throw std::system_error.
    ~NetnsScope();                                   // Restores the original namespace; never throws.
    NetnsScope(const NetnsScope&) = delete;
    NetnsScope& operator=(const NetnsScope&) = delete;

private:
    int origNetNsFd;
};

// Tears down a namespace created by NetnsScope, once nothing inside it (e.g. TUN devices) remains.
// Idempotent - safe to call even if the namespace file is already gone.
void destroyNetns(const std::string& name);

#endif //GWLBTUN_NETNAMESPACE_H
