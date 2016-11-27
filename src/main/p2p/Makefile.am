#######################################
# The list of libraries we are building seperated by spaces.
# The 'lib_' indicates that these build products will be installed
# in the $(libdir) directory. For example /usr/lib
noinst_LTLIBRARIES = lib.la

#######################################
# Build information for each library

# Sources for libhycast
lib_la_SOURCES 	= \
	VersionMsg.cpp VersionMsg.h \
	Channel.h \
	ChannelImpl.cpp ChannelImpl.h \
	RegChannel.cpp RegChannel.h \
	RegChannelImpl.cpp RegChannelImpl.h \
	ChunkChannelImpl.cpp ChunkChannelImpl.h \
	ChunkChannel.cpp ChunkChannel.h \
	PeerImpl.cpp PeerImpl.h \
	Peer.cpp Peer.h \
	PeerMgr.h \
	MapOfLists.cpp MapOfLists.h \
	PeerSetImpl.cpp PeerSetImpl.h \
	PeerSet.cpp PeerSet.h

# Compiler options. Here we are adding the include directory
# to be searched for headers included in the source code.
lib_la_CPPFLAGS = -I$(srcdir)/.. -I$(srcdir)/../net -I$(srcdir)/../prod