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
#include "FileUtil.h"
#include "HashSetQueue.h"
#include "Parser.h"
#include "PatternAction.h"
#include "Shield.h"
#include "ThreadException.h"

#include <fcntl.h>
#include <limits.h>
#include <list>
#include <queue>
#include <sys/wait.h>
#include <unistd.h>
#include <yaml-cpp/yaml.h>

namespace hycast {

/// An implementation of a class that locally disposes of data products
class Disposer::Impl
{
    using PatActs = std::list<PatternAction>;
    using ChildProcs = std::unordered_map<pid_t, String>;

    PatActs                    patActs;       ///< Pattern-actions to be matched
    std::unordered_set<Action> actionSet;     ///< Persistent (e.g., open) actions
    HashSetQueue<Action>       actionQueue;   ///< Persistent actions sorted by last use
    int                        maxPersistent; ///< Maximum number of persistent actions
    ChildProcs                 childProcs;    ///< Child processes to be waited on
    mutable Mutex              mutex;         ///< State mutex
    ThreadEx                   threadEx;      ///< Exception thrown on waiter thread
    Thread                     waitThread;    ///< Thread on which child processes are waited on

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
     * @throw RuntimeError  Too many open file descriptors or user processes
     */
    void process(
            Action         action,
            const char*    bytes,
            const ProdSize nbytes) {
        Guard  guard{mutex};
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

        if (pid >= 0)
            childProcs[pid] = childStr;
    }

    /*
     * TODO: Extract the child-waiting code (logChild(), killChildren(), runWaiter()) into a separate
     * class so that waiting on child processes can be done at the program-level rather than in this
     * class.
     */

    /**
     * Logs the termination of a child process.
     * @param[in] pid   PID of the child process
     * @param[in] stat  Status of the child process
     */
    void logChild(
            const pid_t pid,
            const int   stat) {
        String childId;

        if (childProcs.count(pid) == 0) {
            childId = std::to_string(pid);
        }
        else {
            childId = childProcs[pid];
            childProcs.erase(pid);
        }

        if (WIFSIGNALED(stat)) {
            if (WTERMSIG(stat) == SIGTERM) {
                LOG_INFO("Child process \"" + childId + "\" terminated by SIGTERM");
            }
            else {
                LOG_WARNING("Child process \"" + childId + "\" terminated by signal " +
                        std::to_string(WTERMSIG(stat)));
            }
        }
        else if (WIFEXITED(stat)) {
            const int exitStatus = WEXITSTATUS(stat);
            if (exitStatus == 0) {
                LOG_INFO("Child process \"" + childId + "\" exited successfully");
            }
            else {
                LOG_WARNING("Child process \"" + childId + "\" exited with status " +
                        std::to_string(exitStatus));
            }
        }
    }

    /**
     * Terminates all known child processes by sending them a SIGTERM and waits until they all
     * terminate. Logs the termination of each child process.
     */
    void killChildren() {
        Guard guard{mutex};

        for (auto& pair : childProcs)
            ::kill(pair.first, SIGTERM);

        for (auto& pair : childProcs) {
            int   stat;
            pid_t pid = ::waitpid(pair.first, &stat, 0);
            if (pid == -1) {
                if (errno != ECHILD)
                    throw SYSTEM_ERROR("Couldn't wait on child process {pid=" +
                            std::to_string(pid) + ", proc=" + pair.second);
            }
            else {
                logChild(pid, stat);
            }
        }
    }

    /**
     * Thread start function for waiting on child processes. Designed to be cancelled.
     */
    void runWaiter() {
        try {
            for (;;) {
                int    stat;
                pid_t  pid = wait(&stat);
                Shield shield{};

                if (pid == -1) {
                    if (errno != ECHILD)
                        throw SYSTEM_ERROR("Couldn't wait on child process " + std::to_string(pid));
                }
                else {
                    Guard guard{mutex};
                    logChild(pid, stat);
                }
            }
        }
        catch (const std::exception& ex) {
            threadEx.set(ex);
        }
    }

public:
    using Iterator = PatActs::iterator;

