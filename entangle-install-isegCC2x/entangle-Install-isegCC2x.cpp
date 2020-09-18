// Boost_Installer.cpp : Defines the entry point for the application.
//
#include <cmrc/cmrc.hpp>
#include "Entangle_Install.hpp"

CMRC_DECLARE(resources);
std::string PROJECT_NAME("entangle-install-isegcc2x");

std::vector<std::string> prerequisites = {
    "pip3 install --user asyncio",
    "pip3 install --user aiohttp",
    "pip3 install --user websockets",
    "pip3 install --user toml",

};


int main(int argc, char* argv[])
{
    try {
        auto fs = cmrc::resources::get_filesystem();
        std::cout << "Installs entangle interface for isegCC2x HV Powersupply." << std::endl;
        boost::filesystem::path  devicedir;
        boost::filesystem::path  resdir;
        entangle_setup(devicedir, resdir, prerequisites, argc, argv);
        boost::filesystem::path dest = devicedir;


        dest /= "iseg";
        try {
            boost::filesystem::create_directories(dest);
        }
        catch (std::exception& ex) {
            std::cout << ex.what() << std::endl;
        }

        boost::filesystem::path relpath;

        boost::function<void(std::string)> t = [&fs,&t,&dest,&resdir, &relpath](std::string root) {
            for (auto&& entry : fs.iterate_directory(root)) {
                if (entry.is_directory()) {

                    boost::filesystem::path orig = relpath.string();
                    relpath /= entry.filename();
                    t(relpath.string());
                    relpath = orig.string();

                }
                else {
                    auto file1 = fs.open(relpath.string() + "/" + entry.filename());
                    boost::filesystem::path fname(entry.filename().c_str());

                    boost::filesystem::path p = dest.string();

                    if (boost::iequals(fname.extension().string(), ".res")) {
                        p = resdir.string();
                    }


                    p /= relpath.string();
                    std::cout << p << std::endl;

                    try { boost::filesystem::create_directories(p); }
                    catch (std::exception& ex) {
                        std::cout << ex.what() << std::endl;
                    }

                    p.append(entry.filename());

                    std::ofstream o(p.c_str(), std::ofstream::binary);

                    std::string logmsg = "written: ";
                    if (!o.write(file1.begin(), file1.size())) {
                        logmsg = "ERROR writing: ";
                    }
                    logmsg += p.string();
                    std::cout <<  logmsg << std::endl;
                    Zweistein::install_log.push_back(logmsg);


                }
            }
        };
        t("");
        if (devicedir.empty()) throw  std::runtime_error("Entangle device dir not found.");
    }
    catch (boost::exception& e) {
        std::cout << boost::diagnostic_information(e) << std::endl;
        Zweistein::install_log.push_back(boost::diagnostic_information(e));
        std::cout << "Prerequisite Entangle NOT INSTALLED." << std::endl;
        std::cout << "Please install from: " << std::endl;
        std::cout << "https://forge.frm2.tum.de/entangle/doc/entangle-master/build/" << std::endl;

    }

    boost::filesystem::path p = Zweistein::UserIniDir();
    p.append(PROJECT_NAME + ".log");
    std::ofstream o(p.c_str());
    for (auto& a : Zweistein::install_log) o << a << std::endl;

}
