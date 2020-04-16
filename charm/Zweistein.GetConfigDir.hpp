#pragma once
#include <boost/atomic.hpp>
#include <boost/filesystem.hpp>
#include <list>
#include <string>
extern std::string PROJECT_NAME;
namespace Zweistein {

	
	namespace Config {
		boost::filesystem::path inipath;
		boost::filesystem::path GetConfigDirectory() {
			
			boost::filesystem::path  r(boost::filesystem::current_path());
		
			r= r.root_path();
			r /= "etc";
			bool use_etcdir = true;
			boost::system::error_code ec;
			if (!boost::filesystem::exists(r)) {
				use_etcdir = boost::filesystem::create_directory(r, ec);
			}

			if (ec) LOG_WARNING << ec.message() << ": " << r.string() << std::endl;
			else {

				r /= PROJECT_NAME;// +"-frm2";

				if (!boost::filesystem::exists(r)) {
					use_etcdir = boost::filesystem::create_directory(r, ec);
				}
				if (ec) LOG_WARNING << ec.message() << ": " << r.string()<<std::endl;
			}
			if (ec) use_etcdir = false;

			boost::filesystem::path inidirectory;
			if (!use_etcdir) {
				boost::filesystem::path homepath = Zweistein::GetHomePath();
				inidirectory = homepath;
				inidirectory /= "." + std::string(PROJECT_NAME);
				if (!boost::filesystem::exists(inidirectory)) {
					boost::filesystem::create_directory(inidirectory);
				}
				inidirectory += boost::filesystem::path::preferred_separator;
			}
			else inidirectory = r;

			return inidirectory;




		}


	}




}