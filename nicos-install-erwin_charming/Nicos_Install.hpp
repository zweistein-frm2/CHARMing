#define BOOST_PROCESS_WINDOWS_USE_NAMED_PIPE
#include <boost/process.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/exception/all.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "Zweistein.HomePath.hpp"
#include <fstream>
#include <exception>
#include <iostream>
#include <string_view>
#include <boost/algorithm/string.hpp>
#include <regex>
#include <boost/algorithm/string/trim.hpp>
#include <boost/regex.hpp>
#include  "toml11/toml.hpp"

//#define DEBUG

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


namespace bp = boost::process;
namespace Zweistein {

    struct Version {
        Version() :Major(0), Minor(0), patch("") {};
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
#if (defined(DEBUG) || defined(_WIN32))
#if defined(DEBUG)
            std::cout << "Debug build:" << std::endl;
#endif
            boost::filesystem::path homepath = Zweistein::GetHomePath();
            inidirectory = homepath;
            inidirectory /= ".CHARMing";
#else
            inidirectory = "/etc/CHARMing";
            //inidirectory = homepath;
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
                    std::cout << line << std::endl;
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
    std::vector<std::string>  RunCmdline(std::string cmdline, std::string expectedlinestart = "", std::string to_stdin = "") {


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

            for (; std::getline(ierr, line);) {
                boost::erase_all(line, "\r");
                std::cerr << line << std::endl;
                if (!errpushed) {
                    errpushed = true;
                    data.push_back("stderr");
                }
                data.push_back(line);
            }



            for (; std::getline(is, line);) {
                boost::erase_all(line, "\r");
                if(!line.empty()) data.push_back(line);
                std::string_view sv(line);
                if (!expectedlinestart.empty()) {
                    size_t s = sv.find(expectedlinestart);
                    if (s == 0) return data;
                }
            }
        }
        catch (std::exception& e) {
            std::cerr << cmdline << " : " << e.what() << std::endl;
            Zweistein::install_log.push_back(e.what());
        }
        for (auto& a : data) Zweistein::install_log.push_back(a);
        return data;
    }
    Zweistein::Version GetProgamVersion(std::string name) {
        Zweistein::Version version;
        std::string cmdline = name + " --version";
        std::vector<std::string> lines = RunCmdline(cmdline, name);
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
    bool RunShellScript(std::string script, std::string fromscript = "") {
        std::string cmdline = "chmod +x " + script;
        std::vector<std::string> result = Zweistein::RunCmdline(cmdline);
        for (auto& a : result) if (a.find("stderr")) return false;

        cmdline = "";

        if (!fromscript.empty()) {
            cmdline += fromscript + " ";
            // we have sudo
        }
        cmdline += script;
        //result=RunCmdline(cmdline);
        Zweistein::Process::exec(cmdline);
        for (auto& a : result) if (a.find("stderr")) return false;
        return true;

    }
    boost::filesystem::path ResourcePath(std::string resource, cmrc::embedded_filesystem& fs) {
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


void nicos_setup(boost::filesystem::path& nicos_root, std::vector<std::string>& prerequisites, int argc, char* argv[]) {

    std::string PYTHON = "python";

#ifndef _WIN32
    int userid = 0;
    userid = geteuid();
    if (userid != 0) {
        std::cout << "You must run with sudo privileges." << std::endl;
#ifndef DEBUG
        exit(-1);
#endif
    }
    PYTHON = "python3";
#endif

     bool b_yes = false;
    int iyes = -1;
    for (int j = 1; j < argc; j++) {
        if (boost::equals(argv[j], "-y")) {
            b_yes = true;
            iyes = j;
        }
        else {
            nicos_root = argv[j];
        }
    }

    if (argc > (b_yes?3:2)) {
        std::cout << "too many arguments." << std::endl;
        exit(-1);
    }




    std::string pythoncmd = R"(import sys;from distutils.sysconfig import get_python_lib;print(get_python_lib());print('\n'.join(sys.path));)";
    std::string cmdline = PYTHON + " -c " + "\"" + pythoncmd + "\"";
    std::vector<std::string> pythonsyspath = Zweistein::RunCmdline(cmdline);
    if (!nicos_root.empty()) {
        pythonsyspath.insert(pythonsyspath.begin(),nicos_root.string());
    }
#ifndef WIN32
    pythonsyspath.insert(pythonsyspath.begin(), "/opt/nicos");
    pythonsyspath.insert(pythonsyspath.begin(), "/control");
#endif
    int ipath = 0;
    bool dobreak = false;
    // remember : with syspath.py we loaded python library path first and syspath(s) second
    for (auto& psyspath : pythonsyspath) {
        std::cout << "Trying " << psyspath << std::endl;
        std::string possibleName[1] = { "nicos"};
        for (int i = 0; i < 1; i++) {
            boost::filesystem::path p(psyspath);
            if (!boost::filesystem::is_directory(p)) continue;
            for (auto& entry : boost::make_iterator_range(boost::filesystem::directory_iterator(p), {})) {
                // std::cout << entry << "\n";
                std::string f = entry.path().filename().string();

                if (boost::algorithm::istarts_with(f, possibleName[i])) {
                    nicos_root = psyspath;
                    boost::filesystem::path conf = nicos_root;
                    conf.append("nicos.conf");
                    if (boost::filesystem::exists(conf) && !boost::filesystem::is_directory(conf)) {
                        std::cout << "Nicos install root found at: " << nicos_root << std::endl;
                        dobreak = true;
                        break;
                    }
                }
                if (dobreak) break;
            }
            if (dobreak) break;
        }
        if (dobreak) break;
    }


    boost::filesystem::path nicosconf = nicos_root;
    nicosconf.append("nicos.conf");

    try {
        auto data = toml::parse(nicosconf.string());
        const auto& rdir = toml::find<std::string>(data, "nicos", "pid_path");
        if (!rdir.empty()) {
            std::cout<< rdir << std::endl;
        }
        try {
            const auto& devdir = toml::find<std::string>(data, "nicos", "logging_path");
            if (!devdir.empty()) {
                std::cout << devdir << std::endl;
            }
        }
        catch(std::exception& ex) {}

    }
    catch (std::exception& ex) {
        std::cout << ex.what() << std::endl;
    }



    std::cout << "Using nicos root : " << nicos_root << std::endl;
    if (!b_yes) {
        std::cout << "Proceed with installation [y/n] ?";
        char c;
        std::cin >> c;
        if (c != 'y') exit(-1);
        std::cout << std::endl;

#ifndef WIN32
        std::string cmdline = "chmod go+rx " + nicos_root.string();
        std::vector<std::string> result = Zweistein::RunCmdline(cmdline);
        for (auto& a : result) if (a.find("stderr")) {
            std::cout << a << std::endl;
        }
#endif


    }



    for (std::string& installcmd : prerequisites) {
        std::vector<std::string> res = Zweistein::RunCmdline(installcmd);
        for (std::string s : res) {
            std::cout << s << std::endl;
        }
    }

}
