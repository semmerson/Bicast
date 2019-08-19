/**
 * This file implements an ingester of data-products. One that returns to a
 * source-node of hycast products a sequence of products to be transmitted.
 *
 * This particular implementation uses inotify(7) to monitor a directory
 * hierarchy and return all existing, newly-closed, and moved-in files as
 * data-products.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: FileIngester.cpp
 *  Created on: Oct 24, 2017
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "error.h"
#include "FileIngester.h"

#include <chrono>
#include <dirent.h>
#include <fcntl.h>
#include <stack>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

namespace hycast {

class FileIngester::Impl : public Ingester::Impl
{
    friend class FileIngester;

    class DirEntry
    {
        DIR*        dir;
        std::string pathname;
    public:
        DirEntry()
            : dir{nullptr}
            , pathname{}
        {}
        DirEntry(const std::string& pathname)
            : dir{::opendir(pathname.data())}
            , pathname{pathname}
        {
            if (dir == nullptr)
                throw SYSTEM_ERROR(std::string{"opendir() failure on \""} +
                        pathname + "\"", errno);
        }
        ~DirEntry() noexcept
        {
            ::closedir(dir);
        }
        const std::string makePathname(const std::string& filename)
        {
            return pathname + "/" + filename;
        }
        bool next(std::string& filename)
        {
            struct dirent  entry, *result;
            auto           status = ::readdir_r(dir, &entry, &result);
            if (status)
                throw SYSTEM_ERROR("readdir() failure on directory \"" +
                        pathname + "\"", status);
            if (result == nullptr) {
                // Nothing more in current directory
                return false;
            }
            filename = makePathname(entry.d_name);
            return true;
        }
    };

    class DirStack
    {
        std::stack<DirEntry> dirs;
    public:
        inline void push(const std::string& pathname)
        {
            dirs.push(DirEntry{pathname});
        }
        inline bool empty()
        {
            return dirs.empty();
        }
        inline void pop()
        {
            dirs.pop();
        }
        inline bool next(std::string& filename)
        {
            return dirs.top().next(filename);
        }
    };

    typedef std::chrono::system_clock      Clock;
    typedef std::chrono::time_point<Clock> TimePoint;

    const std::string dirPathname;
    const int         fd;
    TimePoint         start;
    bool              scanDir;
    DirStack          dirStack;

    bool isDirectory(const std::string& pathname)
    {
        struct stat statBuf;
        auto        status = ::stat(pathname.data(), &statBuf);
        if (status)
            LOG_WARN("stat() failure on " + pathname);
        return S_ISDIR(statBuf.st_mode);
    }

    TimePoint getTime(const std::string& pathname)
    {
        struct stat statBuf;
        auto        status = ::stat(pathname.data(), &statBuf);
        if (status)
            throw SYSTEM_ERROR(std::string{"stat() failure on \""} + pathname +
                    "\"", errno);
        return Clock::from_time_t(statBuf.st_mtim.tv_sec) +
            std::chrono::nanoseconds{statBuf.st_mtim.tv_nsec};
    }

    bool getScanDirProd(Product& prod)
    {
        while (!dirStack.empty()) {
            std::string pathname;
            if (!dirStack.next(pathname)) {
                dirStack.pop();
                continue;
            }
            if (isDirectory(pathname)) {
                dirStack.push(pathname);
                continue;
            }
            if (getTime(pathname) < start) {
#if 0
                prod = Product{pathname}; // TODO
#endif
                return true;
            }
        }
        return false;
    }

    Product getInotifyProd()
    {
        union {
            char                 buf[sizeof(struct inotify_event) + NAME_MAX + 1];
            struct inotify_event event;
        };
        for (;;) {
            auto nbytes = ::read(fd, buf, sizeof(buf));
            if (nbytes == -1)
                throw SYSTEM_ERROR("read() failure", errno);
            switch (event.mask) {
            // TODO
            }
        }
        return Product{};
    }

    explicit Impl(const std::string& rootDirPathname)
        : dirPathname{rootDirPathname}
        , fd{::inotify_init1(IN_CLOEXEC)}
        , start{}
        , scanDir{true}
        , dirStack{}
    {
        if (fd == -1)
            throw SYSTEM_ERROR("inotify_init1() failure", errno);
        try {
            dirStack.push(rootDirPathname);

            auto wd = ::inotify_add_watch(fd, rootDirPathname.data(),
                    IN_CLOSE_WRITE | IN_CREATE | IN_DELETE_SELF | IN_MOVED_TO);
            if (wd == -1)
                throw SYSTEM_ERROR("inotify_add_watch() failure", errno);

            start = Clock::now();
        }
        catch (const std::exception& ex) {
            ::close(fd);
            throw;
        }
    }

    ~Impl() noexcept
    {
        if (fd != -1)
            ::close(fd);
    }

    Product getProduct()
    {
        if (scanDir) {
            Product prod{};
            if (getScanDirProd(prod))
                return prod;
            scanDir = false;
        }
        return getInotifyProd();
    }
}; // FileIngester::Impl

FileIngester::FileIngester()
    : Ingester{}
{}

FileIngester::FileIngester(const std::string& dirPathname)
    : Ingester{new Impl{dirPathname}}
{}

} // namespace
