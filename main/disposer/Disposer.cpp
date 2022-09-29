/**
 * This file defines a class for disposing of (i.e., locally processing) data-products.
 *
 *  @file:  Disposer
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
 *
 *    Copyright 2022 University Corporation for Atmospheric Research
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

#include "config.h"

#include "ActionTemplate.h"
#include "Disposer.h"
#include "FileUtil.h"
#include "HashSetQueue.h"
#include "Parser.h"
#include "PatternAction.h"

#include <list>
#include <queue>
#include <yaml-cpp/yaml.h>

namespace hycast {

class Disposer::Impl
{
    std::list<PatternAction>   patActs;       ///< Pattern-actions to be matched
    std::unordered_set<Action> actionSet;     ///< Persistent (e.g., open) actions
    HashSetQueue<Action>       actionQueue;   ///< Persistent actions sorted by last use
    const int                  maxPersistent; ///< Maximum number of persistent actions

    /**
     * Removes the least-recently-used, persistent action. This should cause destruction of the
     * action and the consequent release of any associated resources (e.g., an open file
     * descriptor).
     */
    inline void eraseLru() {
        actionSet.erase(actionQueue.front());
        actionQueue.pop();
    }

    /**
     * Returns the concrete action corresponding to an action template with regular expression
     * substitutions. If the action should persist, then, on return, it will be in the set of
     * persistent actions.
     *
     * @param[in] actionTemplate  Action template to be reified
     * @param[in] match           Regular expression matches
     * @return                    Corresponding concrete action
     */
    Action getAction(
            ActionTemplate& actionTemplate,
            std::smatch&    match) {
        auto action = actionTemplate.reify(match);

        if (action.shouldPersist()) {
            auto pair = actionSet.insert(action);

            if (!pair.second) {
                action = *pair.first;      // Use previously-inserted action
                actionQueue.erase(action); // Remove from action-queue
            } // Action already exists in action-set
            actionQueue.push(action); // Insert at back of action-queue

            while (actionSet.size() > maxPersistent)
                eraseLru();
        } // Action should persist

        return action;
    }

    /**
     * Processes a product's data.
     *
     * @param[in] action   Action to perform
     * @param[in] bytes    Product's data
     * @param[in] nbytes   Number of bytes
     * @retval    `true`   Success
     * @retval    `false`  Failure due to too many open file descriptors
     */
    bool process(
            Action         action,
            const char*    bytes,
            const ProdSize nbytes) {
        bool success;
        LOG_DEBUG("Executing action " + action.to_string());
        for (success = action.process(bytes, nbytes);
                !success && actionQueue.size() > 1;
                success = action.process(bytes, nbytes)) {
            // Product couldn't be processed because of too many open file descriptors
            eraseLru();
        }
        return success;
    }

public:
    Impl(const int maxPersistent)
        : patActs()
        , actionSet(maxPersistent+1)
        , actionQueue(maxPersistent+1)
        , maxPersistent(maxPersistent)
    {}

    ~Impl() {
    }

    void add(const PatternAction& patAct) {
        patActs.push_back(patAct);
    }

    void disposeOf(
            const ProdInfo prodInfo,
            const char*    bytes) {
        // TODO: Erase persistent actions that haven't been used for some time
        for (auto& patAct : patActs) {
            const auto& prodName = prodInfo.getName();
            std::smatch match;

            if (std::regex_search(prodName, match, patAct.include) &&
                    !std::regex_search(prodName, patAct.exclude)) {
                auto action = getAction(patAct.actionTemplate, match);
                if (process(action, bytes, prodInfo.getSize()))
                    LOG_INFO("Processed product " + prodInfo.to_string());
                else
                    LOG_WARNING("Couldn't process product " + prodInfo.to_string() + " because of "
                            "too many open file descriptors");
            } // Product should be processed
        } // Pattern-action loop
    }
};

Disposer::Disposer(const int maxPersistent)
    : pImpl(new Impl{maxPersistent})
{}

void Disposer::add(const PatternAction& patAct) {
    pImpl->add(patAct);
}

void Disposer::disposeOf(
        const ProdInfo prodInfo,
        const char*    bytes) {
    pImpl->disposeOf(prodInfo, bytes);
}

