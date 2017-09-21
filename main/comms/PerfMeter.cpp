/**
 * This file implements a performance meter for measuring data-product
 * transmission.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PerfMeter.cpp
 *  Created on: Aug 22, 2017
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "error.h"
#include "PerfMeter.h"

#include <chrono>
#include <mutex>
#include <ostream>
#include <yaml-cpp/yaml.h>

namespace hycast {

class PerfMeter::Impl
{
    typedef std::mutex                     Mutex;
    typedef std::lock_guard<Mutex>         LockGuard;
    typedef std::chrono::steady_clock      Clock;
    typedef std::chrono::time_point<Clock> TimePoint;
    typedef std::chrono::duration<double>  Duration; // Double precision seconds
    typedef enum {
        READY = 1,
        STARTED,
        STOPPED
    }             State;

    mutable Mutex mutex;
    State         state;
    TimePoint     start;
    unsigned long prodCount;
    unsigned long byteCount;
    Duration      duration;

public:
    Impl()
        : mutex{}
        , state{READY}
        , start{}
        , prodCount{0}
        , byteCount{0}
        , duration{}
    {}

    void product(const ProdInfo& prodInfo)
    {
        LockGuard lock{mutex};
        if (state == READY) {
            state = STARTED;
            start = Clock::now();
        }
        else if (state != STARTED) {
            throw LOGIC_ERROR("Performance meter not ready");
        }
        ++prodCount;
        byteCount += prodInfo.getSize();
    }

    unsigned long getProdCount() const
    {
        LockGuard lock{mutex};
        return prodCount;
    }

    /**
     * Idempotent.
     */
    void stop()
    {
        LockGuard lock{mutex};
        if (state != STOPPED) {
            duration = std::chrono::duration_cast<Duration>
                    (Clock::now() - start);
            state = STOPPED;
        }
    }

    std::ostream& print(std::ostream& ostream) {
        LockGuard lock{mutex};
        if (state != STOPPED)
            throw LOGIC_ERROR("Performance meter not stopped");
        const double seconds = duration.count();
        YAML::Emitter out;
        out << YAML::BeginMap;
            out << YAML::Key << "Duration";
            out << (std::to_string(seconds) + " s");
            out << YAML::Key << "Products";
            out << YAML::BeginMap;
                out << YAML::Key << "count";
                out << prodCount;
                if (seconds != 0) {
                    out << YAML::Key << "rate";
                    out << (std::to_string(prodCount/seconds) + "/s");
                }
            out << YAML::EndMap;
            out << YAML::Key << "Bytes";
            out << YAML::BeginMap;
                out << YAML::Key << "count";
                out << byteCount;
                if (seconds != 0) {
                    out << YAML::Key << "rate";
                    out << (std::to_string(byteCount/seconds) + "/s");
                }
            out << YAML::EndMap;
            out << YAML::Key << "Bits";
            out << YAML::BeginMap;
                out << YAML::Key << "count";
                out << 8*byteCount;
                if (seconds != 0) {
                    out << YAML::Key << "rate";
                    out << (std::to_string(8*byteCount/seconds) + "/s");
                }
            out << YAML::EndMap;
        out << YAML::EndMap;
        return ostream << out.c_str();
    }
};

PerfMeter::PerfMeter()
    : pImpl{new Impl()}
{}

void PerfMeter::product(const ProdInfo& prodInfo) const
{
    pImpl->product(prodInfo);
}

unsigned long PerfMeter::getProdCount() const
{
    return pImpl->getProdCount();
}

void PerfMeter::stop() const
{
    pImpl->stop();
}

std::ostream& operator<<(
        std::ostream&    ostream,
        const PerfMeter& perfMeter)
{
    return perfMeter.pImpl->print(ostream);
}

} // namespace
