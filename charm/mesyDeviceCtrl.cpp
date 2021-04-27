﻿/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License v3 as published  *
 *   by the Free Software Foundation;                                      *
 ***************************************************************************/

#include "stdafx.h"
#include <boost/locale.hpp>
#include <iostream>
#include <fstream>
#include <list>
#include <string>
#include <vector>
#include <boost/exception/all.hpp>
#include <boost/system/error_code.hpp>
#include <boost/exception/error_info.hpp>
#include <errno.h>
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/array.hpp>
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
#include "Zweistein.ThreadPriority.hpp"
#include "Zweistein.Logger.hpp"
#include "Zweistein.Averaging.hpp"
#include "CounterMonitor.hpp"
#include "Charm.System.hpp"
#include "version.h"
#include "Zweistein.deHumanizeNumber.hpp"

#ifdef _DEBUG
boost::log::trivial::severity_level SEVERITY_THRESHOLD = boost::log::trivial::trace;
#else
boost::log::trivial::severity_level SEVERITY_THRESHOLD = boost::log::trivial::info;
#endif
std::string PROJECT_NAME("CHARMing");
std::string CONFIG_FILE("charmsystem"); // or "mesytecsystem"

bool defaulttocharm = true;

using boost::asio::ip::udp;


EXTERN_FUNCDECLTYPE boost::mutex coutGuard;
EXTERN_FUNCDECLTYPE boost::thread_group worker_threads;

Zweistein::Lock histogramsLock;
std::vector<Histogram> histograms;

static_assert(COUNTER_MONITOR_COUNT >= 4, "Mcpd8 Datapacket can use up to 4 counters: params[4][3]");
boost::atomic<unsigned long long> CounterMonitor[COUNTER_MONITOR_COUNT];

namespace po = boost::program_options;
void conflicting_options(const po::variables_map& vm,
	const char* opt1, const char* opt2)
{
	if (vm.count(opt1) && !vm[opt1].defaulted()
		&& vm.count(opt2) && !vm[opt2].defaulted())
		throw std::logic_error(std::string("Conflicting options '")
			+ opt1 + "' and '" + opt2 + "'.");
}

boost::scoped_ptr<boost::asio::io_context> ptr_ctx( new boost::asio::io_context);