    /**
     * Default constructs.
     */
    Impl()
        : patActs()
        , actionSet(0)
        , actionQueue(0)
        , maxPersistent(0)
        , childProcs()
        , mutex()
        , threadEx()
        , waitThread(&Disposer::Impl::runWaiter, this)
    {}

    ~Impl() {
        int status = ::pthread_cancel(waitThread.native_handle());
        if (status && status != ESRCH)
            LOG_SYSERR("Couldn't cancel thread on which child processes are waited", status);
        waitThread.join();

        try {
            killChildren();
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex); // Destructors must not throw
        }
    }

    /**
     * Adds an entry.
     * @param[in] patAct  The pattern and action to add
     */
    void add(const PatternAction& patAct) {
        patActs.push_back(patAct);
    }

    /**
     * Sets the maximum number of file descriptors to keep open between products.
     * @param[in] maxKeepOpen  Maximum number of file descriptors to keep open
     */
    void setMaxKeepOpen(const int maxKeepOpen) {
        maxPersistent = maxKeepOpen;
    }

    /**
     * Returns the maximum number of file descriptors to keep open between products.
     * @return Maximum number of file descriptors to keep open
     */
    int getMaxKeepOpen() const {
        return maxPersistent;
    }

    /**
     * Returns an iterator over this instance's pattern-actions.
     * @return Iterator over pattern-actions
     */
    Iterator begin() {
        return patActs.begin();
    }

    /**
     * Returns an iterator beyond this instance's pattern-actions.
     * @return Iterator beyond pattern-actions
     */
    Iterator end() {
        return patActs.end();
    }

    /**
     * Disposes of a data product.
     * @param[in] prodInfo  Information on the product
     * @param[in] bytes     The product's data
     */
    void dispose(
            const ProdInfo prodInfo,
            const char*    bytes) {
        threadEx.throwIfSet();

        // TODO: Erase persistent actions that haven't been used for some time
        for (auto& patAct : patActs) {
            const auto& prodName = prodInfo.getName();
            std::smatch match;

            if (std::regex_search(prodName, match, patAct.include.getRegex()) &&
                    !std::regex_search(prodName, patAct.exclude.getRegex())) {
                auto action = getAction(patAct.actionTemplate, match);

                LOG_INFO("Executing " + action.to_string() + " on " + prodInfo.to_string());

                try {
                    process(action, bytes, prodInfo.getSize());
                }
                catch (const std::exception& ex) {
                    LOG_ERROR(ex, ("Couldn't process product " + prodInfo.to_string()).data());
                }
            } // Product should be processed
        } // Pattern-action loop
    }

