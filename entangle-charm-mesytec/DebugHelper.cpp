/*                          _              _                _
	___  __ __ __  ___     (_)     ___    | |_     ___     (_)    _ _
   |_ /  \ V  V / / -_)    | |    (_-<    |  _|   / -_)    | |   | ' \
  _/__|   \_/\_/  \___|   _|_|_   /__/_   _\__|   \___|   _|_|_  |_||_|
	   .
	   |\       Copyright (C) 2019 - 2020 by Andreas Langhoff
	 _/]_\_                            <andreas.langhoff@frm2.tum.de>
 ~~~"~~~~~^~~   This program is free software; you can redistribute it
 and/or modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation;*/


#include <boost/exception/all.hpp>
#include <boost/system/error_code.hpp>
#include <boost/exception/error_info.hpp>
#include <errno.h>
#include <stdio.h>
#include "MesytecSystem.cpp"


bool retry = true;
void catch_ctrlc(const boost::system::error_code& error, int signal_number) {
	{
		boost::mutex::scoped_lock lock(coutGuard);
		LOG_INFO << "handling signal " << signal_number << std::endl;
	}
	if (signal_number == 2) {
		retry = false;
		//	boost::throw_exception(std::runtime_error("aborted by user"));
		//	throw std::runtime_error("aborted by user");
			//throw boost::system::system_error{ boost::system::errc::make_error_code(boost::system::errc::owner_dead) };

	}

}

int main(int argc, char* argv[])
{
	boost::asio::signal_set signals(io_service, SIGINT, SIGSEGV);
	//signals.async_wait(boost::bind(&boost::asio::io_service::stop, &io_service));
	signals.async_wait(&catch_ctrlc);
	boost::shared_ptr < NeutronMeasurement> pNM ;
	try {
		pNM.reset( new NeutronMeasurement(_fileno(stdout)));
		pNM->on();
		boost::this_thread::sleep_for(boost::chrono::milliseconds(8000));
		pNM->off();
		boost::this_thread::sleep_for(boost::chrono::milliseconds(2000));



		boost::this_thread::sleep_for(boost::chrono::milliseconds(2000));
		pNM->on();
		int l = 0;
		do {
			char clessidra[8] = { '|', '/' , '-', '\\', '|', '/', '-', '\\' };
			//std::cout << "\r  waiting for action " << clessidra[l % 8];
			l++;
			std::chrono::milliseconds ms(100);
			io_service.run_for(ms);

		}
		while (retry);

	}
	catch (boost::exception& e) {
		LOG_ERROR << boost::diagnostic_information(e) << std::endl;
	}
	pNM->off();
	pNM.reset();

}