Disposer Disposer::create(const String& configFile) {
    YAML::Node node0;
    try {
        node0 = YAML::LoadFile(configFile);
    }
    catch (const std::exception& ex) {
        std::throw_with_nested(RUNTIME_ERROR("Couldn't load YAML file \"" + configFile + "\""));
    }

    try {
        int maxPersistent = 20;
        if (node0["MaxKeepOpen"]) {
            try {
                maxPersistent = node0["MaxKeepOpen"].as<int>();
            }
            catch (const std::exception& ex) {
                std::throw_with_nested(RUNTIME_ERROR("Couldn't decode \"MaxKeepOpen\" value: \"" +
                        node0["MaxKeepOpen"].as<String>() + "\""));
            }
        }

        Disposer disposer(maxPersistent);

        if (!node0["PatternActions"] || node0["PatternActions"].size() == 0) {
            LOG_WARNING("No pattern-actions in configuration-file \"" + configFile + "\"");
        }
        else {
            auto patActsNode = node0["PatternActions"];
            for (size_t iPatAct = 0; iPatAct < patActsNode.size(); ++iPatAct) {
                auto patActNode = patActsNode[iPatAct];

                std::regex include(".*"); // Default matches anything
                if (patActNode["Include"]) {
                    try {
                        include = std::regex(patActNode["Include"].as<String>());
                    }
                    catch (const std::exception& ex) {
                        std::throw_with_nested(RUNTIME_ERROR("Couldn't decode \"Include\" in "
                                "pattern-action " + std::to_string(iPatAct)));
                    }
                }

                std::regex exclude; // Default matches nothing
                if (patActNode["Exclude"]) {
                    try {
                        exclude = std::regex(patActNode["Exclude"].as<String>());
                    }
                    catch (const std::exception& ex) {
                        std::throw_with_nested(RUNTIME_ERROR("Couldn't decode \"Exclude\" in "
                                "pattern-action " + std::to_string(iPatAct)));
                    }
                }

                bool persist = false;
                try {
                    Parser::tryDecode<bool>(patActNode, "KeepOpen", persist);
                }
                catch (const std::exception& ex) {
                    std::throw_with_nested(RUNTIME_ERROR("Couldn't decode \"KeepOpen\" value in "
                            "pattern-action " + std::to_string(iPatAct)));
                }

                if (!patActNode["Action"])
                    throw RUNTIME_ERROR("No action in pattern-action " + std::to_string(iPatAct));

                auto action = patActNode["Action"].as<String>();

                PatternAction patAct;
                if (::strcasecmp(action.data(), "PIPE") == 0) {
                    if (!patActNode["Command"])
                        throw RUNTIME_ERROR("No \"Command\" node specified in pattern-action " +
                                std::to_string(iPatAct));

                    auto                argsNode = patActNode["Command"];
                    std::vector<String> args(argsNode.size());
                    for (size_t iarg = 0; iarg < argsNode.size(); ++iarg)
                        args[iarg] = argsNode[iarg].as<String>();
                    try {
                        PipeTemplate actTempl(args, persist);
                        patAct = PatternAction(include, exclude, actTempl);
                    }
                    catch (const std::exception& ex) {
                        std::throw_with_nested(RUNTIME_ERROR("Couldn't create decoder template "
                                "from \"Command\" node in pattern-action " +
                                std::to_string(iPatAct)));
                    }
                } // PIPE action
                else if ((::strcasecmp(action.data(), "FILE") == 0 ||
                        (::strcasecmp(action.data(), "APPEND") == 0))) {
                    if (!patActNode["Path"])
                        throw RUNTIME_ERROR("No \"Path\" node specified in pattern-action " +
                                std::to_string(iPatAct));

                    auto                pathNode = patActNode["Path"];
                    std::vector<String> pathname(1);
                    pathname[0] = pathNode.as<String>();

                    try {
                        ActionTemplate actTempl;
                        if (::strcasecmp(action.data(), "FILE") == 0) {
                            actTempl = FileTemplate(pathname, persist);
                        }
                        else {
                            actTempl = AppendTemplate(pathname, persist);
                        }
                        patAct = PatternAction(include, exclude, actTempl);
                    }
                    catch (const std::exception& ex) {
                        std::throw_with_nested(RUNTIME_ERROR("Couldn't create pathname template "
                                "from \"Path\" node in pattern-action " + std::to_string(iPatAct)));
                    }
                }

                disposer.add(patAct);
            } // Pattern-action loop
        } // Pattern-actions exist

        return disposer;
    } // YAML file loaded
    catch (const std::exception& ex) {
        LOG_ERROR(ex.what());
        std::throw_with_nested(RUNTIME_ERROR("Couldn't parse YAML file \"" + configFile + "\""));
    }
}

} // namespace
