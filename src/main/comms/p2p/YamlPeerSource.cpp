/**
 * This file implements a source of peers based on a YAML document.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: YamlPeerSsource.cpp
 * @author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "YamlPeerSource.h"

#include <set>

namespace hycast {

class YamlPeerSource::Impl
{
    std::set<InetSockAddr> peerAddrs;

public:
    /**
     * Constructs.
     * @param[in] list          YAML node containing peer specifications
     * @throws InvalidArgument  Node isn't a sequence
     * @throws InvalidArgument  Sequence element isn't a map
     * @exceptionsafety         Strong guarantee
     */
    Impl(YAML::Node list)
    {
        if (!list.IsSequence())
            throw InvalidArgument(__FILE__, __LINE__,
                    "YAML node is not a sequence");
        try {
            for (size_t i = 0; i < list.size(); ++i) {
                if (!list[i].IsMap())
                    throw InvalidArgument(__FILE__, __LINE__,
                            "Element " + std::to_string(i) + " is not a map");
                auto inetAddr = list[i]["inetAddr"].as<std::string>();
                auto port = list[i]["port"].as<in_port_t>();
                peerAddrs.emplace(InetSockAddr(inetAddr, port));
            }
        }
        catch (const std::exception& e) {
            peerAddrs.clear();
            throw;
        }
    }

    /**
     * Constructs.
     * @param[in] string        Encoded YAML string containing peer
     *                          specifications
     * @throws InvalidArgument  Node isn't a sequence
     * @throws InvalidArgument  Sequence element isn't a map
     * @exceptionsafety         Strong guarantee
     */
    Impl(const std::string& string)
        : Impl(YAML::Load(string))
    {}

    /**
     * Constructs.
     * @param[in] istream       Input stream containing YAML-encoded peer
     *                          specifications
     * @throws InvalidArgument  Node isn't a sequence
     * @throws InvalidArgument  Sequence element isn't a map
     * @exceptionsafety         Strong guarantee
     */
    Impl(std::istream& istream)
        : Impl(YAML::Load(istream))
    {}

    /**
     * Returns an iterator over the potential peers. Blocks if no peers are
     * available.
     * @return Iterator over potential peers:
     */
    Iterator getPeers()
    {
        return peerAddrs.begin();
    }

    /**
     * Returns the "end" iterator.
     * @return End iterator
     */
    Iterator end()
    {
        return peerAddrs.end();
    }
};

YamlPeerSource::YamlPeerSource(YAML::Node node)
    : pImpl{new Impl(node)}
{}

YamlPeerSource::YamlPeerSource(const std::string& string)
    : pImpl{new Impl(string)}
{}

YamlPeerSource::YamlPeerSource(std::istream& istream)
    : pImpl{new Impl(istream)}
{}

PeerSource::Iterator YamlPeerSource::getPeers()
{
    return pImpl->getPeers();
}

PeerSource::Iterator YamlPeerSource::end()
{
    return pImpl->end();
}

} // namespace
