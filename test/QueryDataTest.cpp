// ======================================================================
/*!
 * \brief Run testing and data dumping on given config files
 *
 */
// ======================================================================

extern "C" {
#include <signal.h>
}

#include <boost/version.hpp>
#if BOOST_VERSION / 100 <= 1055
// Due to linking problems, at least boost 1.55 requires these
#ifndef BOOST_NO_CXX11_SCOPED_ENUMS
#define BOOST_NO_CXX11_SCOPED_ENUMS
#endif
#endif

#include "Engine.h"
#include "Producer.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include <libconfig.h++>
#include <memory>

#include <boost/atomic.hpp>

#include <boost/filesystem.hpp>

using namespace std;
using namespace SmartMet::Engine::Querydata;

const string _configfile = "querydata.conf";

const int timeout = 10;  // Timeout in seconds

void sighandler(int)
{
  cout << "exiting after signal catch" << endl;
  exit(0);
}

// We use this class to get some testing methods which have access to protected members of
// Engine-class
class EngineW : public SmartMet::Engine::Querydata::Engine
{
 public:
  EngineW(const std::string& configfile) : SmartMet::Engine::Querydata::Engine(configfile) {}

  void dumpConfigs()
  {
    for (auto a : this->producers())
    {
      cout << configToStr(a);
    }
  }

  std::string configToStr(std::string producer)
  {
    auto cfg = getProducerConfig(producer);
    std::stringstream strstr;

    strstr << cfg.producer << ":" << endl << "{" << endl;
    for (auto alias : cfg.aliases)
      strstr << "  alias = \"" << alias << "\";" << endl;
    strstr << "  directory = \"" << cfg.directory.string() << "\";" << endl
           << "  pattern = \"" << cfg.pattern << "\";" << endl
           << "  type = \"" << cfg.type << "\";" << endl
           << "  leveltype = \"" << cfg.leveltype << "\";" << endl
           << "  refresh_interval_secs = " << cfg.refresh_interval_secs << ";" << endl
           << "  number_to_keep = " << cfg.number_to_keep << ";" << endl
           << "  maxdistance = " << cfg.maxdistance << ";" << endl
           << "  ismultifile = " << cfg.ismultifile << ";" << endl
           << "  isforecast = " << cfg.isforecast << ";" << endl
           << "  isclimatology = " << cfg.isclimatology << ";" << endl
           << "  isfullgrid = " << cfg.isfullgrid << ";" << endl
           << "};" << endl
           << endl;

    return strstr.str();
  }

  // Allow explicit initialization
  inline void initMe() { init(); }

  inline std::time_t getConfigModTime()
  {
    return SmartMet::Engine::Querydata::Engine::getConfigModTime();
  }
};

static std::vector<string> errors;

void adderror(string err)
{
  errors.push_back(err);
  cerr << "Test failed: " << err << endl;
}

// Test configs
static struct _A : public SmartMet::Engine::Querydata::ProducerConfig
{
  _A()
  {
    producer = "ecmwf_eurooppa_pinta";
    aliases = {"ec"};
    directory = "../../../data/ecpinta";
    pattern = ".*_ecmwf_eurooppa_pinta\\.sqd$";
    type = "grid";
    leveltype = "surface";
    refresh_interval_secs = 10;
    number_to_keep = 1;
    maxdistance = -1;
    ismultifile = 0;
    isforecast = 1;
    isclimatology = 0;
    isfullgrid = 1;
  }
} ecmwf_eurooppa_pinta;

static struct _B : public SmartMet::Engine::Querydata::ProducerConfig
{
  _B()
  {
    producer = "tutka_suomi_rr";
    aliases = {"rr"};
    directory = "/data/pal/querydata/tutka/suomi/rr";
    pattern = ".*_tutka_suomi_rr\\.sqd$";
    isforecast = 1;
    type = "grid";
    leveltype = "surface";
    refresh_interval_secs = 10;
    number_to_keep = 50;
  }
} tutka_suomi_rr;

