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
#include <boost/function.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include "OptionPrinter.hpp"
#include "CustomOptionDescription.hpp"
#include <boost/program_options.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "Mesytec.Mcpd8.hpp"
#include "Zweistein.PrettyBytes.hpp"
#include "Zweistein.HomePath.hpp"
#include "Zweistein.GetLocalInterfaces.hpp"
#include "Mesytec.listmode.write.hpp"
#include "Mcpd8.Parameters.hpp"
#include "Mesytec.config.hpp"
#include "Mesytec.enums.Generator.hpp"
#include "Zweistein.Histogram.hpp"
#include <opencv2/highgui.hpp>

std::string PROJECT_NAME("charm");

using boost::asio::ip::udp;

boost::mutex coutGuard;
boost::thread_group worker_threads;

namespace po = boost::program_options;
void conflicting_options(const po::variables_map& vm,
	const char* opt1, const char* opt2)
{
	if (vm.count(opt1) && !vm[opt1].defaulted()
		&& vm.count(opt2) && !vm[opt2].defaulted())
		throw std::logic_error(std::string("Conflicting options '")
			+ opt1 + "' and '" + opt2 + "'.");
}


int main(int argc, char* argv[])
{
	char *cLocal=setlocale(LC_ALL, NULL);
	setlocale(LC_ALL, "de-DE");
	boost::filesystem::path::imbue(std::locale());
	std::string appName = boost::filesystem::basename(argv[0]);
	boost::asio::io_service io_service;
	auto ptrmsmtsystem1 = new Mesytec::MesytecSystem();
	try
	{
		if (argc == 1) {
			std::cout << "--help for usage info" << std::endl;
		}
		boost::filesystem::path homepath= Zweistein::GetHomePath();
		boost::filesystem::path inidirectory = homepath;
		inidirectory /= "." + std::string(PROJECT_NAME);
		if (!boost::filesystem::exists(inidirectory)) {
			boost::filesystem::create_directory(inidirectory);
		}
		homepath += boost::filesystem::path::preferred_separator;
		boost::filesystem::path inipath = inidirectory;
		const char* HELP="help";
		const char* LISTMODE_FILE="listmodefile(s)";
		const char *CONFIG="config";
		const char *WRITE2DISK="writelistmode";
		bool inputfromlistfile = false;
		bool write2disk = false;
		po::options_description desc("command line options");
		int maxNlistmode = 16;// maximal 16 listmode files
		desc.add_options()
			(HELP, "produce help message")
			(LISTMODE_FILE, po::value< std::vector<std::string> >(),(std::string("file1 [file2] ... [file")+std::to_string(maxNlistmode)+std::string("N]")).c_str())
			(CONFIG, po::value< std::string>(), (std::string("alternative config file[.json], must be in ")+ inidirectory.string()).c_str())
			(WRITE2DISK, (std::string("write DataPackets to ")+ Mesytec::writelistmodeFileNameInfo()).c_str())
			;
		po::positional_options_description positionalOptions;
		positionalOptions.add(LISTMODE_FILE, maxNlistmode); 
		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).
			options(desc).positional(positionalOptions).run(), vm);
		if (vm.count(HELP)) {
			std::cout << "Options for Charm or Mesytec Detector systems instrument control" << std::endl << std::endl;
			rad::OptionPrinter::printStandardAppDesc(appName,
				std::cout,
				desc,
				&positionalOptions);
			return 0;
		}
		conflicting_options(vm, LISTMODE_FILE, WRITE2DISK);
		po::notify(vm);
		if (vm.count(WRITE2DISK)) write2disk = true;
		if (vm.count(CONFIG)) {
			std::string a=vm[CONFIG].as<std::string>();
			boost::filesystem::path p(a);
			if(!p.has_extension()) p.replace_extension(".json");
			inipath.append(p.string());
		}
		else {
			inipath.append(appName + ".json");
		}
		std::cout << "Using config file:" << inipath << " " << std::endl;
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
		boost::property_tree::ptree root;
		try {boost::property_tree::read_json(inipath.string(), root);}
		catch(std::exception){}
		std::list<Mcpd8::Parameters> _devlist = std::list<Mcpd8::Parameters>();
		
		ptrmsmtsystem1->data.Format = Mcpd8::EventDataFormat::Mpsd8;
		boost::function<void()> t;
		if (inputfromlistfile) {
			ptrmsmtsystem1->inputFromListmodeFile = true;
			t = [&io_service, &ptrmsmtsystem1, &listmodeinputfiles]() {
				try {
					auto abfunc=boost::bind(&Mesytec::MesytecSystem::analyzebuffer, ptrmsmtsystem1,_1);
					auto read=Mesytec::listmode::Read(abfunc,ptrmsmtsystem1->data, ptrmsmtsystem1->deviceparam);
					ptrmsmtsystem1->connected = true;
					read.files(listmodeinputfiles, io_service);
					
				}
				catch (boost::exception & e) {
					boost::mutex::scoped_lock lock(coutGuard);
					std::cout << boost::diagnostic_information(e);
				}

			};

		}
		else  {
		    try {
				Mesytec::Config::get(root, _devlist,io_service);
				
			}
			catch (boost::exception &) { // exception expected, //std::cout << boost::diagnostic_information(e); 
			}
			boost::property_tree::write_json(inipath.string(), root);
			boost::property_tree::write_json(std::cout, root);
			t = [&io_service, &ptrmsmtsystem1, &_devlist]() {
				try {
					ptrmsmtsystem1->connect(_devlist, io_service);
					io_service.run();
				}
				catch (Mesytec::cmd_error& x) {
					boost::mutex::scoped_lock lock(coutGuard);
					if (int const* mi = boost::get_error_info<Mesytec::my_info>(x)) {
						auto  my_code = magic_enum::enum_cast<Mesytec::cmd_errorcode>(*mi);
						if (my_code.has_value()) {
							auto c1_name = magic_enum::enum_name(my_code.value());
							std::cout << c1_name<<std::endl;
						}
						
					}
					
				}
				catch (boost::exception & e) {
					boost::mutex::scoped_lock lock(coutGuard);
					std::cout << boost::diagnostic_information(e);
					
				}
				io_service.stop();

			};
		}

		worker_threads.create_thread(boost::bind(t));
		boost::this_thread::sleep_for(boost::chrono::milliseconds(500));// so msmtsystem1 should be connected

		if(write2disk) worker_threads.create_thread([&io_service, &ptrmsmtsystem1] {Mesytec::writeListmode(io_service,*ptrmsmtsystem1); });
		
					
		boost::chrono::system_clock::time_point start = boost::chrono::system_clock::now();
		int i = 0;
		long long lastcount = 0;
		{
			boost::mutex::scoped_lock lock(coutGuard);
			std::cout << std::endl << "Ctrl-C to stop" << std::endl;
		}
		

		if (inputfromlistfile) {
			worker_threads.create_thread([&io_service, &ptrmsmtsystem1] {Zweistein::populateHistogram(io_service, *ptrmsmtsystem1); });
			// nothing to do really,
			while (!io_service.stopped()) {
				boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
				io_service.run_one();
			}
		}
		else {
			// data acquisition monitoring loop

			for (int i = 0; i < 10; i++) {
				if (ptrmsmtsystem1->connected) {
					worker_threads.create_thread([&io_service, &ptrmsmtsystem1] {Zweistein::populateHistogram(io_service, *ptrmsmtsystem1); });
					boost::function<void()> sendstartcmd = [&io_service, &ptrmsmtsystem1, &_devlist]() {
						unsigned long rate = 1850000;
						try {
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
						catch (boost::exception& e) {
							boost::mutex::scoped_lock lock(coutGuard);
							std::cout << boost::diagnostic_information(e);
							
						}
					};
					worker_threads.create_thread(boost::bind(sendstartcmd));
					break;

				}
				boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
			
			}
			std::cout << std::endl;
			
			while (!io_service.stopped()) {
				boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
				boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - start;
				start = boost::chrono::system_clock::now();
				long long currentcount = ptrmsmtsystem1->data.evntcount;
				double evtspersecond = sec.count() != 0 ? (double)(currentcount - lastcount) / sec.count() : 0;
				{
					boost::mutex::scoped_lock lock(coutGuard);
					std::cout << "\r" <<std::setprecision(1) <<std::fixed<< evtspersecond << " Events/s, (" << Zweistein::PrettyBytes((size_t)(evtspersecond * sizeof(Mesy::Mpsd8Event))) << "/s)\t";// << std::endl;
					lastcount = currentcount;
					size_t inqueue = ptrmsmtsystem1->data.evntqueue.read_available();
					if (inqueue > 0)	std::cout << inqueue << " Events in queue" << std::endl;
#ifndef _WIN32
					std::cout << std::flush;
#endif
				}
				io_service.run_one();
			};
		}
	}
	catch (boost::exception & e) {
		boost::mutex::scoped_lock lock(coutGuard);
		std::cout<< boost::diagnostic_information(e);
	}
	if (ptrmsmtsystem1) {
		ptrmsmtsystem1->SendAll(Mcpd8::Cmd::STOP);
		delete ptrmsmtsystem1;
	}

	boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
	{
		boost::mutex::scoped_lock lock(coutGuard);
		std::cout << "main() exiting..." << std::endl;
	}
	return 0;
}