int main(int argc, char* argv[])
{

	try {
		histograms = std::vector<Histogram>(2);
		boost::locale::generator gen;
		std::locale loc = gen("de-DE");
		std::locale::global(loc);
		boost::filesystem::path::imbue(loc);
	}
	catch (std::exception &ex) {
		LOG_ERROR << ex.what() << std::endl;
	}
	std::string appName = boost::filesystem::basename(argv[0]);

	boost::shared_ptr <  Charm::CharmSystem> ptrmsmtsystem1 = nullptr;

	unsigned long simuratedefault = 7; // 7 cts/second

	try
	{
		//std::cout << PROJECT_NAME << " : BRANCH: " << GIT_BRANCH << " LATEST TAG:" << GIT_LATEST_TAG << " commits since:"<< GIT_NUMBER_OF_COMMITS_SINCE<<" "<<GIT_DATE<< std::endl;

		std::string git_latest_tag(GIT_LATEST_TAG);
		git_latest_tag.erase(std::remove_if(git_latest_tag.begin(), git_latest_tag.end(), (int(*)(int)) std::isalpha), git_latest_tag.end());
		std::cout << PROJECT_NAME << " : " << git_latest_tag << "." << GIT_NUMBER_OF_COMMITS_SINCE << "." << GIT_REV << "_" << GIT_DATE << std::endl;

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
		const char* MESYTECDEVICE = "mesytecdevice";
		const char* SIMRATE = "sim-rate";
		bool inputfromlistfile = false;
		bool write2disk = false;
		bool setupafterconnect = false;
		bool bmesyteconly = false;
		po::options_description desc("command line options");
		int maxNlistmode = 16;// maximal 16 listmode files

		desc.add_options()
			(HELP, "produces this help message" )
			(LISTMODE_FILE, po::value< std::vector<std::string> >(),(std::string("file1 [file2] ... [file")+std::to_string(maxNlistmode)+std::string("N]")).c_str())
			(CONFIG, po::value< std::string>(), (std::string("alternative config file[.json], must be in ")+ inidirectory.string()).c_str())
			(WRITE2DISK, (std::string("write DataPackets to ")+ Mesytec::writelistmodeFileNameInfo()).c_str())
			(MESYTECDEVICE, (std::string("use mesytec device protocol. ")).c_str())
			(SETUP, (std::string("config mesytec device(s): ")+ std::string("set module IP addr")).c_str())
			(SIMRATE, (std::string("simulator rate [cts/s]")+std::string("(default=") +std::to_string(simuratedefault)+std::string(")")).c_str())
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
			preferred.append(CONFIG_FILE + ".json");
			std::cout << "according to [preferred settings] in "<<preferred.string() << std::endl;
			boost::filesystem::path current = inidirectory;
			current/= PROJECT_NAME;
			current.append(CONFIG_FILE + ".json");
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

		if (defaulttocharm) bmesyteconly = false;
		else bmesyteconly = true;

		if(vm.count(MESYTECDEVICE)) bmesyteconly = true;

		if(bmesyteconly ) CONFIG_FILE = "mesytecsystem";

		if (bmesyteconly) std::cout << "using MESYTEC device protocol" << std::endl;
		else std::cout << "using CHARM device  protocol" << std::endl;
		if (vm.count(WRITE2DISK)) write2disk = true;
		if (vm.count(SETUP)) setupafterconnect = true;
		if (vm.count(CONFIG)) {
			std::string a=vm[CONFIG].as<std::string>();
			boost::filesystem::path p(a);
			if(!p.has_extension()) p.replace_extension(".json");
			Zweistein::Config::inipath.append(p.string());
		}
		else {
			Zweistein::Config::inipath.append(CONFIG_FILE + ".json");
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

		if (vm.count(SIMRATE))
		{
			std::string sr = vm[SIMRATE].as<std::string>();
			simuratedefault = (unsigned long)Zweistein::Dehumanize(sr);
			std::cout << "SIMRATE=" << simuratedefault << std::endl;
		}

		ptrmsmtsystem1 = boost::shared_ptr < Charm::CharmSystem>(new  Charm::CharmSystem());

		if (bmesyteconly)  ptrmsmtsystem1->systype = Zweistein::XYDetector::Systemtype::Mesytec;

		//ptrmsmtsystem1 = boost::shared_ptr < Mesytec::MesytecSystem>(new  Mesytec::MesytecSystem());
		//else ptrmsmtsystem1 = boost::shared_ptr <Charm::CharmSystem>(new Charm::CharmSystem());
		ptrmsmtsystem1->initatomicortime_point();

		boost::asio::signal_set signals(*ptr_ctx, SIGINT, SIGTERM);
		signals.async_wait(boost::bind(&boost::asio::io_service::stop,& *ptr_ctx));

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
						bool configok = Mesytec::Config::get(_devlist, "",false);
						std::stringstream ss_1;
						boost::property_tree::write_json(ss_1, Mesytec::Config::root);
						LOG_INFO << ss_1.str() << std::endl;

						if (!configok)	return ;
						for (Mcpd8::Parameters& p : _devlist) {
							p.mcpd_ip = fname;
						}


						for (Mcpd8::Parameters& p : _devlist) {
							if (!p.n_charm_units) ptrmsmtsystem1->systype = Zweistein::XYDetector::Systemtype::Mesytec;
							// ptrmesay != null means that the listmode file is from a mesytec measurement and not charm
						}
						ptrmsmtsystem1->eventdataformat = Zweistein::Format::EventData::Undefined;
						if(ptrmsmtsystem1->systype == Zweistein::XYDetector::Systemtype::Mesytec) ptrmsmtsystem1->MesytecSystem::listmode_connect(_devlist, *ptr_ctx);
						else ptrmsmtsystem1->listmode_connect(_devlist, *ptr_ctx);
						// find the .json file for the listmode file
						// check if Binning file not empty, if not empty wait for
						Zweistein::setupHistograms(*ptr_ctx,ptrmsmtsystem1,Mesytec::Config::BINNINGFILE.string());
						bool ok = ptrmsmtsystem1->evdata.evntqueue.push(Zweistein::Event::Reset());
						if (!ok) LOG_ERROR << " cannot push Zweistein::Event::Reset()" << std::endl;
						try {
							boost::function<void(Mcpd8::DataPacket&)> abfunc;
							if (ptrmsmtsystem1->systype == Zweistein::XYDetector::Systemtype::Mesytec) {
								abfunc = boost::bind(&Mesytec::MesytecSystem::analyzebuffer, ref(ptrmsmtsystem1), boost::placeholders::_1);
							}
							else abfunc = boost::bind(&Charm::CharmSystem::charm_analyzebuffer, ref(ptrmsmtsystem1), boost::placeholders::_1);
							boost::shared_ptr <Mesytec::listmode::Read> ptrRead = boost::shared_ptr < Mesytec::listmode::Read>(new Mesytec::listmode::Read(abfunc, ptrmsmtsystem1->data, ptrmsmtsystem1->deviceparam));
							auto now = boost::chrono::system_clock::now();
							ptrmsmtsystem1->setStart(now);
							ptrRead->file(fname, *ptr_ctx);
							while (ptrmsmtsystem1->evdata.evntqueue.read_available()); // wait unitl queue consumed
							// pointer to obj needed otherwise exceptions are not propagated properly
						}
						catch (Mesytec::listmode::read_error& x) {
							if (int const* mi = boost::get_error_info<Mesytec::listmode::my_info>(x)) {
								auto  my_code = magic_enum::enum_cast<Mesytec::listmode::read_errorcode>(*mi);
								if (my_code.has_value()) {
									auto c1_name = magic_enum::enum_name(my_code.value());
									 LOG_ERROR << c1_name << std::endl;
								}
							}
						}
						catch (std::exception& e) {
							LOG_ERROR << e.what() << std::endl;
						}
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

			if(configok) Zweistein::Logger::Add_File_Sink(Mesytec::Config::DATAHOME.string() + appName + ".log");
			try {
				//we can write as text file:
				std::string fp = Zweistein::Config::inipath.string();
				std::ofstream outfile;
				outfile.open(fp, std::ios::out | std::ios::trunc);
				outfile << ss_1.str();
				outfile.close();

				if (!configok) LOG_ERROR << "Please edit to fix configuration error : " << fp << std::endl;

			}
			catch (std::exception& e) { // exception expected, //std::cout << boost::diagnostic_information(e);
				LOG_ERROR << e.what() << " for writing." << std::endl;
			}

			if (!configok)	return -1;

			if (!inputfromlistfile) {
				for (Mcpd8::Parameters& p : _devlist) {
					Zweistein::ping(p.mcpd_ip); // sometime
				}
			}

			t = [ &ptrmsmtsystem1, &_devlist,&write2disk]() {
				try {
					ptrmsmtsystem1->write2disk=write2disk;
					if(ptrmsmtsystem1->systype == Zweistein::XYDetector::Systemtype::Mesytec) ptrmsmtsystem1->Mesytec::MesytecSystem::connect(_devlist, *ptr_ctx);
					else ptrmsmtsystem1->connect(_devlist, *ptr_ctx);
					ptr_ctx->run();
				}

				catch (Mesytec::cmd_error& x) {
					LOG_ERROR << Mesytec::cmd_errorstring(x) << std::endl;
				}

				catch (boost::exception & e) {
					LOG_ERROR << boost::diagnostic_information(e) << std::endl;

				}

				ptr_ctx->stop();


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

			worker_threads.create_thread([&ptrmsmtsystem1] {Zweistein::populateHistograms(*ptr_ctx, ptrmsmtsystem1); });
			worker_threads.create_thread([&ptrmsmtsystem1] {Zweistein::displayHistogram(*ptr_ctx, ptrmsmtsystem1); });
			// nothing to do really,


			boost::function<void()> progressmonitor = [&ptrmsmtsystem1]() {
					int l = 0;
					char clessidra[8] = { '|', '/' , '-', '\\', '|', '/', '-', '\\' };
					long long lastcount = 0;
					boost::chrono::nanoseconds elap_ns_last = boost::chrono::nanoseconds(0);
					while(!ptr_ctx->stopped()) {
						boost::this_thread::sleep_for(boost::chrono::milliseconds(300));
						boost::chrono::system_clock::time_point tps(ptrmsmtsystem1->getStart());
						boost::chrono::nanoseconds elap_ns = ptrmsmtsystem1->data.elapsed;
						boost::chrono::duration<double> sec = elap_ns- elap_ns_last;
						boost::chrono::duration<double> total_running = elap_ns;

						boost::chrono::nanoseconds real_elap_ns = boost::chrono::duration_cast<boost::chrono::nanoseconds>(boost::chrono::system_clock::now() - tps);

						double replayspeedmultiplier = (real_elap_ns.count() != 0) ? ((double)elap_ns.count())/((double)real_elap_ns.count()) : 1;
						long long currentcount = ptrmsmtsystem1->evdata.evntcount;
						double evtspersecond = sec.count() != 0 ? (double)(currentcount - lastcount) / sec.count() : 0;
						{
							boost::mutex::scoped_lock lock(coutGuard);
							std::cout << "\r" << clessidra[l++ % 8] << " " << std::setprecision(0) << std::fixed << evtspersecond <<
								" Events/s, (" << Zweistein::PrettyBytes((size_t)(evtspersecond * sizeof(Mesy::Mpsd8Event))) <<
								 "/s)\t" << Mcpd8::DataPacket::deviceStatus(ptrmsmtsystem1->data.last_deviceStatusdeviceId) <<
								" elapsed:" << total_running << " Replay speed Multiplier: "<< std::setprecision(1) << std::fixed << replayspeedmultiplier << "            ";// << std::endl;
#ifndef _WIN32
							std::cout << std::flush;
#endif
						}
						lastcount = currentcount;
						elap_ns_last = elap_ns;
					}
			};
			auto pt2 = new boost::thread(boost::bind(progressmonitor));
			worker_threads.add_thread(pt2);
			while (!ptr_ctx->stopped()) {
				boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
				ptr_ctx->run_one();
			}
		}
		else {
			// data acquisition monitoring loop
			int k = 0;
			int maxK = 10;
			for ( k = 0; k < maxK; k++) {
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
								LOG_ERROR << Mesytec::cmd_errorstring(x) << std::endl;
							}

							catch (boost::exception& e) {
									LOG_ERROR << boost::diagnostic_information(e) << std::endl;

							}
							ptr_ctx->stop();
						};
						worker_threads.create_thread(boost::bind(setipaddrcmd));
						boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
						break;
					}
					else {
						Zweistein::setupHistograms(*ptr_ctx, ptrmsmtsystem1, Mesytec::Config::BINNINGFILE.string());
						worker_threads.create_thread([&ptrmsmtsystem1] {Zweistein::populateHistograms(*ptr_ctx, ptrmsmtsystem1); });
						worker_threads.create_thread([&ptrmsmtsystem1] {Zweistein::displayHistogram(*ptr_ctx, ptrmsmtsystem1); });
						boost::function<void()> sendstartcmd = [&ptrmsmtsystem1, &_devlist, &write2disk,&simuratedefault]() {
							try {
								unsigned long rate = simuratedefault/_devlist.size();
								if (write2disk) worker_threads.create_thread([&ptrmsmtsystem1] {Mesytec::writeListmode(*ptr_ctx, ptrmsmtsystem1); });
								ptrmsmtsystem1->SendAll(Mcpd8::Cmd::START);
								for (auto& kvp : ptrmsmtsystem1->deviceparam) {
									if (kvp.second.datagenerator == Zweistein::DataGenerator::NucleoSimulator) {
										ptrmsmtsystem1->Send(kvp, Mcpd8::Internal_Cmd::SETNUCLEORATEEVENTSPERSECOND, rate);//1650000 is maximum
									}
									if (kvp.second.datagenerator == Zweistein::DataGenerator::CharmSimulator) {
										//if(typeid(*ptrmsmtsystem1) !=
										ptrmsmtsystem1->Send(kvp, Mcpd8::Internal_Cmd::CHARMSETEVENTRATE, rate); // oder was du willst
										ptrmsmtsystem1->Send(kvp, Mcpd8::Internal_Cmd::CHARMPATTERNGENERATOR, 1); // oder was du willst
									}
								}
							}
							catch (Mesytec::cmd_error& x) {
								LOG_ERROR << Mesytec::cmd_errorstring(x) << std::endl;
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
				if (k == maxK) {
					LOG_ERROR << " NOT CONNECTED (tried " << k << " seconds)" << std::endl;
				}
			Zweistein::Averaging<double> avg(3);
			int l = 0;
			char clessidra[8] = { '|', '/' , '-', '\\', '|', '/', '-', '\\' };
			std::chrono::milliseconds ms(100);

			while (!ptr_ctx->stopped()) {
					boost::this_thread::sleep_for(boost::chrono::milliseconds(500)); // we want watch_incoming_packets active
					if (setupafterconnect) continue;
					boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - start;
					start = boost::chrono::system_clock::now();
					long long currentcount = ptrmsmtsystem1->evdata.evntcount;
					double evtspersecond = sec.count() != 0 ? (double)(currentcount - lastcount) / sec.count() : 0;
					avg.addValue(evtspersecond);
					{
						boost::mutex::scoped_lock lock(coutGuard);
						boost::chrono::system_clock::time_point tps(ptrmsmtsystem1->getStart());
						boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - tps;
						std::cout << "\r"<<clessidra[l++ % 8] <<" "<< std::setprecision(0) << std::fixed << avg.getAverage() << " Events/s, (" << Zweistein::PrettyBytes((size_t)(evtspersecond * sizeof(Mesy::Mpsd8Event))) << "/s)\t" << Mcpd8::DataPacket::deviceStatus(ptrmsmtsystem1->data.last_deviceStatusdeviceId) << " elapsed:" << sec << "     ";// << std::endl;
						lastcount = currentcount;
#ifndef _WIN32
						std::cout << std::flush;
#endif
					}
					ptr_ctx->run_for(ms);
				};
			}


	}
	catch (boost::exception & e) {
		LOG_ERROR<< boost::diagnostic_information(e) << std::endl;
	}


	if (ptrmsmtsystem1 ) {
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
