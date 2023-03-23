/**
 * This file defines the actions for local processing of a data-product.
 *
 *  @file:  Action.cpp
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

#include "Action.h"
#include "FileUtil.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

namespace hycast {

/// Implementation of an action
class Action::Impl
{
public:
    /// The type of action to be performed
    enum ActionType {
        PIPE,   ///, Pipe product to program
        FILE,   ///< Write product to file
        APPEND, ///< Append product to file
        EXEC    ///< Execute external program
    };

private:
    ActionType          actionType;
    const size_t        hashValue;

    /**
     * Returns the hash code of an action-type.
     * @param[in] actionType  The action-type
     * @return                Corresponding hash code
     */
    static size_t hashActionType(const ActionType actionType) {
        static auto hashInt = std::hash<int>();
        return hashInt(static_cast<int>(actionType));
    }

    /**
     * Returns the hash code of a vector of strings.
     *
     * @param[in] args  Vector of string
     * @return          Corresponding hash code
     */
    static size_t hashArgs(const std::vector<String>& args) noexcept {
        static auto hashString = std::hash<String>();
        size_t      hashValue = 0;
        for (const String arg : args)
            hashValue ^= hashString(arg);
        return hashValue;
    }

protected:
    std::vector<String> args;    ///< Command-line arguments
    const bool          persist; ///< Instance should persist between calls to `process()`?

    /**
     * Returns the single-string representation of the command-line arguments.
     *
     * @return Single-string representation of the command-line arguments
     */
    String cmdVec() const {
        String cmd = '[' + args[0];

        for (auto i = 1; i < args.size(); ++i)
            cmd += ", " + args[i];
        cmd += ']';

        return cmd;
    }

public:
    /**
     * Constructs.
     * @param[in] actionType  Type of action
     * @param[in] args        Command-line arguments
     * @param[in] persist     Should this instance keep the file descriptor open?
     */
    Impl(   const ActionType           actionType,
            const std::vector<String>& args,
            const bool                 persist)
        : actionType{actionType}
        , hashValue{hashActionType(actionType) ^ hashArgs(args)}
        , args(args)
        , persist{persist}
    {}

    virtual ~Impl() {
    }

    /**
     * Returns the string representation of this instance.
     * @return The string representation of this instance
     */
    String to_string() const {
        String cmd;
        switch (actionType) {
        case ActionType::PIPE: {
            cmd = "PIPE";
            break;
        }
        case ActionType::FILE: {
            cmd = "FILE";
            break;
        }
        case ActionType::APPEND: {
            cmd = "APPEND";
            break;
        }
        default:
            throw LOGIC_ERROR("Unknown action type");
        }
        return "{cmd=" + cmd + ", args=" + cmdVec() + "}";
    }

    /**
     * Should the file descriptor be kept open?
     * @retval true     Yes
     * @retval false    No
     */
    bool shouldPersist() const noexcept {
        return persist;
    }

    /**
     * Returns the hash code of this instance.
     * @return The hash code of this instance
     */
    size_t hash() const noexcept {
        return hashValue;
    }

    /**
     * Indicates if this instance is equal to another.
     * @param[in] rhs      The other instance
     * @retval    true     This instance is equal to the other
     * @retval    false    This instance is not equal to the other
     */
    bool operator==(const Impl& rhs) const noexcept {
        if (actionType != rhs.actionType)
            return false;

        const auto nargs = args.size();
        if (nargs != rhs.args.size())
            return false;

        for (int i = 0; i < nargs; ++i)
            if (args[i] != rhs.args[i])
                return false;

        return true;
    }

    /**
     * Performs the action.
     * @param[in] data      The data to be processed
     * @param[in] nbytes    The amount of data in bytes
     * @retval    true      Success
     * @retval    false     Too many file descriptors are open
     * @throw SystemError   System failure
     */
    virtual bool process(
            const char* data,
            size_t      nbytes) =0;
};