int main()
{
  std::shared_ptr<EngineW> engine;
  signal(SIGINT, &sighandler);
  signal(SIGQUIT, &sighandler);
  signal(SIGHUP, &sighandler);
  signal(SIGABRT, &sighandler);
  signal(SIGTERM, &sighandler);

  // Testing compiler support for atomic and shared pointers
  {
    //
    boost::shared_ptr<SmartMet::Spine::Exception> ptr1 =
        boost::make_shared<SmartMet::Spine::Exception>();
    //	  tmp.store(ptr1);
    boost::shared_ptr<SmartMet::Spine::Exception> ptr2;
    boost::atomic_store(&ptr2, ptr1);
    cout << boost::atomic_load(&ptr2)->what() << endl;
  }
  cout << endl
       << "\tThis program will monitor test querydata." << endl
       << "\tYou can abort the program by pressing Ctrl-C." << endl
       << endl;

  try
  {
    cout << "Testing non-existing config file" << endl;
    engine = std::make_shared<EngineW>("/A file which surely does not exist");
    adderror("init should have failed with non-existing file");
    exit(1);
  }
  catch (std::exception& e)
  {
    // FIXME: Just accept any error for now
    //  if (std::strcmp(e.what(), "Qengine configuration error") != 0)
    //    adderror((std::string) "non-existent config file error " + e.what());
    engine.reset();
  }

  // Generate a temporary config file to modify
  boost::filesystem::path origfile(_configfile);
  boost::filesystem::path configfile(origfile.string() + "_" +
                                     (boost::filesystem::path(__FILE__).filename()).string() + "_" +
                                     origfile.filename().string());

  boost::filesystem::copy_file(
      origfile, configfile, boost::filesystem::copy_option::overwrite_if_exists);

  try
  {
    // Open config file for future modifications
    std::fstream conff(configfile.string());

    // Initialize with config file
    engine = std::make_shared<EngineW>(configfile.string());
    //  string pal_skandinavia;

    // Previous update time of config file
    std::time_t prevstamp = 0;

    for (int i = 0; i < 3; i++)
    {
      alarm(timeout);
      // Run iterations with the same file and test various things
      // 0: run tests and change file trivially so that real config will not change
      // 1: run tests and check that config has not changed. Change file more substantially.
      // 2: run tests and check that config has actually changed. Empty file completely.
      // 3: run tests but should not have any producers anymore
      // pal_skandinavia engine->dumpConfig("pal_skandinavia");
      cout << "// Test iteration " << i << endl;
      while (prevstamp >= engine->getConfigModTime())
        this_thread::sleep_for(chrono::seconds(1));

      cout << prevstamp << " " << engine->getConfigModTime() << endl;
      prevstamp = engine->getConfigModTime();

      ProducerConfig cfg1 = engine->getProducerConfig(ecmwf_eurooppa_pinta.producer);
      if (cfg1 != ecmwf_eurooppa_pinta)
        adderror((std::string) "configuration for producer " + ecmwf_eurooppa_pinta.producer +
                 " not read correctly");
      engine->dumpConfigs();
      if (i == 0)
      {
        // Only test int on first ieration
        if (engine->producers().size() != 0)
          adderror((std::string) "non-zero producer list before init (was " +
                   std::to_string(engine->producers().size()) + ")");
        engine->initMe();
      }

      cout << endl << "Producers " << engine->producers().size() << ": " << endl;
      if (engine->producers().size() < 1)
        adderror((std::string) "producer list less than one after init (was " +
                 std::to_string(engine->producers().size()) + ")");
      engine->dumpConfigs();

      if (i == 0)
      {
        conff.seekp(0, ios_base::end);
        conff << "# Dummy line to force change of file"
              << endl;  // Just a dummy line to modify something
        conff.flush();  // Force modifications to disk
      }
    }

    cout << "\tThere are " << timeout << " seconds before the program will" << endl
         << "\texit automatically." << endl
         << endl;

    sleep(timeout);

    cout << endl << "Ending the program after a " << timeout << " second timeout" << endl;
  }
  catch (libconfig::ParseException& e)
  {
    std::cerr << std::endl
              << "Parse error on line " << e.getLine() << " of '" << configfile << "' : '"
              << e.getError() << "'" << std::endl;
    return 1;
  }
  catch (SmartMet::Spine::Exception& e)
  {
    cerr << "Stack trace: " << endl << e.getStackTrace();
    return 120;
  }

  boost::filesystem::remove(configfile);

  if (errors.size() > 0)
  {
    cerr << errors.size() << " tests failed:" << endl;
    for (string e : errors)
    {
      cerr << "  " << e << endl;
    }
    exit(errors.size());
  }
  exit(0);
  // return 0; // Currently this will segfault
}
