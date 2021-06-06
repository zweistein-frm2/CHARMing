/***************************************************************************
  *   Copyright (C) 2019 - 2020 by Andreas Langhoff
  *                                          <andreas.langhoff@frm2.tum.de> *
  *   This program is free software; you can redistribute it and/or modify  *
  *   it under the terms of the GNU General Public License as published by  *
  *   the Free Software Foundation;                                         *
  ***************************************************************************/

#include <iostream>
#include <vector>
#include <string>
#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <boost/exception/all.hpp>
#include "Zweistein.Logger.hpp"
#include "Zweistein.ping.hpp"


namespace Zweistein {

	bool ping(std::string ipaddress) {

    namespace bp = boost::process;

    bp::ipstream is;
    bp::ipstream ierr;
    std::string  param;


    try {
        std::vector<std::string> data;
        std::string line;
        std::error_code ec;
        param = "ping";
        boost::filesystem::path p = bp::search_path(param); //
        if (p.empty()) {
            LOG_ERROR << param << " not found." << std::endl;
            return false;
        }
        int retry = 1;
        std::string cc = "-c" + std::to_string(retry);
#ifdef WIN32
        cc = "-n " + std::to_string(retry);
#endif
        std::string cmdline = p.string() + " " + cc + " " + ipaddress;
        param = cmdline;
        bp::child c(cmdline, bp::std_out > is, bp::std_err > ierr);
        c.wait_for(std::chrono::seconds(1), ec);
        if (c.running()) c.terminate();
        while (std::getline(is, line) && !line.empty()) {
            std::string desired = "64 bytes from";
            std::string desired1 = std::to_string(retry)+ " received";
            std::string undesired = "100 % packet loss";
#ifdef WIN32
            undesired = "Request timed out";
            desired = "Reply from";
            desired1= "Reply from";
#endif
            if (line.find(undesired) != std::string::npos) {
                LOG_WARNING << cmdline<<" : "<< line << std::endl;
                return false;
            }
            if (line.find(desired) != std::string::npos) {
                       data.push_back(line);
            }
            else  if (line.find(desired1) != std::string::npos) {
                data.push_back(line);
            }
        }

        if (data.size()) {
            for (auto& s : data)  LOG_INFO <<"ping: "<< s << std::endl;
            return true;
        }
        else {
            while (std::getline(ierr, line) && !line.empty()) {
                LOG_ERROR << line << std::endl;
            }
        }
    }
    catch (boost::exception& e) {
        LOG_ERROR << param << " : " <<boost::diagnostic_information(e) << std::endl;
    }
    return false;
}




}