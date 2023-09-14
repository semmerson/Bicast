/**
 * This file defines a class for disposing of (i.e., locally processing) data-products.
 *
 *  @file:  Disposer.cpp
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
#include "error.h"
#include "FileUtil.h"
#include "HashSetQueue.h"
#include "Parser.h"
#include "PatternAction.h"
#include "Shield.h"
#include "ThreadException.h"

#include <fcntl.h>
#include <limits.h>
#include <LastProd.h>
#include <list>
#include <queue>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

namespace hycast {

/// An implementation of a class that locally disposes of data products
class Disposer::Impl
{
    using PatActs  = std::list<PatternAction>;
    using ProcCmds = std::unordered_map<pid_t, String>;

    mutable Mutex              mutex;         ///< State mutex
    mutable Cond               cond;          ///< Condition variable
    String                     lastProcDir;   ///< Pathname of directory to hold information on the
                                              ///< last, successfully-processed data-product
    LastProdPtr                lastProd;      ///< Saves information on last, successfully-processed
                                              ///< data-product
    PatActs                    patActs;       ///< Pattern-actions to be matched
    std::unordered_set<Action> actionSet;     ///< Persistent (e.g., open) actions
    HashSetQueue<Action>       actionQueue;   ///< Persistent actions sorted by last use
    int                        maxPersistent; ///< Maximum number of persistent actions
    ProcCmds                   childCmds;     ///< Child processes to be waited on

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
     * Returns the concrete action corresponding to a action template with regular expression
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
        LOG_ASSERT(!mutex.try_lock());

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
     * @pre                 Mutex is locked
     * @param[in] action    Action to perform
     * @param[in] bytes     Product's data
     * @param[in] nbytes    Number of bytes
     * @throw RuntimeError  Too many open file descriptors or user processes
     * @post                Mutex is locked
     */
    void process(
            Action         action,
            const char*    bytes,
            const ProdSize nbytes) {
        LOG_ASSERT(!mutex.try_lock());

        bool   success;
        pid_t  pid;
        String childStr;

        while (!(success = action.process(bytes, nbytes, pid, childStr)) && actionQueue.size() > 1) {
            /*
             * Product couldn't be processed because of too many open file descriptors or too many
             * user processes.
             * TODO: Make this more discerning: depending on why `action.process()` failed, close
             * only the associated file-descriptor.
             */
            eraseLru();
        }
        if (!success)
            throw RUNTIME_ERROR("Too many open file descriptors or user processes");

        if (pid > 0) {
            childCmds[pid] = childStr;
            cond.notify_all();
        }
    }

    /*
     * TODO: Extract the child-waiting code (logChild(), killChildren(), runWaiter()) into a separate
     * class so that waiting on child processes can be done at the program-level rather than in this
     * class.
     */

    /**
     * Logs the termination of a child process.
     * @param[in] childCmd   Child process' command
     * @param[in] stat       Status of the child process
     */
    static void logChild(
            const String& childCmd,
            const int     stat) {
        if (WIFSIGNALED(stat)) {
            if (WTERMSIG(stat) == SIGTERM) {
                LOG_INFO("Child process \"" + childCmd + "\" terminated by SIGTERM");
            }
            else {
                LOG_WARN("Child process \"" + childCmd + "\" terminated by signal " +
                        std::to_string(WTERMSIG(stat)));
            }
        }
        else if (WIFEXITED(stat)) {
            const int exitStatus = WEXITSTATUS(stat);
            if (exitStatus == 0) {
                LOG_INFO("Child process \"" + childCmd + "\" exited successfully");
            }
            else {
                LOG_WARN("Child process \"" + childCmd + "\" exited with status " +
                        std::to_string(exitStatus));
            }
        }
    }

    /**
     * Reaps (i.e., waits upon) child processes and logs their termination.
     * @pre                The mutex is locked
     * @post               The mutex is locked
     * @throw SystemError  `wait(3)` failure
     * @throw LogicError   Terminated child process is unknown
     */
    void reap() {
        LOG_ASSERT(!mutex.try_lock());

        for (;;) {
            int        stat;
            const auto pid = ::wait(&stat);

            if (pid == -1) {
                if (errno != ECHILD)
                    throw SYSTEM_ERROR("wait() failure");
                childCmds.clear();
                break; // No child processes to wait upon
            }

            auto iter = childCmds.find(pid);
            if (iter == childCmds.end())
                throw LOGIC_ERROR("Child process " + std::to_string(pid) + " is unknown");

            auto childCmd = iter->second;
            childCmds.erase(iter);

            logChild(childCmd, stat);
        }
    }

    /**
     * Terminates all known child processes by sending them a SIGTERM and waits until they all
     * terminate. Logs the termination of each child process.
     * @pre   Mutex is locked
     * @post  Mutex is locked
     */
    void killChildren() {
        LOG_ASSERT(!mutex.try_lock());

        for (auto& pair : childCmds)
            ::kill(pair.first, SIGTERM);

        reap();
    }

