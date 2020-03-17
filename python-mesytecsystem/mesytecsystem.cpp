// NumpyCpp.cpp : Diese Datei enthält die Funktion "main". Hier beginnt und endet die Ausführung des Programms.
//
#define PY_ARRAY_UNIQUE_SYMBOL pbcvt_ARRAY_API
#include <boost/python.hpp>
#include <boost/python/tuple.hpp>
#include <pyboostcvconverter/pyboostcvconverter.hpp>
#include <boost/python/numpy.hpp>
#include <iostream>
#include <filesystem>
#include "Zweistein.PrettyBytes.hpp"
#include "Mesytec.RandomData.hpp"
#include <opencv2/core.hpp>
#include <opencv2/core/mat.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "Zweistein.Histogram.hpp"
#include "Mesytec.Mcpd8.hpp"
#include "Zweistein.HomePath.hpp"
#include "Zweistein.GetConfigDir.hpp"
#include "Mesytec.config.hpp"
#include "Zweistein.ThreadPriority.hpp"
#include "Zweistein.populateHistograms.hpp"
#include "Mesytec.listmode.write.hpp"

std::string PROJECT_NAME("charm");

namespace p = boost::python;
namespace np = boost::python::numpy;
using boost::asio::ip::udp;


EXTERN_FUNCDECLTYPE boost::mutex coutGuard;
EXTERN_FUNCDECLTYPE boost::thread_group worker_threads;
boost::mutex histogramsGuard;
std::vector<Histogram> histograms = std::vector<Histogram>(2);

boost::asio::io_service io_service;


np::ndarray GetHistogramNumpy() {

    int maxX = 3;
    int maxY = 40;
    p::tuple shape = p::make_tuple(maxY, maxX);
    np::dtype dtype = np::dtype::get_builtin<float>();
    np::ndarray histogram = np::zeros(shape, dtype);
    int z = 0;

    unsigned  short x_pos;
    unsigned short position_y;
    long imax = 10 * maxX * maxY;
    for (int i = 0; i < imax; i++) {
        unsigned long l = Zweistein::Random::RandomData(x_pos, position_y, i, maxY, maxX);
        histogram[x_pos, maxY - position_y - 1] += 1;
    }

    return histogram;
}

namespace legacy {

    void initialize() { LOG_INFO << "legacy::initialize()" << std::endl; }
    void shutdown() { LOG_INFO << "legacy::shutdown()" << std::endl;
       io_service.stop();
       boost::this_thread::sleep_for(boost::chrono::milliseconds(500));
      
       worker_threads.join_all();
       LOG_INFO << "worker_threads.join_all()";
       boost::log::core::get()->remove_all_sinks();
    }

}


struct StartMsmtParameters {
    StartMsmtParameters() :MaxCount(0), DurationSeconds(0), writelistmode(false) {}
    boost::atomic<long long> MaxCount;
    boost::atomic<double> DurationSeconds;
    boost::atomic<bool> writelistmode;

};

