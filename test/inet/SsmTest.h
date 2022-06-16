/**
 * Copyright 2021 University Corporation for Atmospheric Research. All rights
 * reserved. See the the file COPYRIGHT in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: SsmTest.h
 * @author: Steven R. Emmerson
 *
 * This file contains common definitions for `SsmSendTest.c` and `SsmRecvTest.c`.
 */

#ifndef SSM_TEST_H_
#define SSM_TEST_H_

/*
 * Source-specific multicast (SSM) address. According to RFC4607:
 *   - Source-specific multicast address in the range 232.0.0/24 are reserved
 *     and must not be used
 *   - "The policy for allocating the rest of the SSM addresses to sending
 *     applications is strictly locally determined by the sending host."
 */
#define SSM_FAMILY     AF_INET
#define SSM_IP_ADDR    "232.1.1.1"
#define SSM_PORT       38800
#define SSM_SOCK_ADDR  SSM_IP_ADDR ":38800"

#ifdef __cplusplus
    extern "C" {
#endif

#ifdef __cplusplus
    }
#endif

#endif /* SSM_TEST_H_ */
