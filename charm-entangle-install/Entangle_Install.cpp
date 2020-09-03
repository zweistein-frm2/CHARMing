// Boost_Installer.cpp : Defines the entry point for the application.
//

#include "Entangle_Install.h"
#define BOOST_PROCESS_WINDOWS_USE_NAMED_PIPE
#include <boost/process.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/exception/all.hpp>
#include <cmrc/cmrc.hpp>
#include "Zweistein.HomePath.hpp"
#include <fstream>
#include <iostream>
#include <string_view>
#include <boost/algorithm/string.hpp>
#include <regex>
#include <boost/algorithm/string/trim.hpp>
#include <boost/regex.hpp>
#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#else
#if defined(__cplusplus)
extern "C" {
#endif
#include <unistd.h>
#if defined(__cplusplus)
}
#endif
#endif

CMRC_DECLARE(resources);
std::string PROJECT_NAME("charm-entangle-install");
namespace bp = boost::process;

namespace Zweistein {

    struct Version {
        Version():Major(0),Minor(0),patch("") {};
        int Major;
        int Minor;
        std::string patch;

    };

    enum Linux_Flavour {
        debian = 0,
        redhat = 1

    };
    std::vector<std::string> install_log;
      boost::filesystem::path inidirectory;
      boost::filesystem::path UserIniDir() {
          if (inidirectory.empty()) {
#ifdef _WIN32
              boost::filesystem::path homepath = Zweistein::GetHomePath();
              inidirectory = homepath;
              inidirectory /= ".CHARMing";
#else
              //inidirectory = "/etc/CHARMing";
              inidirectory = "/home/localadmin/.CHARMing";
#endif
              if (!boost::filesystem::exists(inidirectory)) {
                  boost::filesystem::create_directory(inidirectory);
              }
              inidirectory += boost::filesystem::path::preferred_separator;

          }
          return inidirectory;

      }


      class Process {
      public:
           Process(std::string& cmd, const int timeout = 0) : command(cmd), timeout(timeout), deadline_timer(ios) {}

           static std::string exec(std::string cmd) {
               std::array<char, 128> buffer;
               std::string result;
               std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
               if (!pipe) {
                   throw std::runtime_error("popen() failed!");
               }
               while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                   result += buffer.data();
                   std::cout << buffer.data();
               }
               return result;
           }

           int run() {

               bp::ipstream output;
               bp::ipstream err;
               std::thread reader([&output] {
                   std::string line;
                   while (std::getline(output, line))
                       std::cout <<line <<  std::endl;
                   });

               if (timeout != 0) {
                   deadline_timer.expires_from_now(boost::posix_time::seconds(timeout));
                   deadline_timer.async_wait(boost::bind(&Process::timeout_handler, this, boost::asio::placeholders::error));
               }
               bp::child c(command, bp::std_in.close(), bp::std_out > output, bp::std_err > err);

               while (c.running()) {
                   std::this_thread::sleep_for(std::chrono::milliseconds(250));
                   std::cout << "(main thread working)" << std::endl;
               }

               std::cout << "(done)" << std::endl;
               output.pipe().close();
               reader.join();
               int rv = c.exit_code();
               if (rv != 0) {
                   std::string line;
                   while (std::getline(err, line)) {
                       std::cout << "Error: '" << line << "'" << std::endl;
                   }
               }
              return rv;
          }
      private:
          void timeout_handler(boost::system::error_code ec) {
              if (stopped)
                  return;

              if (ec == boost::asio::error::operation_aborted)
                  return;

              if (deadline_timer.expires_at() <= boost::asio::deadline_timer::traits_type::now()) {
                  std::cout << "Time Up!" << std::endl;
                  group.terminate(); // NOTE: anticipate errors
                  std::cout << "Killed the process and all its decendants" << std::endl;
                  killed = true;
                  stopped = true;
                  deadline_timer.expires_at(boost::posix_time::pos_infin);
              }
              //NOTE: don't make it a loop
              //deadline_timer.async_wait(boost::bind(&Process::timeout_handler, this, boost::asio::placeholders::error));
          }
          const std::string command;
          const int timeout;
          bool killed = false;
          bool stopped = false;

          std::string stdOut;
          std::string stdErr;
          int returnStatus = 0;

