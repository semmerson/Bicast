/**
 * This file implements a source of potential remote peers based on a YAML
 * document.
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

namespace hycast {

YamlPeerSource::YamlPeerSource(YAML::Node peerAddrs)
    : PeerSource{}
{
    if (!peerAddrs.IsSequence())
        throw INVALID_ARGUMENT("YAML node is not a sequence");
    for (size_t i = 0; i < peerAddrs.size(); ++i) {
        if (!peerAddrs[i].IsMap())
            throw INVALID_ARGUMENT("Element " + std::to_string(i) +
                    " is not a map");
        auto inetAddr = peerAddrs[i]["inetAddr"].as<std::string>();
        auto port = peerAddrs[i]["port"].as<in_port_t>();
        push(InetSockAddr(inetAddr, port));
    }
}

YamlPeerSource::YamlPeerSource(const std::string& string)
    : YamlPeerSource{YAML::Load(string)}
{}

YamlPeerSource::YamlPeerSource(std::istream& istream)
    : YamlPeerSource{YAML::Load(istream)}
{}

} // namespace
