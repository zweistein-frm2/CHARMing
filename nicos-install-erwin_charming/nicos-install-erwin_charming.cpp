// Boost_Installer.cpp : Defines the entry point for the application.
//
#include <cmrc/cmrc.hpp>
#include "Nicos_Install.hpp"
#include <boost/filesystem.hpp>
CMRC_DECLARE(resources);
std::string PROJECT_NAME("nicos-install-erwin_charming");

std::vector<std::string> prerequisites = {
	"pip3 install numpy --upgrade",
    "pip3 install scikit-build",
    "pip3 install opencv-python",

};


int main(int argc, char* argv[])
{
    try {
        auto fs = cmrc::resources::get_filesystem();
        std::cout << "Installs charming support for Nicos." << std::endl;
        std::cout << "charming supports Mesytec and Charm detector hardware" << std::endl;
        std::cout << "[nicos_root] [-y]" << std::endl;
        boost::filesystem::path  nicosroot;
        nicos_setup(nicosroot, prerequisites, argc, argv);
        boost::filesystem::path dest = nicosroot;

        dest /= "nicos_mlz";

        try {
            boost::filesystem::create_directories(dest);
        }
        catch (std::exception& ex) {
            std::cout << ex.what() << std::endl;
        }

#ifndef WIN32
        std::string script_path("/usr/local/bin/nicos-erwin_charming");
        std::ofstream script(script_path);
        if (script.is_open()) {
            using namespace boost::filesystem;
            script << "#!/bin/bash" << std::endl;
            script << "python3 " << dest.string() + "/erwin_charming/erwin-loader.py" << std::endl;
            script.close();
            permissions(script_path, add_perms | owner_exe| group_exe | others_exe);
            std::cout << "exec script created :" << script_path << std::endl;
        }


#endif


        boost::filesystem::path relpath;

        boost::function<void(std::string)> t = [&fs,&t,&dest, &relpath](std::string root) {

            for (auto&& entry : fs.iterate_directory(root)) {
                if (entry.is_directory()) {

                    boost::filesystem::path orig = relpath.string();
                    relpath /= entry.filename();
                    relpath = relpath.generic_path();
                    t(relpath.string());
                    relpath = orig;

                }
                else {
                    auto file1 = fs.open(relpath.string() + "/" + entry.filename());
                    boost::filesystem::path fname(entry.filename().c_str());
                    fname = fname.generic_path();
                    boost::filesystem::path p = dest;




                    p /= relpath.string();
                    p = p.generic_path();
                    std::cout << p << std::endl;

                    try { boost::filesystem::create_directories(p); }
                    catch (std::exception& ex) {
                        std::cout << ex.what() << std::endl;
                    }

                    p.append(entry.filename());
                    p = p.generic_path();
                    std::ofstream o(p.c_str(), std::ofstream::binary);

                    std::string logmsg = "written: ";
                    if (!o.write(file1.begin(), file1.size())) {
                        logmsg = "ERROR writing: ";
                    }
#ifndef WIN32
                    if (boost::iequals(p.extension().string(), ".sh")) {
                        std::string cmdline = "chmod +x " + p.string();
                        Zweistein::RunCmdline(cmdline);
                        Zweistein::RunCmdline(p.string());
                    }
#endif
                    logmsg += p.string();
                    std::cout <<  logmsg << std::endl;
                    Zweistein::install_log.push_back(logmsg);


                }
            }
        };
        t("");
        if (nicosroot.empty()) throw  std::runtime_error("Nicos root dir not found.");
        std::cout << "please edit the following files to reflect real tango_base :" << std::endl;
        std::cout << dest.string() + "/erwin_charming/setups/charm.py" << std::endl;
        std::cout << dest.string() + "/erwin_charming/setups/detector.py" << std::endl;
        std::cout << dest.string() + "/erwin_charming/setups/listmode.py" << std::endl;



    }
    catch (boost::exception& e) {
        std::cout << boost::diagnostic_information(e) << std::endl;
        Zweistein::install_log.push_back(boost::diagnostic_information(e));
        std::cout << "Prerequisite Nicos NOT INSTALLED." << std::endl;
        std::cout << "Please install from: " << std::endl;
        std::cout << "https://nicos-controls.org/" << std::endl;

    }

    boost::filesystem::path p = Zweistein::UserIniDir();
    p.append(PROJECT_NAME + ".log");
    std::ofstream o(p.c_str());
    for (auto& a : Zweistein::install_log) o << a << std::endl;

}
