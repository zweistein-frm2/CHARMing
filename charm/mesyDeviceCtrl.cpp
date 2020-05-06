/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/

#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <list>
#include <string>
#include <filesystem>
#include <clocale>
#include <vector>
#include <boost/locale.hpp>
#include <boost/exception/all.hpp>
#include <boost/system/error_code.hpp>
#include <boost/exception/error_info.hpp>
#include <errno.h>
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include "OptionPrinter.hpp"
#include "CustomOptionDescription.hpp"
#include <boost/program_options.hpp>
#include <boost/foreach.hpp>
#include <boost/throw_exception.hpp>
#include "Zweistein.PrettyBytes.hpp"
#include "Mesytec.listmode.write.hpp"
#include "Mesytec.config.hpp"
#include "Zweistein.populateHistograms.hpp"
#include "Zweistein.displayHistogram.hpp"
#include <opencv2/highgui.hpp>
#include "Zweistein.ThreadPriority.hpp"
#include "Zweistein.Logger.hpp"


#ifdef _DEBUG
boost::log::trivial::severity_level SEVERITY_THRESHOLD = boost::log::trivial::trace;
#else
boost::log::trivial::severity_level SEVERITY_THRESHOLD = boost::log::trivial::info;
#endif
std::string PROJECT_NAME("charm");

using boost::asio::ip::udp;

EXTERN_FUNCDECLTYPE boost::mutex coutGuard;
EXTERN_FUNCDECLTYPE boost::thread_group worker_threads;

extern const char* GIT_REV;
extern const char* GIT_TAG ;
extern const char* GIT_BRANCH;
extern const char* GIT_DATE;

Zweistein::Lock histogramsLock;
std::vector<Histogram> histograms = std::vector<Histogram>(2);

namespace po = boost::program_options;
void conflicting_options(const po::variables_map& vm,
	const char* opt1, const char* opt2)
{
	if (vm.count(opt1) && !vm[opt1].defaulted()
		&& vm.count(opt2) && !vm[opt2].defaulted())
		throw std::logic_error(std::string("Conflicting options '")
			+ opt1 + "' and '" + opt2 + "'.");
}

boost::asio::io_service io_service;

