/**
 * This file declares a source of peers based on a YAML document.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: JsonPeerSource.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_COMMS_P2P_YAMLPEERSOURCE_H_
#define MAIN_COMMS_P2P_YAMLPEERSOURCE_H_

#include "PeerSource.h"

#include <memory>
#include <yaml-cpp/yaml.h>

namespace hycast {

class YamlPeerSource : public PeerSource
{
    class Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs.
     * @param[in] node          YAML node containing peer specifications
     * @throws InvalidArgument  Node isn't a sequence
     * @throws InvalidArgument  Sequence element isn't a map
     * @exceptionsafety         Strong guarantee
     */
    YamlPeerSource(YAML::Node node);

    /**
     * Constructs.
     * @param[in] string        Encoded YAML string containing peer
     *                          specifications
     * @throws InvalidArgument  Node isn't a sequence
     * @throws InvalidArgument  Sequence element isn't a map
     * @exceptionsafety         Strong guarantee
     */
    YamlPeerSource(const std::string& string);

    /**
     * Constructs.
     * @param[in] istream       Input stream containing YAML-encoded peer
     *                          specifications
     * @throws InvalidArgument  Node isn't a sequence
     * @throws InvalidArgument  Sequence element isn't a map
     * @exceptionsafety         Strong guarantee
     */
    YamlPeerSource(std::istream& istream);

    /**
     * Returns an iterator over the potential peers. Blocks if no peers are
     * available.
     * @return Iterator over potential peers
     */
    Iterator getPeers();

    /**
     * Returns the "end" iterator.
     * @return End iterator
     */
    Iterator end();
};

} // namespace

#endif /* MAIN_COMMS_P2P_YAMLPEERSOURCE_H_ */