public:
    /// Iterator over the patten-action entries
    using Iterator = PatActs::iterator;

    /**
     * Constructs.
     * @param[in] lastProcessed  Saves information on the last successfully-processed data-product
     * @param[in] maxPersistent  Maximum number of persistent actions (i.e., actions whose file
     *                           descriptors are kept open)
     */
    Impl(   const LastProdPtr& lastProcessed,
            const int          maxPersistent)
        : mutex()
        , cond()
        , lastProd(lastProcessed)
        , patActs()
        , actionSet(0)
        , actionQueue(0)
        , maxPersistent(maxPersistent)
        , childCmds()
    {}

    ~Impl() {
        try {
            Guard guard{mutex};
            killChildren();
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex); // Destructors must not throw
        }
    }

    /**
     * Returns the maximum number of file descriptors to keep open between products.
     * @return Maximum number of file descriptors to keep open
     */
    int getMaxKeepOpen() const {
        Guard guard{mutex};
        return maxPersistent;
    }

    /**
     * Adds an entry.
     * @param[in] patAct  The pattern and action to add
     */
    void add(const PatternAction& patAct) {
        Guard guard{mutex};
        patActs.push_back(patAct);
    }

    /**
     * Returns the modification-time of the last, successfully-processed data-product.
     * @return Modification-time of the last, successfully-processed data-product
     */
    SysTimePoint getLastProcTime() const {
        return lastProd->recall();
    }

    /**
     * Returns the number of pattern/action entries.
     * @return The number of pattern/action entries
     */
    size_t size() const {
        Guard guard{mutex};
        return patActs.size();
    }

    /**
     * Returns an iterator over this instance's pattern-actions.
     * @return Iterator over pattern-actions
     */
    Iterator begin() {
        Guard guard{mutex};
        return patActs.begin();
    }

    /**
     * Returns an iterator beyond this instance's pattern-actions.
     * @return Iterator beyond pattern-actions
     */
    Iterator end() {
        Guard guard{mutex};
        return patActs.end();
    }

    /**
     * Disposes of a data product and waits on any terminated child processes.
     * @param[in] prodInfo  Information on the product
     * @param[in] bytes     The product's data
     * @param[in] path      Pathname of the underlying file
     * @retval    true      Disposition was successful
     * @retval    false     Disposition was not successful
     * @throw RuntimeError  Too many open file descriptors or user processes
     * @throw SystemError  `wait(3)` failure
     * @throw LogicError    Terminated child process is unknown
     */
    bool dispose(
            const ProdInfo prodInfo,
            const char*    bytes,
            const String&  path) {
        bool  success = true;
        Guard guard{mutex};

        // TODO: Erase persistent actions that haven't been used for some time
        for (auto& patAct : patActs) {
            const auto& prodName = prodInfo.getName();
            std::smatch match;

            if (std::regex_search(prodName, match, patAct.include.getRegex()) &&
                    !std::regex_search(prodName, patAct.exclude.getRegex())) {
                auto action = getAction(patAct.actionTemplate, match);

                try {
                    process(action, bytes, prodInfo.getSize());
                    LOG_INFO("Executed " + action.to_string() + " on " + prodInfo.to_string() +
                            ". Latency=" + std::to_string((SysClock::now() -
                            prodInfo.getCreateTime()).count() * SysClockRatio) + " s");
                    lastProd->save(FileUtil::getModTime(path));
                }
                catch (const std::exception& ex) {
                    LOG_ERROR(ex, ("Couldn't execute " + action.to_string() + " on " +
                            prodInfo.to_string()).data());
                    success = false;
                }
            } // Product should be processed
        } // Pattern-action loop

        reap();

        return success;
    }

    /**
     * Returns the YAML representation of this instance.
     * @return YAML representation of this instance
     */
    String getYaml() {
        Guard         guard{mutex};
        YAML::Emitter yaml;

        yaml << YAML::BeginMap;

        yaml << YAML::Key << "maxKeepOpen";
        yaml << YAML::Value << std::to_string(maxPersistent);

        yaml << YAML::Key << "patternActions";
        yaml << YAML::BeginSeq;

        for (auto& patAct : patActs) {
            yaml << YAML::BeginMap;
                yaml << YAML::Key << "include";
                yaml << YAML::Value << patAct.include.to_string();

                const String& string = patAct.exclude.to_string();
                if (string.size()) {
                    yaml << YAML::Key << "exclude";
                    yaml << YAML::Value << string;
                }

                yaml << YAML::Key;
                switch (patAct.actionTemplate.getType()) {
                case ActionTemplate::Type::EXEC:   yaml << "exec";   break;
                case ActionTemplate::Type::PIPE:   yaml << "pipe";   break;
                case ActionTemplate::Type::FILE:   yaml << "file";   break;
                case ActionTemplate::Type::APPEND: yaml << "append"; break;
                }
                const std::vector<String>& args = patAct.actionTemplate.getArgs();
                const bool needsSeq = args.size() > 1;
                yaml << YAML::Flow;
                if (needsSeq)
                    yaml << YAML::BeginSeq;
                    for (auto& arg : args)
                        yaml << arg;
                if (needsSeq)
                    yaml << YAML::EndSeq;

                if (patAct.actionTemplate.getKeepOpen())
                    yaml << YAML::Key << "KeepOpen" << YAML::Value << "true";
            yaml << YAML::EndMap;
        }

        yaml << YAML::EndSeq;
        yaml << YAML::EndMap;

        return yaml.c_str();
    }
};

