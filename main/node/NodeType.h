/**
 * Type of node. One that is
 *     - The source of data-products; or
 *     - Has a path to the source of data-products; or
 *     - Doesn't have a path to the source of data-products.
 *
 *        File: NodeType.h
 *  Created on: Mar 9, 2020
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

#ifndef MAIN_NODE_NODETYPE_H_
#define MAIN_NODE_NODETYPE_H_

namespace hycast {

enum NodeType
{
    PUBLISHER,
    PATH_TO_PUBLISHER,
    NO_PATH_TO_PUBLISHER
};

} // namespace

#endif /* MAIN_NODE_NODETYPE_H_ */