Action::Action(Impl* const pImpl)
    : pImpl{pImpl}
{}

Action::operator bool() const noexcept {
    return static_cast<bool>(pImpl);
}

String Action::to_string() const {
    return pImpl->to_string();
}

bool Action::shouldPersist() const noexcept {
    return pImpl->shouldPersist();
}

size_t Action::hash() const noexcept {
    return pImpl->hash();
}

bool Action::operator==(const Action& rhs) const noexcept {
    return *pImpl == *rhs.pImpl;
}

bool Action::process(
        const char* data,
        size_t      nbytes) {
    return pImpl->process(data, nbytes);
}

/******************************************************************************/

/// Action to pipe a data-product to a decoder
class PipeImpl final : public Action::Impl
{
private:
    int   pipeFds[2];
    pid_t decoderPid;

    /**
     * Indicates if the pipe is open.
     *
     * @retval true     The pipe is open
     * @retval false    The pipe is not open
     */
    inline bool pipeOpen() {
        return pipeFds[1] >= 0;
    }

    /**
     * Ensures that a given file descriptor will be closed upon execution of an exec(2) family
     * function.
     *
     * @param[in] fd      The file descriptor to be set to close-on-exec().
     * @throw SystemError  Couldn't get file descriptor flags
     * @throw SystemError  Couldn't set file descriptor to close-on-exec()
     */
    static void ensureCloseOnExec(const int fd)
    {
        int flags = ::fcntl(fd, F_GETFD);

        if (-1 == flags)
            throw SYSTEM_ERROR("Couldn't get flags for file descriptor %d", fd);

        if (!(flags & FD_CLOEXEC) && (-1 == ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC)))
            throw SYSTEM_ERROR("Couldn't set file descriptor %d to close-on-exec()", fd);
    }

    /**
     * Creates the pipe to the decoder.
     *
     * @retval true        Success
     * @retval false       Too many file descriptors are open
     * @throw SystemError  Couldn't create pipe
     * @throw SystemError  Couldn't get file descriptor flags
     * @throw SystemError  Couldn't set file descriptor to close-on-exec()
     */
    bool createPipe() {
        LOG_ASSERT(!pipeOpen());
        auto status = ::pipe(pipeFds);
        if (status) {
            if (errno == EMFILE || errno == ENFILE)
                return false;
            throw SYSTEM_ERROR("::pipe() failure");
        }
        /*
         * Ensure that the write-end of the pipe will close upon execution
         * of an exec(2) family function because no child processes should
         * inherit it.
         */
        ensureCloseOnExec(pipeFds[1]);

        return true;
    }

    /**
     * Forks this process.
     *
     * @throw SystemError  Couldn't fork process
     */
    bool fork() {
        decoderPid = ::fork();

        if (decoderPid == -1) {
            if (errno == EAGAIN)
                return false;
            throw SYSTEM_ERROR("fork() failure");
        }

        return true;
    }

    /**
     * Configures the parent (pipe writer) process
     */
    inline void configParent() {
        // Close the read-end of the pipe because it won't be used.
        (void)::close(pipeFds[0]);
        pipeFds[0] = -1;
    }

    /**
     * Executes the decoder by replacing this process.
     *
     * @throw SystemError  Couldn't make decoder a process-group leader
     * @throw SystemError  Couldn't redirect standard input to read-end of pipe
     * @throw SystemError  Couldn't execute decoder
     */
    void execChild() {
        (void)::signal(SIGTERM, SIG_DFL);

        /*
         * This process is made its own process-group leader to isolate it from signals sent to the
         * parent process.
         */
        if (::setpgid(0, 0) == -1)
            throw SYSTEM_ERROR("Couldn't make decoder a process-group leader");

        /*
         * It is assumed that the standard output and error streams are correctly established and
         * should not be modified.
         */

         // Ensure that the standard input stream is the read-end of the pipe.
        if (STDIN_FILENO != pipeFds[0]) {
            if (-1 == ::dup2(pipeFds[0], STDIN_FILENO))
                throw SYSTEM_ERROR("Couldn't redirect standard input to read-end of pipe: "
                        "pipeFds[0]=" + std::to_string(pipeFds[0]));

            (void)::close(pipeFds[0]);
            pipeFds[0] = STDIN_FILENO;
        }

        // Construct the argument vector
        const auto nargs = args.size();
        char*      argv[nargs+1];
        for (int i = 0; i < nargs; ++i)
            argv[i] = const_cast<char*>(args[i].data());
        argv[nargs] = nullptr;

        (void)::execvp(argv[0], argv);
        throw SYSTEM_ERROR("Couldn't execute decoder \"" + args[0] + "\": PATH=" +
                ::getenv("PATH"));
    }

    /**
     * Executes the decoder as a child process.
     *
     * @retval true        Success
     * @retval false       Too many child processes
     * @throw SystemError  Couldn't fork process
     * @throw SystemError  Couldn't make decoder a process-group leader
     * @throw SystemError  Couldn't redirect standard input to read-end of pipe
     * @throw SystemError  Couldn't execute decoder
     */
    bool execDecoder() {
        LOG_ASSERT(pipeOpen());

        if (!fork())
            return false;

        if (decoderPid)
            configParent();
        else
            execChild();

        return true;
    }

    /**
     * @throws SystemError  Couldn't wait on decoder
     */
    void waitForDecoder() {
        int exitStatus;
        for (;;) {
            if (::waitpid(decoderPid, &exitStatus, 0) == -1)
                throw SYSTEM_ERROR("waitpid() failure for decoder " + cmdVec());

            if (WIFEXITED(exitStatus)) {
                if (WIFSIGNALED(exitStatus)) {
                    LOG_WARNING("Decoder %s terminated due to uncaught signal %d",
                            cmdVec().data(), WTERMSIG(exitStatus));
                }
                else {
                    exitStatus = WEXITSTATUS(exitStatus);
                    if (exitStatus) {
                        LOG_WARNING("Decoder %s exited with status %d", cmdVec().data(),
                                exitStatus);
                    }
                    else {
                        LOG_DEBUG("Decoder %s exited successfully", cmdVec().data());
                    }
                }
                decoderPid = 0;
                break;
            }
        }
    }