    String getYaml() {
        YAML::Emitter yaml;

        yaml << YAML::BeginMap;

        yaml << YAML::Key << "MaxKeepOpen";
        yaml << YAML::Value << std::to_string(maxPersistent);

        yaml << YAML::Key << "PatternActions";
        yaml << YAML::BeginSeq;

        for (auto& patAct : patActs) {
            yaml << YAML::BeginMap;
                yaml << YAML::Key << "Include";
                yaml << YAML::Value << patAct.include.to_string();

                const String& string = patAct.exclude.to_string();
                if (string.size()) {
                    yaml << YAML::Key << "Exclude";
                    yaml << YAML::Value << string;
                }

                yaml << YAML::Key;
                switch (patAct.actionTemplate.getType()) {
                case ActionTemplate::Type::EXEC:   yaml << "Exec";   break;
                case ActionTemplate::Type::PIPE:   yaml << "Pipe";   break;
                case ActionTemplate::Type::FILE:   yaml << "File";   break;
                case ActionTemplate::Type::APPEND: yaml << "Append"; break;
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
    : pImpl(new Impl{})
{}

void Disposer::setMaxKeepOpen(const int maxKeepOpen) noexcept {
    pImpl->setMaxKeepOpen(maxKeepOpen);
}

int Disposer::getMaxKeepOpen() const noexcept {
    return pImpl->getMaxKeepOpen();
}

void Disposer::add(const PatternAction& patAct) {
    pImpl->add(patAct);
}

void Disposer::dispose(
        const ProdInfo prodInfo,
        const char*    bytes) const {
    pImpl->dispose(prodInfo, bytes);
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
 * @param[in] actionName   Action name
 * @param[in] include      What products to process
 * @param[in] exclude      What products to ignore
 * @param[in] persist      Keep the output-file open?
 * @param[in] disposer     Disposer
 * @retval true            An action was found
 * @retval false           An action was not found
 * @throw InvalidArgument  Node isn't a map
 * @throw InvalidArgument  A subnode isn't the expected type
 * @throw InvalidArgument  A scalar value isn't the expected type
 * @throw LogicError       More than one action was found
 */
static bool parseAction(
        YAML::Node&    mapNode,
        const Pattern& include,
        const Pattern& exclude,
        Disposer&      disposer)
{
    bool persist = false;
    Parser::tryDecode(mapNode, "KeepOpen", persist);

    int numActions = 0;

    if (parseAction<std::vector<String>, ExecTemplate>(mapNode, "Exec", include, exclude, persist,
            disposer))
        ++numActions;
    if (parseAction<std::vector<String>, PipeTemplate>(mapNode, "Pipe", include, exclude, persist,
            disposer))
        ++numActions;
    if (parseAction<String, FileTemplate>(mapNode, "File", include, exclude, persist,
            disposer))
        ++numActions;
    if (parseAction<String, AppendTemplate>(mapNode, "Append", include, exclude, persist,
            disposer))
        ++numActions;

    if (numActions > 1)
        throw LOGIC_ERROR("Node has multiple actions");

    return numActions == 1;
}

/**
 * Decodes the sequence of actions in a subnode of a map node into pattern-action templates and adds
 * them to a disposer.
 * @param[in] node         Map node containing a sequence of actions in a subnode
 * @param[in] include      Products to process
 * @param[in] exclude      Products to ignore
 * @param[in] disposer     Disposer
 * @retval true            An action was found.
 * @retval false           An action was not found.
 * @throw InvalidArgument  Node isn't a map
 * @throw InvalidArgument  A subnode isn't the expected type
 * @throw InvalidArgument  A scalar value isn't the expected type
 */
static bool parseActions(
        YAML::Node&    mapNode,
        const Pattern& include,
        const Pattern& exclude,
        Disposer&      disposer)
{
    bool haveAction = false;

    if (!mapNode.IsMap())
        throw INVALID_ARGUMENT("Node isn't a map");

    if (mapNode["Actions"]) {
        auto seqNode = mapNode["Actions"];
        if (!seqNode.IsSequence())
            throw INVALID_ARGUMENT("Node's value isn't a sequence");

        for (size_t i = 0, n = seqNode.size(); i < n; ++i) {
            auto actionNode = seqNode[i];
            if (!parseAction(actionNode, include, exclude, disposer)) {
                LOG_WARNING("Node has no action");
            }
            else {
                haveAction = true;
            }
        }
    }

    return haveAction;
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
 * @param[in] mapNodeode   Map node containing a pattern and one or more actions
 * @param[in] disposer     Disposer
 * @retval true            A pattern-action was found.
 * @retval false           A pattern-action was not found.
 * @throw InvalidArgument  Node isn't a map
 * @throw InvalidArgument  A subnode isn't the expected type
 * @throw InvalidArgument  A scalar value isn't the expected type
 * @throw InvalidArgument  The node has both a single action and a subnode with actions
 */
static bool parsePatActNode(
        YAML::Node& mapNode,
        Disposer&   disposer)
{
    bool havePatAct = false;

    String  string(".*"); // Matches everything
    Parser::tryDecode(mapNode, "Include", string);
    Pattern include(string);

    string.clear(); // Matches nothing
    Parser::tryDecode(mapNode, "Exclude", string);
    Pattern exclude(string);

    if (parseActions(mapNode, include, exclude, disposer)) {
        if (parseAction(mapNode, include, exclude, disposer))
            throw INVALID_ARGUMENT("Pattern " + to_string(include, exclude) + " has a single "
                    "action and a subnode with actions");
        havePatAct = true;
    }
    else if (!parseAction(mapNode, include, exclude, disposer)) {
        LOG_WARNING("Pattern " + to_string(include, exclude) + " has no action");
    }

    return havePatAct;
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
static bool parsePatternActions(
        YAML::Node& mapNode,
        Disposer&   disposer)
{
    if (!mapNode.IsMap())
        throw INVALID_ARGUMENT("Node isn't a map");

    bool havePatAct = false;

    if (mapNode["PatternActions"]) {
        auto seqNode = mapNode["PatternActions"];
        if (!seqNode.IsSequence())
            throw INVALID_ARGUMENT("\"PatternActions\" node isn't a sequence");

        for (size_t i = 0, n = seqNode.size(); i < n; ++i) {
            auto patActNode = seqNode[i];

            try {
                if (parsePatActNode(patActNode, disposer))
                    havePatAct = true;
            }
            catch (const std::exception& ex) {
                std::throw_with_nested(INVALID_ARGUMENT("Couldn't parse pattern-action #" +
                        std::to_string(i+1)));
            }
        } // Sequence loop
    } // Pattern-actions subnode exists

    return havePatAct;
}

Disposer Disposer::createFromYaml(const String& configFile)
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
        int maxKeepOpen = 20;
        Parser::tryDecode(node0, "MaxKeepOpen", maxKeepOpen);
        if (maxKeepOpen < 0)
            throw INVALID_ARGUMENT("Invalid \"MaxKeepOpen\" value: " + std::to_string(maxKeepOpen));

        // Construct the Disposer
        Disposer disposer{};
        disposer.setMaxKeepOpen(maxKeepOpen);

        // Add pattern-actions to the Disposer
        if (!parsePatternActions(node0, disposer))
            LOG_WARNING("No pattern-action found in configuration-file \"" + configFile + "\"");

        return disposer;
    } // YAML file loaded
    catch (const std::exception& ex) {
        std::throw_with_nested(RUNTIME_ERROR("Couldn't parse configuration-file \"" + configFile +
                "\""));
    }
}

String Disposer::getYaml(const Disposer& disposer) {
    auto          pImpl = disposer.pImpl;
    YAML::Emitter yaml;

    yaml << YAML::BeginMap;

    yaml << YAML::Key << "MaxKeepOpen";
    yaml << YAML::Value << pImpl->getMaxKeepOpen();

    yaml << YAML::Key << "PatternActions";
    yaml << YAML::BeginSeq;

    for (auto patAct = pImpl->begin(), end = pImpl->end(); patAct != end; ++patAct) {
        yaml << YAML::BeginMap;
            yaml << YAML::Key << "Include";
            yaml << YAML::Value << patAct->include.to_string();

            const String& string = patAct->exclude.to_string();
            if (string.size()) {
                yaml << YAML::Key << "Exclude";
                yaml << YAML::Value << string;
            }

            yaml << YAML::Key;
            switch (patAct->actionTemplate.getType()) {
            case ActionTemplate::Type::EXEC:   yaml << "Exec";   break;
            case ActionTemplate::Type::PIPE:   yaml << "Pipe";   break;
            case ActionTemplate::Type::FILE:   yaml << "File";   break;
            case ActionTemplate::Type::APPEND: yaml << "Append"; break;
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
                yaml << YAML::Key << "KeepOpen" << YAML::Value << "true";
        yaml << YAML::EndMap;
    }

    yaml << YAML::EndSeq;
    yaml << YAML::EndMap;

    return yaml.c_str();
}

} // namespace
