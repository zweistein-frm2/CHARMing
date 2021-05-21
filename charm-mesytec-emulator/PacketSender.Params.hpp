#pragma once
#include <boost/exception/all.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/filesystem.hpp>
#include "Zweistein.HomePath.hpp"
class PacketSenderParams {
    static boost::property_tree::ptree root;
    static std::string fullpathfilename;
    const static std::string mesytecdevice;
    const static std::string devid;
    const static std::string n_charm_units;
    const static std::string punkt;
    const static std::string port;
public:
    static void ReadIni(std::string appName, std::string projectname) {
        boost::filesystem::path homepath = Zweistein::GetHomePath();
        std::cout << "HOMEPATH=" << homepath << std::endl;
        boost::filesystem::path inidirectory = homepath;
        inidirectory /= "." + projectname;
        if (!boost::filesystem::exists(inidirectory)) {
            boost::filesystem::create_directory(inidirectory);
        }
        homepath += boost::filesystem::path::preferred_separator;
        boost::filesystem::path inipath = inidirectory;
        inipath.append(appName + ".json");
        fullpathfilename = inipath.string();
		std::cout << "ini file : "<< fullpathfilename << std::endl;
        try { 
			boost::property_tree::read_json(fullpathfilename, root);
            unsigned short p = get_port();
			root.put<unsigned short>(mesytecdevice + punkt + port, p);
             
			unsigned char n = get_n_charm_units();
			root.put<unsigned char>(mesytecdevice + punkt + n_charm_units, n);
				
			setDevId(getDevId());
			
		}
        catch (std::exception& e) {
			std::cout << e.what() << std::endl;
			try {
			 std::cout << "create file: "<< fullpathfilename << std::endl;
			 unsigned short p = get_port();
			 root.put<unsigned short>(mesytecdevice + punkt + port, p);
             
			 unsigned char n = get_n_charm_units();
			 root.put<unsigned char>(mesytecdevice + punkt + n_charm_units, n);
			 
			 setDevId(getDevId());
			 
			}
			catch(std::exception& e){
				std::cout << e.what() << std::endl;
			}
			
			
		}
    }
    static unsigned char getDevId() {
        return root.get<unsigned char>(mesytecdevice + punkt + devid, 0);

    }
    static unsigned short get_port() {
        return root.get<unsigned short>(mesytecdevice + punkt + port, 54321);

    }
    static unsigned char get_n_charm_units() {
        return root.get<unsigned char>(mesytecdevice + punkt + n_charm_units, 2);

    }
    static void setDevId(unsigned char newdevid) {
        root.put<unsigned char>(mesytecdevice + punkt + devid, newdevid);
        boost::property_tree::write_json(fullpathfilename, root);
    }
};
const std::string PacketSenderParams::n_charm_units = "n_charm_units";
const std::string PacketSenderParams::devid = "devid";
const std::string PacketSenderParams::port = "port";
const std::string PacketSenderParams::punkt = ".";
const std::string PacketSenderParams::mesytecdevice = "MesytecDevice";
boost::property_tree::ptree PacketSenderParams::root;
std::string PacketSenderParams::fullpathfilename = "";