Disposer::Disposer()
    : pImpl()
{}

Disposer::Disposer(
        const LastProdPtr& lastProcessed,
        const int          maxPersistent)
    : pImpl(new Impl{lastProcessed, maxPersistent})
{}

Disposer::operator bool() const noexcept {
    return pImpl.operator bool();
}

void Disposer::add(const PatternAction& patAct) const {
    pImpl->add(patAct);
}

SysTimePoint Disposer::getLastProcTime() const {
    return pImpl ? pImpl->getLastProcTime() : SysTimePoint::min();
}

size_t Disposer::size() const {
    return pImpl ? pImpl->size() : 0;
}

bool Disposer::dispose(
        const ProdInfo prodInfo,
        const char*    bytes,
        const String&  path) const {
    return pImpl->dispose(prodInfo, bytes, path);
}

/**
 * Decodes the action in a map node into a pattern-action template and adds it to a disposer.
 * @tparam VALUE           Value type (e.g., string, command vector)
 * @tparam TEMPLATE        Pattern-action template type
 * @param[in] mapNode      Map node
 * @param[in] actionName   Action name
 * @param[in] include      What products to process
 * @param[in] exclude      What products to ignore
 * @param[in] persist      Keep the output-file open?
 * @param[in] disposer     Disposer
 * @retval true            Value was found
 * @retval false           Value was not found
 * @throw InvalidArgument  Node isn't a map
 * @throw InvalidArgument  A subnode isn't the expected type
 * @throw InvalidArgument  A scalar value isn't the expected type
 */
template<class VALUE, class TEMPLATE>
static bool parseAction(
        YAML::Node&    mapNode,
        const String&  actionName,
        const Pattern& include,
        const Pattern& exclude,
        const bool     persist,
        Disposer&      disposer)
{
    bool  exists = false;
    VALUE value;

    if (Parser::tryDecode(mapNode, actionName, value)) {
        TEMPLATE      actTemplate(value, persist);
        PatternAction patAct(include, exclude, actTemplate);

        disposer.add(patAct);
        exists = true;
    }

    return exists;
}

