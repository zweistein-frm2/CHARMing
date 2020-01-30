/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/

#pragma once
#include "stdafx.h"
#include <cstddef>
#include <string>
#include <iostream>
#include <bitset>
#include <string_view>
#include <magic_enum.hpp>
#include <random>
#include "Mesytec.hpp"
//#include <opencv2/core.hpp>
//#include <opencv2/core/mat.hpp>
//#include <opencv2/imgcodecs.hpp>
//#include <vector>

namespace Zweistein {
	namespace Font {
		namespace _16x12_horizontal_LSB_1 {
#include "16x12_horizontal_LSB_1.h"
			int width = 16;
			int height = 12;
		}
		namespace _8x14_horizontal_LSB_1 {
#include "8x14_horizontal_LSB_1.h"
			int width = 8;
			int height = 14;
		}
		namespace _8x8_horizontal_LSB_1 {
#include "8x8_horizontal_LSB_1.h"
			int width = 8;
			int height = 8;
		}
	
		
	}
}

namespace Zweistein {
	namespace Random {
		uint32_t xor128(void) {
			static uint32_t x = 123456789;
			static uint32_t y = 362436069;
			static uint32_t z = 521288629;
			static uint32_t w = 88675123;
			uint32_t t;
			t = x ^ (x << 11);
			x = y; y = z; z = w;
			return w = w ^ (w >> 19) ^ (t ^ (t >> 8));
		}

		std::string message("\x03 Gr\x81\xe1\x65 von Zweistein \x0e \x0e");
		int counter = 0;
		long ncalls = 0;
		namespace ourfont = Zweistein::Font::_8x14_horizontal_LSB_1;
		int txt_y = ourfont::width * message.length();
		int txt_x = ourfont::height;
		//static cv::Mat image = cv::Mat::zeros(ourfont::width * message.length(), ourfont::height, CV_8U);
		//bool imgwritten = false;
		int xoffset = 25;
		int yoffset = 200;

		auto start = boost::chrono::high_resolution_clock::now();
		auto lastposchange= boost::chrono::system_clock::now();

		unsigned long RandomData(unsigned short& x_pos, unsigned short& position_y, int i,int sizeY, int maxX = 8) {
			unsigned long l = Zweistein::Random::xor128();
			

			if (Zweistein::Random::ncalls % 30 != 0) {

				position_y = ((unsigned short)(l >> 10)) % sizeY;
				x_pos = (unsigned short)(l >> 20) % maxX;
				int tmp = 0;
			}
			else {
				bool pixelisblack = false;
				int cpos = 0;
				int bytepos = 0;
				int pixx = 0;
				int pixy = 0;
				int le = message.length();
				do {
					cpos = ((counter / (ourfont::height * ourfont::width)) % message.length());
					int curpix = counter % (ourfont::height * ourfont::width);
					unsigned char currentChar = message[cpos];
					const unsigned char* pixelbytes = ourfont::font[currentChar];
					pixx = curpix / ourfont::height;
					pixy = curpix % ourfont::height;
					bytepos = pixx / 8 + pixy * ((ourfont::width + 7) / 8); //ourfont::height;
					unsigned char c = pixelbytes[bytepos];
					counter++;
					pixelisblack = (1 << pixx) & c;
				} while (!pixelisblack);


				x_pos = (((ourfont::height - 1) - pixy) + xoffset) % maxX;
				position_y = (sizeY - (cpos * ourfont::width + pixx + yoffset)) % sizeY;
			}
			return l;


		}
		void Mpsd8EventRandomData(unsigned short data[3], int i, int maxX = 8) {


			data[0] = data[1] = data[2] = 0;
			
			unsigned short x_pos = 0;
			unsigned short position_y = 0;

			unsigned long l=RandomData(x_pos, position_y, i, Mesy::Mpsd8Event::sizeY, maxX);

			unsigned short amplitude = l & 1023L;
			amplitude = 1;//
			data[1] |= (amplitude & 0x7) << 13;
			data[2] |= (amplitude & 0x7f << 3);
			/*
			if (counter < ourfont::height * ourfont::width * message.length()) {
				image.at<char>(position_y, x_pos) = 255;
			}
			else {
				if (!imgwritten) {
					std::cout << std::endl << image << std::endl;
					//	std::vector<int> compression_params;
					//	compression_params.push_back(cv::IMWRITE_PNG_COMPRESSION);
					//	compression_params.push_back(1);
					//	cv::imwrite("c:\\temp\\test.png", image, compression_params);
				}
				imgwritten = true;
			}
			*/
			unsigned short modid = x_pos / Mesy::Mpsd8Event::sizeSLOTS;
			unsigned short slotid = x_pos % Mesy::Mpsd8Event::sizeSLOTS;
			
			
			data[1] |= position_y << 3;
			data[2] |= slotid << 7;
			data[2] |= modid << 12;
			if (i == 0) start = boost::chrono::high_resolution_clock::now();
			auto diff = boost::chrono::high_resolution_clock::now() - start;
			auto nsec = boost::chrono::duration_cast<boost::chrono::nanoseconds>(diff);
			Mesy::Mpsd8Event::settime19bit(data, nsec.count());
			ncalls++;
		}
	}
}

