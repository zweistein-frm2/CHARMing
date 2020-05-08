#include "Mesytec.listmode.write.hpp"

namespace Mesytec {
	 boost::atomic<bool> bListmodeWriting = false;
	 void writeListmode(boost::asio::io_service& io_service, boost::shared_ptr < Mesytec::MesytecSystem> device1) {
		 
		 if (bListmodeWriting) {
			 int MaxWait = 15;
			 int i = 0;
			 for (i = 0; i < MaxWait; i++) {
				 if (listmode::stopwriting)
				 {
					 if (i == 0) LOG_INFO << "Waiting " << MaxWait << " seconds for other writelistmode thread to finish" << std::endl;
					 boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
				 }
				 else break;
			 }
			 if (listmode::stopwriting) {
				 LOG_ERROR << "ERROR : other writelistmode thread not finished after " << MaxWait << " seconds" << std::endl;
				 return;
			 }
			 else if (i > 0) LOG_INFO << "other writelistmode thread finished after " << i << " seconds" << std::endl;
		 }
		 else {
			 listmode::stopwriting = false;
		 }

		 bListmodeWriting = true;
		 namespace bio = boost::iostreams;
		 boost::filesystem::path tmppath = Mesytec::Config::DATAHOME;
		 boost::filesystem::space_info si = boost::filesystem::space(tmppath);
		 auto t = std::time(nullptr);
		 auto tm = *std::localtime(&t);
		 std::ostringstream oss;
		 oss << std::put_time(&tm, "%Y%m%d_%H-%M-%S");
		 auto str = oss.str();
		 tmppath.append(oss.str() + ".mdat");
		 size_t byteswritten = 0;
		 try {

			 bio::mapped_file_params params(tmppath.string());
			 size_t keepspace = 10LL * 1000LL * 1000LL * 1000LL; //10GB
			 params.new_file_size = si.available > keepspace * 3 / 2 ? si.available - keepspace : si.available;
			 params.flags = bio::mapped_file::mapmode::readwrite;

			 bio::stream<bio::mapped_file_sink> f;
			 f.open(bio::mapped_file_sink(params));

			 std::string header = u8"mesytec psd listmode data\nheader length: 2 lines\n";
			 f.write(header.c_str(), header.length());
			 f.write(Mesytec::listmode::header_separator, sizeof(Mesytec::listmode::header_separator));
			 {
				 LOG_INFO << "Writing to:" << tmppath << std::endl;
			 }
			 Mcpd8::DataPacket dp;

			 do {
				 while (device1->data.listmodequeue.pop(dp)) {
					 short* sp = (short*)&dp;
					 int len = dp.BytesUsed();
					 for (int i = 0; i < len / sizeof(short); i++) 	boost::endian::endian_reverse_inplace(sp[i]);
					 f.write(reinterpret_cast<char*>(&dp), len);
					 f.write(Mesytec::listmode::datablock_separator, sizeof(Mesytec::listmode::datablock_separator));
				 }
				 if (listmode::stopwriting) break;
			 } while (!io_service.stopped());
			 f.write(Mesytec::listmode::closing_signature, sizeof(Mesytec::listmode::closing_signature));
			 byteswritten = f.tellp();
			 f.close();

		 }
		 catch (boost::exception& e) {
			 LOG_ERROR << boost::diagnostic_information(e) << std::endl;
		 }

		 std::filesystem::resize_file(tmppath.string(), byteswritten);

		 boost::filesystem::path jsonpath(tmppath.string() + ".json");
		
		 try {
			 boost::property_tree::write_json(jsonpath.string(), Mesytec::Config::root);
		 }
		 catch (std::exception& e) { // exception expected, //std::cout << boost::diagnostic_information(e); 
			 LOG_ERROR << e.what() << " for writing." << std::endl;
		 }

		 bListmodeWriting = false;
		 listmode::stopwriting = false; // rearm for next, but must start new thread again

		 {
			 LOG_INFO << Zweistein::PrettyBytes(byteswritten) << " written to:" << tmppath.string() << std::endl;
		 }
		 
	 }

	 

}