/**
 * Decodes the action in a map node into an pattern-action template and adds it to a disposer.
 * @param[in] node         Map node
 * @param[in] include      What products to process
 * @param[in] exclude      What products to ignore
 * @param[in] disposer     Disposer
 * @throw InvalidArgument  Node isn't a map
 * @throw InvalidArgument  A subnode isn't the expected type
 * @throw InvalidArgument  A scalar value isn't the expected type
 * @throw LogicError       More than one action was found
 */
static void parseAction(
        YAML::Node&    node,
        const Pattern& include,
        const Pattern& exclude,
        Disposer&      disposer)
{
    if (!node.IsMap())
        throw INVALID_ARGUMENT("Node isn't a map-node");

    bool persist = false;
    Parser::tryDecode(node, "keepOpen", persist);

    int numActions = 0;

    if (parseAction<std::vector<String>, ExecTemplate>(node, "exec", include, exclude, persist,
            disposer))
        ++numActions;
    if (parseAction<std::vector<String>, PipeTemplate>(node, "pipe", include, exclude, persist,
            disposer))
        ++numActions;
    if (parseAction<String, FileTemplate>(node, "file", include, exclude, persist, disposer))
        ++numActions;
    if (parseAction<String, AppendTemplate>(node, "append", include, exclude, persist, disposer))
        ++numActions;

    if (numActions == 0)
        throw LOGIC_ERROR("Node has no action");
    if (numActions > 1)
        throw LOGIC_ERROR("Node has multiple actions");
}

/**
 * Decodes the sequence of actions in a subnode of a map node into pattern-action templates and adds
 * them to a disposer.
 * @param[in] node         Sequence node containing a sequence of actions in a subnode
 * @param[in] include      Products to process
 * @param[in] exclude      Products to ignore
 * @param[in] disposer     Disposer
 * @throw InvalidArgument  Node isn't a map
 * @throw InvalidArgument  A subnode isn't the expected type
 * @throw InvalidArgument  A scalar value isn't the expected type
 */
static void parseActions(
        YAML::Node&    node,
        const Pattern& include,
        const Pattern& exclude,
        Disposer&      disposer)
{
    if (!node.IsSequence())
        throw INVALID_ARGUMENT("Node isn't a sequence");

    for (size_t i = 0, n = node.size(); i < n; ++i) {
        auto subNode = node[i];
        parseAction(subNode, include, exclude, disposer);
    }
}

/**
 * Returns the string representation of the include and exclude patterns of a pattern-action.
 * @param[in] incl  Include pattern
 * @param[in] excl  Exclude pattern
 * @return          String representation of the include and exclude patterns
 */
static String to_string(
        const Pattern& incl,
        const Pattern& excl)
{
    const String& exclStr = excl.to_string();
    return exclStr.size() == 0
            ? '"' + incl.to_string() + '"'
            : "{incl=\"" + incl.to_string() + "\", excl=\"" + excl.to_string() + "\"}";
}

/**
 * Decodes the pattern and one or more actions in a map node into one or more pattern-action
 * templates and adds them to a disposer.
 * @param[in] node         Map node containing a pattern and one or more actions
 * @param[in] disposer     Disposer
 * @throw InvalidArgument  Node isn't a map
 * @throw InvalidArgument  A subnode isn't the expected type
 * @throw InvalidArgument  A scalar value isn't the expected type
 * @throw InvalidArgument  The node has both a single action and a subnode with actions
 */
static void parsePatActNode(
        YAML::Node& node,
        Disposer&   disposer)
{
    String  string(".*"); // Matches everything
    Parser::tryDecode(node, "include", string);
    Pattern include(string);

    string.clear(); // Matches nothing
    Parser::tryDecode(node, "exclude", string);
    Pattern exclude(string);

    if (!node["actions"]) {
        parseAction(node, include, exclude, disposer);
    }
    else {
        auto subNode = node["actions"];
        parseActions(subNode, include, exclude, disposer);
        if (node["exec"] || node["pipe"] || node["file"] || node["append"])
            throw LOGIC_ERROR("Node has single action and actions sequence");
    }
}


