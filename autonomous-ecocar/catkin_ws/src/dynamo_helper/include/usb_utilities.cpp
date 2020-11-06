/*
  USB Utilities for automatic detection of removable USB storage devices
  and autopilot configuration files. Also includes functions for parsing the
  configuration file.

  Author: Thomas Passer Jensen (s134234@student.dtu.dk)

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#include <dirent.h>
#include <vector>
#include <string>
#include <iostream>
#include <unistd.h>
#include <sstream>
#include <fstream>

#include <sys/stat.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string.hpp>

inline bool is_file_exists(const std::string& name) {
  // source: https://stackoverflow.com/questions/12774207/fastest-way-to-check-if-a-file-exist-using-standard-c-c11-c
  struct stat buffer;
  return (stat (name.c_str(), &buffer) == 0);
}

std::string loadfile(std::string filename){
  // source: https://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring
  std::ifstream t(filename);
  std::stringstream buffer;
  buffer << t.rdbuf();

  std::string str = buffer.str();

  return str;
}

int getdir (std::string dir, std::vector<std::string> &files)
{
  // source: https://www.linuxquestions.org/questions/programming-9/c-list-files-in-directory-379323/
  DIR *dp;
  struct dirent *dirp;
  if((dp  = opendir(dir.c_str())) == NULL) {
    std::cout << "Error(" << errno << ") opening " << dir << std::endl;
    return errno;
  }

  while ((dirp = readdir(dp)) != NULL) {
    files.push_back(std::string(dirp->d_name));
  }
  closedir(dp);
  return 0;
}

std::vector<std::string> getUSBpaths(){
  std::vector<std::string> USBpaths;

  std::string mstabfile = loadfile("/etc/mtab");

  std::string dir = std::string("/dev/disk/by-id/");
  std::vector<std::string> files = std::vector<std::string>();

  getdir(dir,files);

  for (unsigned int i = 0;i < files.size();i++) {
    if(files[i].substr(0,3) == "usb"){
      // If string starts wit usb
      char buf[512];
      std::string path = dir + files[i];
      int count = readlink(path.c_str(), buf, sizeof(buf));
      if (count >= 0) {
        buf[count] = '\0';
        //std::cout << path << " -> " << buf << std::endl;
        std::string devpath(buf);
        //std::cout << devpath << ", pos: " << devpath.rfind("/") << std::endl;
        std::string devname = devpath.substr(devpath.rfind("/")+1, std::string::npos);

        if(std::isdigit(devname.back())){
          //std::cout << "USB found: " << files[i] << " -> " << devname << std::endl;
          size_t posbeg = 0;
          size_t posend = 0;
          // Find the line describing this device in /etc/mtab file
          if((posbeg = mstabfile.find(devname)) != std::string::npos){
            if((posend = mstabfile.find("\n", posbeg)) != std::string::npos){
              //std::cout << posbeg << " " << posend << std::endl;
              std::string mountpathstring = mstabfile.substr(posbeg,posend);
              //std::cout << mountpathstring << std::endl;
              // Find mount path in string
              if((posbeg = mountpathstring.find(" ")) != std::string::npos){
                if((posend = mountpathstring.find(" ", posbeg+1)) != std::string::npos){
                  //std::cout << posbeg << " " << posend << std::endl;
                  std::string mountpath = mountpathstring.substr(posbeg+1,posend-posbeg-1);
                  //std::cout << mountpath << std::endl;

                  // Fix spaces, these are writting as \040 in the file
                  boost::replace_all(mountpath, "\\040", " ");
                  //std::cout << mountpath << std::endl;

                  USBpaths.push_back(mountpath);
                } else {
                  std::cout << "Error parsing /etc/mtab file." << std::endl;
                }
              } else {
                std::cout << "Error parsing /etc/mtab file." << std::endl;
              }

            } else {
              std::cout << "Error parsing /etc/mtab file; couldn't find end of line." << std::endl;
            }
          } else {
            std::cout << "Error parsing /etc/mtab file; didn't find device." << std::endl;
          }
        } // not digit in end
      } else {
        std::cout << "Error finding symlink destination." << std::endl;
      }
    } else {
      //std::cout << "Not a USB storage device." << std::endl;
    }
  }

  return USBpaths;
}

std::string findAutopilotFile(){
  std::vector<std::string> USBmountpaths = getUSBpaths();

  std::vector<std::string> filepaths;

  if(USBmountpaths.size() > 0){
    for(unsigned int i = 0; i < USBmountpaths.size(); i++){
      std::cout << "Found USB storage device mounted at path: " << USBmountpaths[i] << std::endl;
      std::string filepath = USBmountpaths[i] + "/autopilot.conf";
      //std::cout << "Looing for config file at: " << filepath << std::endl;
      if(is_file_exists(filepath)){
        std::cout << "Found file: " << filepath << std::endl;
        filepaths.push_back(filepath);
      } else {
        std::cout << "No autopilot configuration file found on this drive. " << std::endl;
      }
    }
  } else {
    std::cout << "No USB storage devices found." << std::endl;
  }

  // If we found something, return the first one. Otherwise return empty string
  if(filepaths.size() > 0){
    return filepaths[0];
  } else {
    return std::string();
  }
}


std::vector<std::string> parseAutopilotFile(std::string str){
  std::vector<std::string> autonomous_nodes;
  size_t posbeg = 0;
  size_t posend = 0;
  size_t poscom = 0;
  std::string token;

  while((posbeg = str.find('"')) != std::string::npos) {
    // pos is begin quote

    if((posend = str.find('"', posbeg+1)) != std::string::npos){
      /*
      std::cout << str << std::endl;
      std::cout << "posbeg: " << posbeg << std::endl;
      std::cout << "posend: " << posend << std::endl;
      */
      token = str.substr(posbeg,posend-posbeg+1);
      //std::cout << ">" << token << "<" << std::endl;

      if((poscom = str.find(',', posend)) != std::string::npos){
        //std::cout << "found comma" << std::endl;
        if(token[0] == '"' && token.back() == '"'){ // If enclosed by quotation marks
          std::string nodestring = token.substr(1, token.length()-2); // Strip quotation marks
          boost::trim_right(nodestring);
          boost::trim_left(nodestring);
          //std::cout << ">>" << nodestring << "<<" << std::endl;
          autonomous_nodes.push_back(nodestring);
        } else {
          std::cout << "Invalid line" << std::endl;
        }

        str.erase(0, poscom + 1);
      } else {
        //std::cout << "Missing comma, must be end?" << std::endl;
        break;
      }

    } else {
      std::cout << "Missing end quote" << std::endl;
      break;
    }

  }

  if(token[0] == '"' && token.back() == '"'){ // If enclosed by quotation marks
    std::string nodestring = token.substr(1, token.length()-2); // Strip quotation marks
    boost::trim_right(nodestring);
    boost::trim_left(nodestring);
    //std::cout << ">>" << nodestring << "<<" << std::endl;
    autonomous_nodes.push_back(nodestring);
  } else {
    std::cout << "Invalid line" << std::endl;
  }

  return autonomous_nodes;
}

std::vector<std::string> getAutopilotNodes(){
  std::vector<std::string> autonomous_nodes;
  std::string str;

  std::string filename = findAutopilotFile();
  if(!filename.empty()){
     str = loadfile(filename);
     autonomous_nodes = parseAutopilotFile(str);
  } else {
    std::cout << "No configuration files found, aborting." << std::endl;
  }

  return autonomous_nodes;

}