int main(int argc, char* argv[])
{
	char *cLocal=setlocale(LC_ALL, NULL);
	setlocale(LC_ALL, "de-DE");
	boost::filesystem::path::imbue(std::locale());
	std::string appName = boost::filesystem::basename(argv[0]);
	
	boost::shared_ptr < Mesytec::MesytecSystem> ptrmsmtsystem1 = boost::shared_ptr < Mesytec::MesytecSystem>(new Mesytec::MesytecSystem());
	ptrmsmtsystem1->initatomicortime_point();
	
	
	try
	{
		std::cout << PROJECT_NAME << " : BRANCH: " << GIT_BRANCH << " TAG:" << GIT_TAG << " REV: " << GIT_REV <<" "<<GIT_DATE<< std::endl;
		if (argc == 1) {
			std::cout << "--help for usage info" << std::endl;
		}

		boost::filesystem::path inidirectory = Zweistein::Config::GetConfigDirectory();
		Zweistein::Config::inipath = inidirectory;
		const char* HELP="help";
		const char* LISTMODE_FILE="listmodefile(s)";
		const char *CONFIG="config-file";
		const char *WRITE2DISK="writelistmode";
		const char* SETUP = "setup";
		bool inputfromlistfile = false;
		bool write2disk = false;
		bool setupafterconnect = false;
		po::options_description desc("command line options");
		int maxNlistmode = 16;// maximal 16 listmode files
		desc.add_options()
			(HELP, "produces this help message" )
			(LISTMODE_FILE, po::value< std::vector<std::string> >(),(std::string("file1 [file2] ... [file")+std::to_string(maxNlistmode)+std::string("N]")).c_str())
			(CONFIG, po::value< std::string>(), (std::string("alternative config file[.json], must be in ")+ inidirectory.string()).c_str())
			(WRITE2DISK, (std::string("write DataPackets to ")+ Mesytec::writelistmodeFileNameInfo()).c_str())
			(SETUP, (std::string("config mesytec device(s): ")+ std::string("set module IP addr")).c_str())
			;
		po::positional_options_description positionalOptions;
		positionalOptions.add(LISTMODE_FILE, maxNlistmode); 
		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).
			options(desc).positional(positionalOptions).run(), vm);
		if (vm.count(HELP)) {
			std::cout << "Options for Charm or Mesytec Detector systems instrument control" << std::endl;
			std::cout<<"connects to mesytec/charm hardware (or simulator=>PacketSender) and starts data acquisition"<< std::endl;
		    boost::filesystem::path preferred=Zweistein::Config::PreferredDirectory();
			preferred /= PROJECT_NAME;
			preferred.append(PROJECT_NAME + ".json");
			std::cout << "according to [preferred settings] in "<<preferred.string() << std::endl;
			boost::filesystem::path current = inidirectory;
			current/= PROJECT_NAME;
			current.append(PROJECT_NAME + ".json");
			std::cout << "[current settings] : " << current.string() << std::endl;
			rad::OptionPrinter::printStandardAppDesc(appName,
				std::cout,
				desc,
				&positionalOptions);
			return 0;
		}

		conflicting_options(vm, LISTMODE_FILE, WRITE2DISK);
		conflicting_options(vm, LISTMODE_FILE, SETUP);
		conflicting_options(vm, WRITE2DISK, SETUP);
		po::notify(vm);
		if (vm.count(WRITE2DISK)) write2disk = true;
		if (vm.count(SETUP)) setupafterconnect = true;
		if (vm.count(CONFIG)) {
			std::string a=vm[CONFIG].as<std::string>();
			boost::filesystem::path p(a);
			if(!p.has_extension()) p.replace_extension(".json");
			Zweistein::Config::inipath.append(p.string());
		}
		else {
			Zweistein::Config::inipath.append(appName + ".json");
		}
		if (!vm.count(LISTMODE_FILE)) LOG_INFO << "Using config file:" << Zweistein::Config::inipath << " " <<std::endl ;
		std::vector<std::string> listmodeinputfiles = std::vector<std::string>();
		
		if (vm.count(LISTMODE_FILE))
		{
			inputfromlistfile = true;
			std::cout << "listmode files are: ";
			for (auto i : vm[LISTMODE_FILE].as< std::vector<std::string> >()) {
				std::cout << i << " ";
				boost::filesystem::path p = boost::filesystem::current_path();// boost::dll::program_location().remove_filename();
				boost::filesystem::path p_argv(i);
				if (p_argv.is_relative()) {
						p_argv = p.append(i);
					}
				listmodeinputfiles.push_back(p_argv.string());
			}
			std::cout << std::endl;
		}
		
		boost::asio::signal_set signals(io_service, SIGINT, SIGTERM);
		signals.async_wait(boost::bind(&boost::asio::io_service::stop, &io_service));
			
		std::list<Mcpd8::Parameters> _devlist = std::list<Mcpd8::Parameters>();

		boost::function<void()> t;
		if (inputfromlistfile) {
			ptrmsmtsystem1->inputFromListmodeFile = true;
			Mesytec::listmode::whatnext = Mesytec::listmode::continue_reading; // start immediately
			t = [ &ptrmsmtsystem1, &_devlist, &listmodeinputfiles]() {
				try {

					for (std::string& fname : listmodeinputfiles) {
						try { boost::property_tree::read_json(fname + ".json", Mesytec::Config::root); }
						catch (std::exception& e) {
							LOG_ERROR << e.what() << " for reading." << std::endl;
							continue;
						}
						LOG_INFO << "Config file : " << fname + ".json" << std::endl;
						_devlist.clear();
						bool configok = Mesytec::Config::get(_devlist, "");
						std::stringstream ss_1;
						boost::property_tree::write_json(ss_1, Mesytec::Config::root);
						LOG_INFO << ss_1.str() << std::endl;

						if (!configok)	return ;

						ptrmsmtsystem1->listmode_connect(_devlist, io_service);
						// find the .json file for the listmode file
						// check if Binning file not empty, if not empty wait for
						
						Zweistein::setupHistograms(io_service,ptrmsmtsystem1,Mesytec::Config::BINNINGFILE.string());

					
						auto abfunc = boost::bind(&Mesytec::MesytecSystem::analyzebuffer, ptrmsmtsystem1, _1);
						auto read = Mesytec::listmode::Read(abfunc, ptrmsmtsystem1->data, ptrmsmtsystem1->deviceparam);
						

						read.file(fname, io_service);

						Zweistein::setup_status = Zweistein::histogram_setup_status::not_done; // next file is all new life 
					}
					
				}
				catch (boost::exception & e) {
					LOG_ERROR << boost::diagnostic_information(e) << std::endl;
				}

			};

		}
		else  {
			try { boost::property_tree::read_json(Zweistein::Config::inipath.string(), Mesytec::Config::root); }
			catch (std::exception& e) {
				LOG_ERROR << e.what() << " for reading." << std::endl;
			}

			bool configok = Mesytec::Config::get(_devlist, inidirectory.string());
			std::stringstream ss_1;
			boost::property_tree::write_json(ss_1, Mesytec::Config::root);
			LOG_INFO << ss_1.str() << std::endl;
			
			if (!configok)	return -1;

						
			Zweistein::Logger::Add_File_Sink(Mesytec::Config::DATAHOME.string() + appName + ".log");
			try {
				//we can write as text file:
				std::string fp = Zweistein::Config::inipath.string();
				std::ofstream outfile;
				outfile.open(fp, std::ios::out | std::ios::trunc);
				outfile << ss_1.str();
				outfile.close();
				
			}
			catch (std::exception& e) { // exception expected, //std::cout << boost::diagnostic_information(e); 
				LOG_ERROR << e.what() << " for writing." << std::endl;
			}
			if (!inputfromlistfile) {
				for (Mcpd8::Parameters& p : _devlist) {
					Zweistein::ping(p.mcpd_ip); // sometime 
				}
			}

			t = [ &ptrmsmtsystem1, &_devlist,&write2disk]() {
				try {
					ptrmsmtsystem1->write2disk=write2disk;
					ptrmsmtsystem1->connect(_devlist, io_service);
					io_service.run();
				}
				catch (Mesytec::cmd_error& x) {
					if (int const* mi = boost::get_error_info<Mesytec::my_info>(x)) {
						auto  my_code = magic_enum::enum_cast<Mesytec::cmd_errorcode>(*mi);
						if (my_code.has_value()) {
							auto c1_name = magic_enum::enum_name(my_code.value());
							LOG_ERROR << c1_name<<std::endl;
						}
						
					}
					
				}
				catch (boost::exception & e) {
					LOG_ERROR << boost::diagnostic_information(e) << std::endl;
					
				}
				
				io_service.stop();
				

			};
		}

		auto pt = new boost::thread(boost::bind(t));

		Zweistein::Thread::CheckSetUdpKernelBufferSize();
		Zweistein::Thread::SetThreadPriority(pt->native_handle(), Zweistein::Thread::PRIORITY::HIGH);

		worker_threads.add_thread(pt);
		boost::this_thread::sleep_for(boost::chrono::milliseconds(2500));// so msmtsystem1 should be connected
		
					
		boost::chrono::system_clock::time_point start = boost::chrono::system_clock::now();
		int i = 0;
		long long lastcount = 0;
		{
			std::cout << std::endl << "Ctrl-C to stop" << std::endl;
		}
		

		if (inputfromlistfile) {
			
			worker_threads.create_thread([&ptrmsmtsystem1] {Zweistein::populateHistograms(io_service, ptrmsmtsystem1); });
			worker_threads.create_thread([&ptrmsmtsystem1] {Zweistein::displayHistogram(io_service, ptrmsmtsystem1); });
			// nothing to do really,
			while (!io_service.stopped()) {
				boost::this_thread::sleep_for(boost::chrono::milliseconds(1500));
				io_service.run_one();
			}
		}
		else {
			// data acquisition monitoring loop
			int k = 0;
			for ( k = 0; k < 10; k++) {
				if (ptrmsmtsystem1->connected) {
					if (setupafterconnect) {
						boost::function<void()> setipaddrcmd = [&ptrmsmtsystem1, &_devlist]() {
							try {
								for (auto& kvp : ptrmsmtsystem1->deviceparam) {

									Mcpd8::CmdPacket  cmdpacket;
									memset(cmdpacket.data, 0, sizeof(unsigned short) * 14);
									std::cout << "MCPD8 : "<< kvp.second.mcpd_endpoint << std::endl;
									
									while (true) {
										std::cout << "enter new mcpd ip addr (0 for no change):";
										std::string ip_str;
										std::cin >> ip_str;
										std::cout << std::endl;
										if (ip_str == "0") break;
										try {
											boost::asio::ip::address_v4 ipaddr = boost::asio::ip::make_address_v4(ip_str);
											ipaddr.to_bytes();
											cmdpacket.data[0] = ipaddr.to_bytes()[0];
											cmdpacket.data[1] = ipaddr.to_bytes()[1];
											cmdpacket.data[2] = ipaddr.to_bytes()[2];
											cmdpacket.data[3] = ipaddr.to_bytes()[3];
											std::cout << "new ip addr: " << ipaddr << std::endl;
											break;
										}
										catch (boost::system::system_error const& e) {
											if (ip_str.empty()) boost::throw_exception(std::runtime_error("aborted by user"));
											
											std::cout << e.what() << ": " << e.code() << " - " << e.code().message() << "\n";
										}
										
									}

									while (true) {
										std::cout << "enter new data sink ip addr (0 for no change, 0.0.0.0 for same as sending pc):";
										std::string ip_str;
										std::cin >> ip_str;
										if (ip_str == "0") break;
										try {
											boost::asio::ip::address_v4 ipaddr = boost::asio::ip::make_address_v4(ip_str);
											ipaddr.to_bytes();
											cmdpacket.data[4] = ipaddr.to_bytes()[0];
											cmdpacket.data[5] = ipaddr.to_bytes()[1];
											cmdpacket.data[6] = ipaddr.to_bytes()[2];
											cmdpacket.data[7] = ipaddr.to_bytes()[3];
											std::cout << "new data sink ip addr: " << ipaddr << std::endl;
											break;
										}
										catch (boost::system::system_error const& e) {
											if (ip_str.empty()) boost::throw_exception(std::runtime_error("aborted by user"));
											std::cout << e.what() << ": " << e.code() << " - " << e.code().message() << "\n";
										}
									}

									while (true) {
										std::cout << "enter new Cmd UDP port (0 for no change)):";
										unsigned short udpport = -1;
										std::cin >> udpport;
										if (udpport == 0) break;
										try{
											cmdpacket.data[8] = udpport;
											std::cout << "new cmd udp port: " << udpport << std::endl;
											break;
										}
										catch (boost::system::system_error const& e) {
											if (udpport == -1) boost::throw_exception(std::runtime_error("aborted by user"));
											std::cout << e.what() << ": " << e.code() << " - " << e.code().message() << "\n";
										}
										
									}

									while (true) {
										std::cout << "enter new Data UDP port (0 for no change)):";
										unsigned short udpport = -1;
										std::cin >> udpport;
										if (udpport == 0) break;
										try {
											cmdpacket.data[9] = udpport;
											std::cout << "new data udp port: " << udpport << std::endl;
											break;
										}
										catch (boost::system::system_error const& e) {
											if (udpport == -1) boost::throw_exception(std::runtime_error("aborted by user"));
											std::cout << e.what() << ": " << e.code() << " - " << e.code().message() << "\n";
										}
										
									}

									while (true) {
										std::cout << "enter new cmd pc ip addr (0 for no change, 0.0.0.0 for same as sending pc):";
										std::string ip_str;
										std::cin >> ip_str;
										if (ip_str == "0") break;
										try {
											boost::asio::ip::address_v4 ipaddr = boost::asio::ip::make_address_v4(ip_str);
											ipaddr.to_bytes();
											cmdpacket.data[10] = ipaddr.to_bytes()[0];
											cmdpacket.data[11] = ipaddr.to_bytes()[1];
											cmdpacket.data[12] = ipaddr.to_bytes()[2];
											cmdpacket.data[13] = ipaddr.to_bytes()[3];
											std::cout << "new data sink ip addr: " << ipaddr << std::endl;
											break;
										}
										catch (boost::system::system_error const& e) {
											if (ip_str.empty()) boost::throw_exception(std::runtime_error("aborted by user"));
											std::cout << e.what() << ": " << e.code() << " - " << e.code().message() << "\n";
										}
										
									}
									
									cmdpacket.cmd = Mcpd8::Cmd::SETPROTOCOL;
									cmdpacket.Length = Mcpd8::CmdPacket::defaultLength + 14;
									Mcpd8::CmdPacket::Send(kvp.second.socket,cmdpacket,kvp.second.mcpd_endpoint);

									std::cout << "OK => parameters written correctly." << std::endl;
	
								}
							}
							catch (Mesytec::cmd_error& x) {
								
								if (int const* mi = boost::get_error_info<Mesytec::my_info>(x)) {
									auto  my_code = magic_enum::enum_cast<Mesytec::cmd_errorcode>(*mi);
									if (my_code.has_value()) {
										auto c1_name = magic_enum::enum_name(my_code.value());
										LOG_ERROR << c1_name << std::endl;
									}

								}

							}

							catch (boost::exception& e) {
									LOG_ERROR << boost::diagnostic_information(e) << std::endl;

							}
							io_service.stop();
						};
						worker_threads.create_thread(boost::bind(setipaddrcmd));
						boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
						break;
					}
					else {
						Zweistein::setupHistograms(io_service, ptrmsmtsystem1, Mesytec::Config::BINNINGFILE.string());
						worker_threads.create_thread([&ptrmsmtsystem1] {Zweistein::populateHistograms(io_service, ptrmsmtsystem1); });
						worker_threads.create_thread([&ptrmsmtsystem1] {Zweistein::displayHistogram(io_service, ptrmsmtsystem1); });
						boost::function<void()> sendstartcmd = [&ptrmsmtsystem1, &_devlist, &write2disk]() {
							unsigned long rate = 1850;
							try {
								if (write2disk) worker_threads.create_thread([&ptrmsmtsystem1] {Mesytec::writeListmode(io_service, ptrmsmtsystem1); });
								ptrmsmtsystem1->SendAll(Mcpd8::Cmd::START);
								for (auto& kvp : ptrmsmtsystem1->deviceparam) {
									if (kvp.second.datagenerator == Mesytec::DataGenerator::NucleoSimulator) {
										ptrmsmtsystem1->Send(kvp, Mcpd8::Internal_Cmd::SETNUCLEORATEEVENTSPERSECOND, rate);//1650000 is maximum
									}
									if (kvp.second.datagenerator == Mesytec::DataGenerator::CharmSimulator) {
										ptrmsmtsystem1->Send(kvp, Mcpd8::Internal_Cmd::CHARMSETEVENTRATE, rate); // oder was du willst
										ptrmsmtsystem1->Send(kvp, Mcpd8::Internal_Cmd::CHARMPATTERNGENERATOR, 1); // oder was du willst
									}
								}
							}
							catch (Mesytec::cmd_error& x) {
									if (int const* mi = boost::get_error_info<Mesytec::my_info>(x)) {
									auto  my_code = magic_enum::enum_cast<Mesytec::cmd_errorcode>(*mi);
									if (my_code.has_value()) {
										auto c1_name = magic_enum::enum_name(my_code.value());
										LOG_ERROR << c1_name << std::endl;
									}

								}

							}

							catch (boost::exception& e) {
								boost::mutex::scoped_lock lock(coutGuard);
								LOG_ERROR << boost::diagnostic_information(e) << std::endl;

							}
						};
						worker_threads.create_thread(boost::bind(sendstartcmd));
						boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
						break;

					}
				}
					boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
				}
				if (k == 10) {
					LOG_ERROR << " NOT CONNECTED (tried " << k << " seconds)" << std::endl;
				}
			while (!io_service.stopped()) {
					boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
					if (setupafterconnect) continue;
					boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - start;
					start = boost::chrono::system_clock::now();
					long long currentcount = ptrmsmtsystem1->data.evntcount;
					double evtspersecond = sec.count() != 0 ? (double)(currentcount - lastcount) / sec.count() : 0;
					{
						boost::mutex::scoped_lock lock(coutGuard);
						boost::chrono::system_clock::time_point tps(ptrmsmtsystem1->started);
						boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - tps;
						std::cout << "\r" << std::setprecision(0) << std::fixed << evtspersecond << " Events/s, (" << Zweistein::PrettyBytes((size_t)(evtspersecond * sizeof(Mesy::Mpsd8Event))) << "/s)\t" << Mcpd8::DataPacket::deviceStatus(ptrmsmtsystem1->data.last_deviceStatusdeviceId) << " elapsed:" << sec << " ";// << std::endl;
						lastcount = currentcount;
#ifndef _WIN32
						std::cout << std::flush;
#endif
					}
					io_service.run_one();
				};
			}

		
	}
	catch (boost::exception & e) {
		LOG_ERROR<< boost::diagnostic_information(e) << std::endl;
	}
	

	if (ptrmsmtsystem1) {
		try {ptrmsmtsystem1->SendAll(Mcpd8::Cmd::STOP_UNCHECKED);}
		catch (boost::exception& e) {
			LOG_ERROR << boost::diagnostic_information(e) << std::endl;
		}
	//	delete ptrmsmtsystem1;
	//	ptrmsmtsystem1 = nullptr;
	}

	worker_threads.join_all();
	LOG_DEBUG << "main() exiting..." << std::endl;
	
	return 0;
}