/**
 * Decodes the patterns and actions in a sequence subnode of a map node into pattern-action
 * templates and adds them to a disposer.
 * @param[in] mapNode      Map node
 * @param[in] disposer     Disposer
 * @throw InvalidArgument  Node isn't a map
 * @throw InvalidArgument  Expected subnode isn't a sequence
 * @throw InvalidArgument  A subnode isn't the expected type
 * @throw InvalidArgument  A scalar value isn't the expected type
 */
static void parsePatternActions(
        YAML::Node& mapNode,
        Disposer&   disposer)
{
    if (!mapNode.IsMap())
        throw INVALID_ARGUMENT("Node isn't a map");

    if (mapNode["patternActions"]) {
        auto seqNode = mapNode["patternActions"];
        if (!seqNode.IsSequence())
            throw INVALID_ARGUMENT("\"patternActions\" node isn't a sequence");

        for (size_t i = 0, n = seqNode.size(); i < n; ++i) {
            auto patActNode = seqNode[i];

            try {
                parsePatActNode(patActNode, disposer);
            }
            catch (const std::exception& ex) {
                std::throw_with_nested(INVALID_ARGUMENT("Couldn't parse \"patternAction\" #" +
                        std::to_string(i+1)));
            }
        } // Sequence loop
    } // Pattern-actions subnode exists
}

Disposer Disposer::createFromYaml(
        const String&      configFile,
        const LastProdPtr& lastProcessed,
        int                maxKeepOpen)
{
    YAML::Node node0;
    try {
        node0 = YAML::LoadFile(configFile);
    }
    catch (const std::exception& ex) {
        std::throw_with_nested(INVALID_ARGUMENT("Couldn't load configuration-file \"" + configFile +
                "\""));
    }

    try {
        // Set the maximum number of file descriptors to keep open
        Parser::tryDecode(node0, "maxKeepOpen", maxKeepOpen);
        if (maxKeepOpen < 0)
            throw INVALID_ARGUMENT("Invalid \"maxKeepOpen\" value: " + std::to_string(maxKeepOpen));

        // Construct the Disposer
        Disposer disposer{lastProcessed, maxKeepOpen};

        // Add pattern-actions to the Disposer
        parsePatternActions(node0, disposer);

        return disposer;
    } // YAML file loaded
    catch (const std::exception& ex) {
        std::throw_with_nested(RUNTIME_ERROR("Couldn't parse configuration-file \"" + configFile +
                "\""));
    }
}

String Disposer::getYaml() {
    YAML::Emitter yaml;

    yaml << YAML::BeginMap;

    yaml << YAML::Key << "maxKeepOpen";
    yaml << YAML::Value << pImpl->getMaxKeepOpen();

    yaml << YAML::Key << "patternActions";
    yaml << YAML::BeginSeq;

    for (auto patAct = pImpl->begin(), end = pImpl->end(); patAct != end; ++patAct) {
        yaml << YAML::BeginMap;
            yaml << YAML::Key << "include";
            yaml << YAML::Value << patAct->include.to_string();

            const String& string = patAct->exclude.to_string();
            if (string.size()) {
                yaml << YAML::Key << "exclude";
                yaml << YAML::Value << string;
            }

            yaml << YAML::Key;
            switch (patAct->actionTemplate.getType()) {
            case ActionTemplate::Type::EXEC:   yaml << "exec";   break;
            case ActionTemplate::Type::PIPE:   yaml << "pipe";   break;
            case ActionTemplate::Type::FILE:   yaml << "file";   break;
            case ActionTemplate::Type::APPEND: yaml << "append"; break;
            }
            const std::vector<String>& args = patAct->actionTemplate.getArgs();
            const bool needsSeq = args.size() > 1;
            yaml << YAML::Flow;
            if (needsSeq)
                yaml << YAML::BeginSeq;
                for (auto& arg : args)
                    yaml << arg;
            if (needsSeq)
                yaml << YAML::EndSeq;

            if (patAct->actionTemplate.getKeepOpen())
                yaml << YAML::Key << "keepOpen" << YAML::Value << "true";
        yaml << YAML::EndMap;
    }

    yaml << YAML::EndSeq;
    yaml << YAML::EndMap;

    return yaml.c_str();
}

} // namespace
