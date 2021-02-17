/**
 * Declarations related to the protocol.
 *
 *        File: protocol.h
 *  Created on: Feb 22, 2020
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MAIN_PROTOCOL_PROTOCOL_H_
#define MAIN_PROTOCOL_PROTOCOL_H_

#include <memory>

namespace hycast {

typedef enum {
    PROD_INFO_NOTICE = 1,
    DATA_SEG_NOTICE,
    PROD_INFO_REQUEST,
    DATA_SEG_REQUEST,
    PROD_INFO,
    DATA_SEG,
    PATH_TO_PUB,
    NO_PATH_TO_PUB
} MsgId;

} // namespace

#endif /* MAIN_PROTOCOL_PROTOCOL_H_ */
