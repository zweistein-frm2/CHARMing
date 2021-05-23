// Boost_Installer.cpp : Defines the entry point for the application.
//
#include <cmrc/cmrc.hpp>
#include "Entangle_Install.hpp"
#include <boost/algorithm/string/predicate.hpp>
#include "gzip/decompress.hpp"
CMRC_DECLARE(resources);
std::string PROJECT_NAME("entangle-install-charming");


std::vector<std::string> prerequisites = {
    "pip3 install numpy --upgrade",
    "pip3 install scikit-build",
    "pip3 install opencv-python"
};



int main(int argc, char* argv[])
{
	try {


        auto fs = cmrc::resources::get_filesystem();
        std::cout << "Installs entangle interface for CHARMing Neutron detector software" << std::endl;
        boost::filesystem::path  devicedir;
        boost::filesystem::path  resdir;
        entangle_setup( devicedir,resdir,prerequisites, argc, argv);

       std::string fsresdir = "rcfiles";
       // exec: pip3 install --user scikit-build
      // exec: pip3 install --user opencv-python
       std::string lastdir="";

       for (auto&& entry : fs.iterate_directory(fsresdir)) {
           boost::filesystem::path fname(entry.filename().c_str());
           auto file1 = fs.open(fsresdir+"/"+entry.filename());
           boost::filesystem::path dest = devicedir;
           dest/= "charming";
           if (boost::iequals(fname.extension().string(), ".res")) {
               dest = resdir;
           }
           try {
			   dest = dest.generic_path();
               boost::filesystem::create_directories(dest);
               if (!boost::equals(dest.string(), lastdir)) {
                   std::cout << "[" << dest << "]" << std::endl;
                   lastdir = dest.string();
               }
           }
           catch (std::exception& ex) {
               std::cout << ex.what() << std::endl;
           }

           std::cout << entry.filename() << '\n';

           dest.append(entry.filename());
		   dest = dest.generic_path();
           std::string tmp = dest.string();
           if (boost::algorithm::ends_with(tmp, ".gz")) {

	           std::cout << tmp << " : compressed length: " << file1.size() << std::endl;

               try {
                   std::string decompressed_data = gzip::decompress(file1.begin(), file1.size());

                   std::cout << " : decompressed length: " << decompressed_data.length() << std::endl;
                   tmp = tmp.substr(0, tmp.size() - 3);
                   std::ofstream o(tmp.c_str(), std::ofstream::binary);
                   o.write(decompressed_data.c_str(), decompressed_data.length());
               }
               catch (std::exception& ex) {
                   std::cout << ex.what() << std::endl;
               }
           }

           else {
               std::ofstream o(dest.c_str(), std::ofstream::binary);
               o.write(file1.begin(), file1.size());

           }
       }
       if (devicedir.empty()) throw  std::runtime_error("Entangle device dir not found.");
    }
    catch (boost::exception& e) {
        std::cout << boost::diagnostic_information(e) << std::endl;
        Zweistein::install_log.push_back(boost::diagnostic_information(e));
		std::cout << "Prerequisite Entangle NOT INSTALLED." <<std::endl;
		std::cout << "Please install from: "<<std::endl;
		std::cout << "https://forge.frm2.tum.de/entangle/doc/entangle-master/build/"<<std::endl;

    }

    boost::filesystem::path p = Zweistein::UserIniDir();
    p.append(PROJECT_NAME + ".log");
    std::ofstream o(p.c_str());
    for (auto& a : Zweistein::install_log) o << a << std::endl;

}
