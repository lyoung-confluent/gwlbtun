// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include "NetNamespace.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include "Logger.h"

using namespace std::string_literals;

static const std::string NETNS_RUN_DIR = "/var/run/netns"s;

NetnsScope::NetnsScope(const std::string& name)
{
    // /proc/self/ns/net would resolve to the *thread-group leader's* namespace, not necessarily this
    // calling thread's own - for a multithreaded process those can already differ (e.g. this thread
    // previously unshared and later restored, but another thread never has). /proc/thread-self (added
    // in Linux 3.17) always resolves to the calling thread specifically, which is what every use here
    // needs.
    origNetNsFd = open("/proc/thread-self/ns/net", O_RDONLY | O_CLOEXEC);
    if(origNetNsFd < 0)
        throw std::system_error(errno, std::generic_category(), "Unable to open /proc/thread-self/ns/net");

    try {
        if(mkdir(NETNS_RUN_DIR.c_str(), 0755) < 0 && errno != EEXIST)
            throw std::system_error(errno, std::generic_category(), "Unable to create "s + NETNS_RUN_DIR);

        std::string path = NETNS_RUN_DIR + "/"s + name;

        int fd = open(path.c_str(), O_RDONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
        if(fd >= 0)
        {
            // We created the placeholder file, so we're the one responsible for building the
            // namespace. Unshare into a fresh netns, then bind-mount it onto the placeholder so
            // it's nameable/joinable (by us later, and by `ip netns exec`) for as long as the
            // mount lives - the same technique `ip netns add` uses, without depending on that
            // binary being present.
            close(fd);
            if(unshare(CLONE_NEWNET) < 0)
            {
                int err = errno;
                unlink(path.c_str());
                throw std::system_error(err, std::generic_category(), "Unable to unshare a new network namespace");
            }
            if(mount("/proc/thread-self/ns/net", path.c_str(), nullptr, MS_BIND, nullptr) < 0)
            {
                int err = errno;
                unlink(path.c_str());
                throw std::system_error(err, std::generic_category(), "Unable to bind-mount new network namespace onto "s + path);
            }
        } else if(errno == EEXIST) {
            // Someone else already created (or is creating) this namespace - most likely a
            // leftover from a prior crash/restart reusing the same ENI id. Join it instead of
            // recreating it. A handful of retries covers the narrow window where the placeholder
            // file exists but hasn't been bind-mounted onto yet (setns() fails with EINVAL/ENOENT
            // in that window since the path isn't actually a namespace file yet).
            bool joined = false;
            std::system_error lastError(0, std::generic_category());
            for(int attempt = 0; attempt < 20 && !joined; attempt ++)
            {
                if(attempt > 0)
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));

                int joinFd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
                if(joinFd < 0)
                {
                    lastError = std::system_error(errno, std::generic_category(), "Unable to open existing network namespace "s + path);
                    continue;
                }
                if(setns(joinFd, CLONE_NEWNET) < 0)
                {
                    lastError = std::system_error(errno, std::generic_category(), "Unable to join existing network namespace "s + path);
                    close(joinFd);
                    continue;
                }
                close(joinFd);
                joined = true;
            }
            if(!joined)
                throw lastError;
        } else {
            throw std::system_error(errno, std::generic_category(), "Unable to create "s + path);
        }
    }
    catch(...) {
        // Whatever failed above, don't strand this thread outside its original namespace.
        if(setns(origNetNsFd, CLONE_NEWNET) < 0)
            LOG(LS_TUNNEL, LL_CRITICAL, "Unable to restore original network namespace after a namespace setup failure: "s + std::error_code{errno, std::generic_category()}.message());
        close(origNetNsFd);
        throw;
    }
}

NetnsScope::~NetnsScope()
{
    if(setns(origNetNsFd, CLONE_NEWNET) < 0)
        LOG(LS_TUNNEL, LL_CRITICAL, "Unable to restore original network namespace: "s + std::error_code{errno, std::generic_category()}.message());
    close(origNetNsFd);
}

void destroyNetns(const std::string& name)
{
    std::string path = NETNS_RUN_DIR + "/"s + name;

    if(umount2(path.c_str(), MNT_DETACH) < 0 && errno != EINVAL && errno != ENOENT)
        LOG(LS_TUNNEL, LL_IMPORTANT, "Unable to unmount network namespace "s + path + ": "s + std::error_code{errno, std::generic_category()}.message());

    if(unlink(path.c_str()) < 0 && errno != ENOENT)
        LOG(LS_TUNNEL, LL_IMPORTANT, "Unable to remove network namespace file "s + path + ": "s + std::error_code{errno, std::generic_category()}.message());
}