          boost::asio::io_service ios;
          boost::process::group group;
          boost::asio::deadline_timer deadline_timer;

      };

      std::vector<std::string>  RunCmdline(std::string cmdline, std::string expectedlinestart="", std::string to_stdin="") {


          std::vector<std::string> data;
          std::cout << cmdline << std::endl;
          std::error_code ec;
          bp::ipstream is;
          bp::ipstream ierr;
          bp::opstream oin;
          bool errpushed = false;

          try {
              bp::child c(cmdline, bp::std_out > is, bp::std_err > ierr, bp::std_in < oin);
              if (!to_stdin.empty()) {
                  oin << to_stdin << std::endl;
              }
              c.wait(ec);


              std::string line;

              while (std::getline(ierr, line) && !line.empty()) {
                  std::cerr << line << std::endl;
                  if (!errpushed) {
                      errpushed = true;
                      data.push_back("stderr");
                  }
                  data.push_back(line);
              }



              while (std::getline(is, line) && !line.empty()) {
                  //std::cout << line << std::endl;
                  data.push_back(line);
                  std::string_view sv(line);
                  if (!expectedlinestart.empty()) {
                      size_t s = sv.find(expectedlinestart);
                      if (s==0) return data;
                  }

              }





          }
          catch (std::exception& e) {
              std::cerr << cmdline<< " : "<< e.what() << std::endl;
              Zweistein::install_log.push_back(e.what());
          }
          for (auto& a : data) Zweistein::install_log.push_back(a);
          return data;
      }
      Zweistein::Version GetProgamVersion(std::string name) {
          Zweistein::Version version;
          std::string cmdline = name + " --version";
          std::vector<std::string> lines= RunCmdline(cmdline, name);
          if (!lines.empty()) {


              if (boost::algorithm::istarts_with(lines[0], name)) {
                  std::string str = lines[0].substr(name.length());

                  str = std::regex_replace(str, std::regex("([^(]*)\\([^)]*\\)(.*)"), "$1$2");
                  boost::algorithm::trim(str);

                  std::vector<std::string> tokens;
                  boost::split(tokens, str, boost::is_any_of(" "));
                  for (std::string& s : tokens) {
                      char c = s[0];
                      if (std::isdigit(c)) {
                          str = s;
                          break;
                      }
                  }

                  std::vector<std::string> strs;
                  boost::split(strs, str, boost::is_any_of(".-_ "));
                  try {
                      version.Major = std::stoi(strs[0]);
                      version.Minor = std::stoi(strs[1]);
                      version.patch = strs[2];
                  }
                  catch (std::exception& e) {
                      std::cerr << e.what() << std::endl;
                  }

              }
          }

          return version;

      }
      bool RunShellScript(std::string script, std::string fromscript="") {
          std::string cmdline = "chmod +x " + script;
          std::vector<std::string> result = Zweistein::RunCmdline(cmdline);
          for (auto& a : result) if (a.find("stderr")) return false;

          cmdline = "";

          if (!fromscript.empty()) {
              cmdline += fromscript + " ";
                  // we have sudo
           }
          cmdline +=  script;
          //result=RunCmdline(cmdline);
          Zweistein::Process::exec(cmdline);
          for (auto& a : result) if (a.find("stderr")) return false;
          return true;

      }
      boost::filesystem::path ResourcePath(std::string resource, cmrc::embedded_filesystem &fs) {
           auto file1 = fs.open(resource);
           boost::filesystem::path p = Zweistein::UserIniDir();
          p.append(resource);
          std::string tmp = p.string();
          //if (!boost::filesystem::exists(tmp)) {
          {
              std::ofstream o(p.c_str(), std::ofstream::binary);
              o.write(file1.begin(), file1.size());
              std::string ext = p.extension().string();
              if (ext == ".sh") {
                  std::string cmdline = "chmod +x " + p.string();
                  std::vector<std::string> result = Zweistein::RunCmdline(cmdline);
                  for (auto& a : result) if (a.find("stderr")) {
                      std::cout << a << std::endl;
                  }
              }

          }
          return p;
      }

}