public:
    /**
     * Constructs.
     * @param[in] args      Command-line argument templates
     * @param[in] keepOpen  Reified instances should persist?
     */
    PipeImpl(
            const std::vector<String>& args,
            const bool                 keepOpen)
        : Impl(ActionType::PIPE, args, keepOpen)
        , pipeFds{-1, -1}
        , decoderPid{0}
    {}

    ~PipeImpl() {
        for (auto fd : pipeFds)
            if (fd >= 0)
                ::close(fd);

        if (decoderPid) {
            try {
                waitForDecoder();
            }
            catch (const std::exception& ex) {
                LOG_ERROR(ex);
            }
        }
    }

    /**
     * @throw SystemError  Couldn't create pipe
     * @throw SystemError  Couldn't get file descriptor flags
     * @throw SystemError  Couldn't set file descriptor to close-on-exec()
     * @throw SystemError  Couldn't fork process
     * @throw SystemError  Couldn't make decoder a process-group leader
     * @throw SystemError  Couldn't redirect standard input to read-end of pipe
     * @throw SystemError  Couldn't execute decoder
     * @throw SystemError  Couldn't write product to decoder
     * @throw SystemError  Couldn't wait on decoder
     */
    bool process(
            const char* data,
            size_t      nbytes) override {
        if (!pipeOpen() && (!createPipe() || !execDecoder()))
            return false;

        while (nbytes > 0) {
            auto nwritten = ::write(pipeFds[1], data, nbytes);
            if (nwritten == -1)
                throw SYSTEM_ERROR("Couldn't write " + std::to_string(nbytes) +
                        " bytes to decoder " + cmdVec());
            nbytes -= nwritten;
            data += nwritten;
        }

        if (!persist) {
            ::close(pipeFds[1]);
            pipeFds[1] = -1;

            waitForDecoder();
        }

        return true;
    }
};