void startMonitor(boost::shared_ptr < Mesytec::MesytecSystem> ptrmsmtsystem1, boost::shared_ptr <StartMsmtParameters> ptrStartParameters ) {
        boost::filesystem::path inidirectory = Zweistein::Config::GetConfigDirectory();
        boost::filesystem::path inipath = inidirectory;
        inipath.append(PROJECT_NAME + ".json");
        LOG_INFO << "Using config file:" << inipath << " " << std::endl;
        boost::asio::signal_set signals(io_service, SIGINT, SIGTERM);
        signals.async_wait(boost::bind(&boost::asio::io_service::stop, &io_service));

        boost::property_tree::ptree root;

        try { boost::property_tree::read_json(inipath.string(), root); }
        catch (std::exception& e) {
            LOG_ERROR << e.what() << " for reading.";
        }
        std::list<Mcpd8::Parameters> _devlist = std::list<Mcpd8::Parameters>();

        ptrmsmtsystem1->data.Format = Mcpd8::EventDataFormat::Mpsd8;


        bool configok = Mesytec::Config::get(root, _devlist, inidirectory.string());


        std::stringstream ss_1;
        boost::property_tree::write_json(ss_1, root);
        LOG_INFO << ss_1.str();

        if (!configok) {
                    return;
        }
        Add_File_Sink(Mesytec::Config::DATAHOME.string() + PROJECT_NAME + ".log");
        try {
            boost::property_tree::write_json(inipath.string(), root);
        }
        catch (std::exception& e) { // exception expected, //std::cout << boost::diagnostic_information(e); 
            LOG_ERROR << e.what() << " for writing.";
        }
        boost::function<void()> t;
        t = [&ptrmsmtsystem1, &_devlist]() {
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
                        LOG_ERROR << c1_name << std::endl;
                    }

                }

            }
            catch (boost::exception& e) {
                boost::mutex::scoped_lock lock(coutGuard);
                LOG_ERROR << boost::diagnostic_information(e);

            }
            io_service.stop();

        };

        auto pt = new boost::thread(boost::bind(t));

        Zweistein::Thread::CheckSetUdpKernelBufferSize();
        Zweistein::Thread::SetThreadPriority(pt->native_handle(), Zweistein::Thread::PRIORITY::HIGH);
        worker_threads.add_thread(pt);
        boost::this_thread::sleep_for(boost::chrono::milliseconds(2500));// so msmtsystem1 should be connected
       
        worker_threads.create_thread([&ptrmsmtsystem1] {Zweistein::populateHistograms(io_service, ptrmsmtsystem1, Mesytec::Config::BINNINGFILE.string(), ""); });
        
        boost::chrono::system_clock::time_point start = boost::chrono::system_clock::now();
        long long lastcount = 0;
        while (!io_service.stopped()) {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(500));
             //       if (PyErr_CheckSignals() == -1) {
         //        io_service.stop();
        //      }

             if (ptrmsmtsystem1->data.last_deviceStatusdeviceId & Mcpd8::Status::DAQ_Running) {

                bool sendstop = false;
                if (ptrStartParameters->MaxCount != 0 && ptrmsmtsystem1->data.evntcount > ptrStartParameters->MaxCount) sendstop = true;
                boost::chrono::system_clock::time_point tps(ptrmsmtsystem1->started);
                boost::chrono::duration<double> secs = boost::chrono::system_clock::now() - tps;
                boost::chrono::duration<double> Maxsecs(ptrStartParameters->DurationSeconds);

                if(secs>=Maxsecs && Maxsecs!= boost::chrono::duration<double>::zero())sendstop = true;
 
                if (sendstop) ptrmsmtsystem1->SendAll(Mcpd8::Cmd::STOP);
            }
           
            boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - start;
            start = boost::chrono::system_clock::now();
            long long currentcount = ptrmsmtsystem1->data.unprocessedcounts;
            double evtspersecond = sec.count() != 0 ? (double)(currentcount - lastcount) / sec.count() : 0;
            {
                boost::mutex::scoped_lock lock(coutGuard);
                std::cout << "\r" << std::setprecision(0) << std::fixed << evtspersecond << " Events/s, (" << Zweistein::PrettyBytes((size_t)(evtspersecond * sizeof(Mesy::Mpsd8Event))) << "/s)\t"<< Mcpd8::DataPacket::deviceStatus(ptrmsmtsystem1->data.last_deviceStatusdeviceId)<<" elapsed:"<<sec;// << std::endl;
                lastcount = currentcount;
#ifndef _WIN32
                std::cout << std::flush;
#endif
            }
            io_service.run_one();
        };
        LOG_DEBUG << "startMonitor() exiting..." << std::endl;

    }

struct NeutronMeasurement {
     public:
        boost::shared_ptr<StartMsmtParameters> ptrStartParameters;
        boost::shared_ptr < Mesytec::MesytecSystem> ptrmsmtsystem1;
        NeutronMeasurement() :ptrStartParameters(boost::shared_ptr < StartMsmtParameters>(new StartMsmtParameters())),
            ptrmsmtsystem1(boost::shared_ptr < Mesytec::MesytecSystem>(new Mesytec::MesytecSystem())) {
            worker_threads.create_thread([this] {startMonitor(ptrmsmtsystem1, ptrStartParameters); });
            LOG_INFO << "NeutronMeasurement()" << std::endl;

        }
        ~NeutronMeasurement() {
           
            LOG_INFO << "~NeutronMeasurement()" << std::endl;
            if (ptrmsmtsystem1) {
                try { ptrmsmtsystem1->SendAll(Mcpd8::Cmd::STOP); }
                catch (boost::exception& e) {
                    boost::mutex::scoped_lock lock(coutGuard);
                    LOG_ERROR << boost::diagnostic_information(e);
                }
                //	delete ptrmsmtsystem1;
                //	ptrmsmtsystem1 = nullptr;
            }
        }

        bool get_writelistmode() {
            return ptrStartParameters->writelistmode;
        }
        void set_writelistmode(bool val) {
            ptrStartParameters->writelistmode = val;
        }
        void stopafter(uint64 counts, double seconds) {
            ptrStartParameters->MaxCount = counts;
            ptrStartParameters->DurationSeconds = seconds;
            LOG_DEBUG << "NeutronMeasurement.stopafter(" << counts << ", " << seconds << ")" << std::endl;
        }
       
        boost::python::tuple status() {

            boost::chrono::system_clock::time_point tps(ptrmsmtsystem1->started);
            boost::chrono::duration<double> secs = boost::chrono::system_clock::now() - tps;
            long long count = ptrmsmtsystem1->data.evntcount;
            return boost::python::make_tuple(count, secs.count(), Mcpd8::DataPacket::deviceStatus(ptrmsmtsystem1->data.last_deviceStatusdeviceId));

        }


