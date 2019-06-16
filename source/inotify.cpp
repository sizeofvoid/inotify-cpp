#include <notify-cpp/inotify.h>

#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

namespace notifycpp
{
Inotify::Inotify()
    : mError(0)
    , mThreadSleep(250)
    , mInotifyFd(0)
{
    // Initialize inotify
    init();
}

Inotify::~Inotify()
{
    if (!close(mInotifyFd)) {
        mError = errno;
    }
}

void Inotify::init()
{
    stopped = false;
    mInotifyFd = inotify_init1(IN_NONBLOCK);
    if (mInotifyFd == -1) {
        mError = errno;
        std::stringstream errorStream;
        errorStream << "Can't initialize inotify ! " << strerror(mError) << ".";
        throw std::runtime_error(errorStream.str());
    }
}

/**
 * @brief Adds the given path and all files and subdirectories
 *        to the set of watched files/directories.
 *        Symlinks will be followed!
 *
 * @param path that will be watched recursively
 *
 */
    /* XXX
    if (isExists(path))
        if (isDirectory(path)) {
            fs::recursive_directory_iterator it(path, fs::symlink_option::recurse);
            fs::recursive_directory_iterator end;

            while (it != end) {
                fs::path currentPath = *it;

                if (isDirectory(currentPath)) {
                    watchFile(currentPath);
                }
                if (fs::is_symlink(currentPath)) {
                    watchFile(currentPath);
                }
                ++it;
            }
        }
        watchFile(path);
    } else {
        throw std::invalid_argument(
            "Can´t watch Path! Path does not exist. Path: " + path.string());
    }
    */


/**
 * @brief Adds a single file/directorie to the list of
 *        watches. Path and corresponding watchdescriptor
 *        will be stored in the directorieMap. This is done
 *        because events on watches just return this
 *        watchdescriptor.
 *
 * @param path that will be watched
 *
 */
void Inotify::watchFile(const FileSystemEvent& fse)
{
    if (!checkWatchFile(fse))
        return;

    mError = 0;
    int wd = 0;
    wd = inotify_add_watch(mInotifyFd, fse.getPath().c_str(), getEventMask(fse.getEvent()));

    if (wd == -1) {
        mError = errno;
        std::stringstream errorStream;
        if (mError == 28) {
            errorStream << "Failed to watch! " << strerror(mError)
                        << ". Please increase number of watches in "
                            "\"/proc/sys/fs/inotify/max_user_watches\".";
            throw std::runtime_error(errorStream.str());
        }

        errorStream << "Failed to watch! " << strerror(mError) << ". Path: " << fse.getPath();
        throw std::runtime_error(errorStream.str());
    }

    mDirectorieMap.emplace(wd, fse.getPath());
}

void Inotify::unwatch(const FileSystemEvent& fse)
{
    auto const itFound = std::find_if(
        std::begin(mDirectorieMap),
        std::end(mDirectorieMap),
        [&](std::pair<int, std::string> const& KeyString) { return KeyString.second == fse.getPath(); });

    if (itFound != std::end(mDirectorieMap))
        removeWatch(itFound->first);
}

/**
 * @brief Removes watch from set of watches. This
 *        is not done recursively!
 *
 * @param wd watchdescriptor
 *
 */
void Inotify::removeWatch(int wd)
{
    int result = inotify_rm_watch(mInotifyFd, wd);
    if (result == -1) {
        mError = errno;
        std::stringstream errorStream;
        errorStream << "Failed to remove watch! " << strerror(mError) << ".";
        throw std::runtime_error(errorStream.str());
    }
}
std::filesystem::path
Inotify::wdToPath(int wd)
{
    return mDirectorieMap.at(wd);
}

/**
 * @brief Blocking wait on new events of watched files/directories
 *        specified on the eventmask. FileSystemEvents
 *        will be returned one by one. Thus this
 *        function can be called in some while(true)
 *        loop.
 *
 * @return A new TFileSystemEventPtr
 */
TFileSystemEventPtr Inotify::getNextEvent()
{
    int length = 0;
    char buffer[EVENT_BUF_LEN];

    // Read Events from fd into buffer
    while (_Queue.empty() && !_Stopped) {
        length = 0;
        memset(buffer, '\0', sizeof(buffer));
        while (length <= 0 && !stopped && !_Stopped) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mThreadSleep));

            length = read(mInotifyFd, buffer, EVENT_BUF_LEN);
            if (length == -1) {
                mError = errno;
                if (mError != EINTR) {
                    continue;
                }
            }
        }

        if (stopped) {
            return nullptr;
        }

        int i = 0;
        while (i < length) {
            inotify_event* event = ((struct inotify_event*)&buffer[i]);

            if (event->mask & IN_IGNORED) {
                i += EVENT_SIZE + event->len;
                mDirectorieMap.erase(event->wd);
                continue;
            }
            auto path = wdToPath(event->wd);

            if (std::filesystem::is_directory(path))
                event->mask |= IN_ISDIR;

            _Queue.push(std::make_shared<FileSystemEvent>(path,
                                                         _EventHandler.getInotify(static_cast<uint32_t>(event->mask))));

            i += EVENT_SIZE + event->len;
        }
    }

    // Return next event
    auto event = _Queue.front();
    _Queue.pop();
    return event;
}

std::uint32_t
Inotify::getEventMask(const Event event) const
{
    return _EventHandler.convertToInotifyEvents(event);
}
}
