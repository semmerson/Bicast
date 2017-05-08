/**
 * This file defines a server-side socket.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ServerSocket.cpp
 * @author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "SrvrSctpSock.h"

namespace hycast {

SrvrSctpSock::SrvrSctpSock(
        const InetSockAddr& addr,
        const uint16_t      numStreams,
        const int           queueSize)
	: SctpSock(socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP), numStreams)
{
	try {
		if (queueSize <= 0)
			throw InvalidArgument(__FILE__, __LINE__,
					"Invalid length for ::accept() queue: " +
					std::to_string(queueSize));
		const int sd = getSock();
		if (sd == -1)
			throw SystemError(__FILE__, __LINE__, "socket() failure");
		addr.bind(sd);
		if (listen(sd, queueSize))
			throw SystemError(__FILE__, __LINE__, "listen() failure: sock=" +
					std::to_string(sd) + ", addr=" + to_string());
	}
	catch (const std::exception& e) {
		std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
				"Couldn't construct server-side SCTP socket"));
	}
}

SctpSock SrvrSctpSock::accept() const
{
	socklen_t len = 0;
	const int sock = getSock();
	const int sd = ::accept(sock, (struct sockaddr*)nullptr, &len);
	if (sd < 0)
		throw SystemError(__FILE__, __LINE__,  "accept() failure: sock=" +
				std::to_string(sock));
	return SctpSock(sd, getNumStreams());
}

} // namespace