PipeAction::PipeAction(
        const std::vector<String>& args,
        const bool                 keepOpen)
    : Action{new PipeImpl(args, keepOpen)}
{}

/******************************************************************************/

/// Base action to write a data-product to a file
class WriteImpl final : public Action::Impl
{
private:
    int      fd;       ///< File descriptor for output file
    int      oflags;   ///< `::open()` flags
    ProdSize prodSize; ///< Product size in bytes

    /**
     * Opens the output file.
     *
     * @param[in] nbytes   Size of the data-product in bytes
     * @retval    true     Success
     * @retval    false    Too many file descriptors are open
     * @throw SystemError  Couldn't open file
     */
    bool openFile(const ProdSize nbytes) {
        LOG_ASSERT(fd < 0);

        FileUtil::ensureDir(FileUtil::dirname(args[0]), 0777);

        fd = ::open(args[0].data(), oflags, 0666);
        if (fd < 0) {
            if (errno == EMFILE || errno == ENFILE)
                return false;
            throw SYSTEM_ERROR("::open() failure on file \"" + args[0] + "\"");
        }

        prodSize = nbytes;

        return true;
    }

public:
    using Action::Impl::ActionType;

    /**
     * Constructs.
     *
     * @param[in] actionType  The type of action (e.g., FILE, APPEND)
     * @param[in] args        Single pathname of output file
     * @param[in] oflag       `open()` flag (e.g., O_TRUNC, O_APPEND)
     * @param[in] keepOpen    Should this action stay open on the output file between products?
     */
    WriteImpl(
            const ActionType           actionType,
            const std::vector<String>& args,
            const int                  oflag,
            const bool                 keepOpen)
        : Impl(actionType, args, keepOpen)
        , fd{-1}
        , oflags(O_WRONLY|O_CREAT|O_CLOEXEC|O_SYNC|oflag)
        , prodSize(0)
    {
        if (args.size() != 1)
            throw INVALID_ARGUMENT("Only a single pathname argument allowed: " + cmdVec());
    }

    ~WriteImpl() {
        if (fd >= 0)
            ::close(fd);
    }

    /**
     * @param[in] bytes    Data-product bytes
     * @param[in] nbytes   Number of data-product bytes
     * @throw SystemError  Couldn't open file
     * @throw SystemError  Couldn't truncate file
     * @throw SystemError  Couldn't write product to file
     */
    bool process(
            const char* bytes,
            size_t      nbytes) override {
        if (fd < 0) {
            if (!openFile(nbytes))
                return false;
        }
        else if ((oflags & O_TRUNC) && ftruncate(fd, 0)) {
            throw SYSTEM_ERROR("Couldn't truncate file \"" + args[0] + "\"");
        }

        const auto nwritten = ::write(fd, bytes, nbytes);
        if (nwritten != nbytes)
            throw SYSTEM_ERROR("Wrote only " + std::to_string(nwritten) + " bytes out of " +
                    std::to_string(nbytes) + " to file \"" + args[0] + "\"");

        if (!persist) {
            ::close(fd);
            fd = -1;
        }

        return true;
    }
};

/******************************************************************************/

/// Action to overwrite an output file with a data-product
FileAction::FileAction(
        const std::vector<String>& args,
        const bool                 keepOpen)
    : Action{new WriteImpl(Action::Impl::FILE, args, O_TRUNC, keepOpen)}
{}

/******************************************************************************/

/// Action to append a data-product to an output file
AppendAction::AppendAction(
        const std::vector<String>& args,
        const bool                 keepOpen)
    : Action{new WriteImpl(Action::Impl::APPEND, args, O_APPEND, keepOpen)}
{}

} // namespace
