#include <Inotify.h>
#include <sys/inotify.h> /* IN_* */

int main(){

  std::vector<std::string> ignoredFolders;
  unsigned int eventTimeout = 0;
  std::string dir("/my_dir/");
  std::string file("my_file");
  uint32_t eventMask = IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVE;


  // Init inotify
  Inotify inotify(ignoredFolders, eventTimeout, eventMask);

  // Watch a file for changes
  if(!inotify.watchFile(file)){
    std::cout << "Can´t watch file " << file << "! Errno " << inotify.getLastError() <<std::endl;
    exit(0);
  }
  
  // Wait for 5 events of this file
  for(unsigned eventCount = 0; eventCount < 5; ++eventCount){
    inotify_event event = inotify.getNextEvent();
    std::cout << "Event " << inotify.maskToString(event.mask) << "for " << file << " was triggered!" << std::endl;
  }
  
  // Watch a directory (plus all subdirectories and files)
  if(!inotify.watchDirectoryRecursively(dir)){
  std::cout << "Can´t watch directory " << dir << "! Errno " << inotify.getLastError() <<std::endl;
    exit(0);
  }
  
  // Wait for 5 events of this directory
  for(unsigned eventCount = 0; eventCount < 5; ++eventCount){
    inotify_event event = inotify.getNextEvent();
    std::cout << "Event " << inotify.maskToString(event.mask) << "for " << file << " was triggered!" << std::endl;
  }
  
  return 0;
}