        void start() {
            LOG_DEBUG << "NeutronMeasurement.start()" << std::endl;
            auto status = Mcpd8::DataPacket::getStatus(ptrmsmtsystem1->data.last_deviceStatusdeviceId);
            if (status & Mcpd8::Status::DAQ_Running) {
                LOG_WARNING << "skipped ," << status << std::endl;
                return;

            }
            unsigned long rate = 185000;
            try {
                if (ptrStartParameters->writelistmode) worker_threads.create_thread([this] {Mesytec::writeListmode(io_service, ptrmsmtsystem1); });
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
                LOG_ERROR << boost::diagnostic_information(e);

            }

        }
        void stop() {
            LOG_DEBUG << "NeutronMeasurement.stop()" << std::endl;
            try { ptrmsmtsystem1->SendAll(Mcpd8::Cmd::STOP); }
            catch (boost::exception& e) {
                boost::mutex::scoped_lock lock(coutGuard);
                LOG_ERROR << boost::diagnostic_information(e);
            }
        }
        void _continue() {}

        Histogram* getHistogram() {
            LOG_DEBUG << "getHistogram()" << std::endl;
            return &histograms[1];
        }

    };

namespace {
struct legacy_api_guard
{
    legacy_api_guard() { legacy::initialize(); }
    ~legacy_api_guard() { legacy::shutdown(); }
};

/// @brief Global shared guard for the legacy API.
boost::shared_ptr<legacy_api_guard> legacy_api_guard_;
/// @brief Get (or create) guard for legacy API.
boost::shared_ptr<legacy_api_guard> get_api_guard()
{
    if (!legacy_api_guard_)
    {
        legacy_api_guard_ = boost::make_shared<legacy_api_guard>();
    }
    return legacy_api_guard_;
}

void release_guard()
{
    io_service.stop();
    boost::this_thread::sleep_for(boost::chrono::milliseconds(500));
    legacy_api_guard_.reset();
}



} // namespace 




/// @brief legacy_object_holder is a smart pointer that will hold
///        legacy types and help guarantee the legacy API is initialized
///        while these objects are alive.  This smart pointer will remain
///        transparent to the legacy library and the user-facing Python.
template <typename T>
class legacy_object_holder
{
public:

    typedef T element_type;

    template <typename... Args>
    legacy_object_holder(Args&&... args)
        : legacy_guard_(::get_api_guard()),
        ptr_(boost::make_shared<T>(boost::forward<Args>(args)...))
    {}

    legacy_object_holder(legacy_object_holder& rhs) = default;

    element_type* get() const { return ptr_.get(); }

private:

    // Order of declaration is critical here.  The guard should be
    // allocated first, then the element.  This allows for the
    // element to be destroyed first, followed by the guard.
    boost::shared_ptr<legacy_api_guard> legacy_guard_;
    boost::shared_ptr<element_type> ptr_;
};

/// @brief Helper function used to extract the pointed to object from
///        an object_holder.  Boost.Python will use this through ADL.
template <typename T>
T* get_pointer(const legacy_object_holder<T>& holder)
{
    return holder.get();
}

/// Auxiliary function to make exposing legacy objects easier.
template <typename T, typename ...Args>
legacy_object_holder<T>* make_legacy_object(Args&&... args)
{
    return new legacy_object_holder<T>(std::forward<Args>(args)...);
}

/*
// Wrap the legacy::use_test function, passing the managed object.
void legacy_use_test_wrap(legacy_object_holder<legacy::Test>& holder)
{
    return legacy::use_test(*holder.get());
}
*/

#if (PY_VERSION_HEX >= 0x03000000)

static void* init_ar() {
#else
static void init_ar() {
#endif
    Py_Initialize();

    import_array();
    return NUMPY_IMPORT_ARRAY_RETVAL;
}



BOOST_PYTHON_MODULE(mesytecsystem)
{
    
    using namespace boost::python;

    try {
        ::get_api_guard(); // Initialize.  
        init_ar();
        
        np::initialize();

        to_python_converter<cv::Mat,
            pbcvt::matToNDArrayBoostConverter>();
        pbcvt::matFromNDArrayBoostConverter();
        class_<Histogram, boost::noncopyable>("Histogram", boost::python::no_init)
            .def("update", &Histogram::update)
            .add_property("Roi", &Histogram::getRoi, &Histogram::setRoi)
            .add_property("Size", &Histogram::getSize)
            ;
        class_< NeutronMeasurement>("NeutronMeasurement")
            .add_property("writelistmode", &NeutronMeasurement::get_writelistmode, &NeutronMeasurement::set_writelistmode)
            .def("start", &NeutronMeasurement::start)
            .def("stopafter", &NeutronMeasurement::stopafter)
            .def("status", &NeutronMeasurement::status)
            .def("stop", &NeutronMeasurement::stop)
            .def("getHistogram", &NeutronMeasurement::getHistogram, return_internal_reference<1, with_custodian_and_ward_postcall<1, 0>>())
            ;
        def("GetHistogramNumpy", GetHistogramNumpy);
        import("atexit").attr("register")(make_function(&::release_guard));


    }
    catch (const error_already_set&)
    {
        ::release_guard();
        throw;
    }

   
   
}