int main()
{
	try {
        auto fs = cmrc::resources::get_filesystem();
        std::cout << "Installs  entangle device interface for CHARMing Neutron detector software" << std::endl;
        std::string PYTHON = "python";
#ifndef _WIN32
        int a =geteuid();
        if (a != 0) std::cout << "must run with sudo privileges.";
        PYTHON = "python3";
#endif
       std::string pythonscript = Zweistein::ResourcePath("syspath.py", fs).string();
       std::string cmdline = PYTHON + " " + pythonscript;
       std::vector<std::string> pythonsyspath = Zweistein::RunCmdline(cmdline);
       boost::filesystem::path entangle_root;
       for(auto & psyspath : pythonsyspath) {

           auto aa = boost::filesystem::path(psyspath).rbegin();
           std::string f = aa->string();
           if (boost::algorithm::istarts_with(f, "entangle-")) {
               entangle_root = psyspath;
               entangle_root /= "entangle";
               std::cout << "Entangle install root set to: " << entangle_root << std::endl;
               break;
           }

       }
        auto p = Zweistein::UserIniDir();
       boost::filesystem::current_path(p);
       std::cout << "cwd=" << boost::filesystem::current_path() << std::endl;

       std::vector<std::string> linux_osinfo=Zweistein::RunCmdline("cat /etc/os-release");
        Zweistein::Linux_Flavour flavour;
        for (auto& a : linux_osinfo) {
            std::vector<std::string> kv;
            boost::split(kv, a, boost::is_any_of("="));
            if (kv[0] == "ID_LIKE") {
                if (kv[1] == "debian") flavour = Zweistein::debian;
                if (kv[1] == "fedora") flavour = Zweistein::redhat;
                if (kv[1] == "rhel fedora") flavour = Zweistein::redhat;
                if (kv[1] == "rhel") flavour = Zweistein::redhat;
            }
        }
        std::string installcmd = "apt-get";
        if (flavour != Zweistein::debian) installcmd = "yum";
        //"import distutils.sysconfig";
        //"distutils.sysconfig.get_python_lib(plat_specific = False, standard_lib = False)";

        std::string bash_sudo = Zweistein::ResourcePath("bash.sudo.sh", fs).string();

        Zweistein::Version version = Zweistein::GetProgamVersion("gcc");
        if (version.Major < 9) {
            std::string script = Zweistein::ResourcePath("getcompiler.sh",fs).string();
            Zweistein::RunShellScript(script, bash_sudo);

        }

        version = Zweistein::GetProgamVersion("git");
        if (version.Major < 2 || version.Major == 2 && version.Minor < 10) {
            std::string script = Zweistein::ResourcePath("getgit.sh", fs).string();
            Zweistein::RunShellScript(script, bash_sudo);
        }

       version = Zweistein::GetProgamVersion("cmake");
       if (version.Major < 3  || version.Major==3  && version.Minor < 16) {

           std::string script = Zweistein::ResourcePath("getcmake.sh", fs).string();
           if (flavour == Zweistein::Linux_Flavour::redhat) {
               // replace apt-get with yum
               // get centos version

           }
           Zweistein::RunShellScript(script, bash_sudo);
       }

       std::string script = Zweistein::ResourcePath("getopencv.sh", fs).string();
       Zweistein::RunShellScript(script, bash_sudo);

       script = Zweistein::ResourcePath("getboost.sh", fs).string();
       Zweistein::RunShellScript(script, bash_sudo);

       script = Zweistein::ResourcePath("remrepository.sh", fs).string();
       Zweistein::RunShellScript(script, bash_sudo);

       script = Zweistein::ResourcePath("getrepository.sh", fs).string();

       Zweistein::Process::exec(script);

       script = Zweistein::ResourcePath("charming_build.sh", fs).string();
       Zweistein::Process::exec(script);

       script = Zweistein::ResourcePath("charming_install.sh", fs).string();
       Zweistein::RunShellScript(script, bash_sudo);

    }
    catch (boost::exception& e) {
        std::cout << boost::diagnostic_information(e) << std::endl;
        Zweistein::install_log.push_back(boost::diagnostic_information(e));
    }

    boost::filesystem::path p = Zweistein::UserIniDir();
    p.append(PROJECT_NAME + ".log");
    std::ofstream o(p.c_str());
    for (auto& a : Zweistein::install_log) o << a << std::endl;

}
