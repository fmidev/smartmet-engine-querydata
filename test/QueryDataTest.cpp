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

const int maxiter = 15;  // Max iterations of wait cycles

void sighandler(int)
{
  cout << "exiting after signal catch" << endl;
  exit(0);
}

static const std::string boolToStr(bool value)
{
  static const string t = "true";
  static const string f = "false";
  if (value)
    return t;
  return f;
}

static std::string configToStr(ProducerConfig cfg)
{
  std::stringstream strstr;

  strstr << cfg.producer << ":" << endl << "{" << endl;
  for (auto alias : cfg.aliases)
    strstr << "  alias = \"" << alias << "\";" << endl;
  strstr << "  directory = \"" << cfg.directory.string() << "\";" << endl
         << "  pattern = \"" << cfg.pattern << "\";" << endl
         << "  type = \"" << cfg.type << "\";" << endl
         << "  leveltype = \"" << cfg.leveltype << "\";" << endl
         << "  refresh_interval_secs = " << cfg.refresh_interval_secs << ";" << endl
         << "  number_to_keep = " << cfg.number_to_keep << ";" << endl;
  if (cfg.maxdistance != -1)
    strstr << "  maxdistance = " << cfg.maxdistance << ";" << endl;
  strstr << "  multifile = " << boolToStr(cfg.ismultifile) << ";" << endl
         << "  forecast = " << boolToStr(cfg.isforecast) << ";" << endl
         << "  climatology = " << boolToStr(cfg.isclimatology) << ";" << endl
         << "  fullgrid = " << boolToStr(cfg.isfullgrid) << ";" << endl
         << "};" << endl
         << endl;

  return strstr.str();
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
    ProducerConfig cfg = getProducerConfig(producer);
    return ::configToStr(cfg);
  }

  // Allow explicit initialization
  inline void initMe() { init(); }

  // Expose some internal data
  inline std::time_t getConfigModTime()
  {
    return SmartMet::Engine::Querydata::Engine::getConfigModTime();
  }

  inline int getLastConfigErrno()
  {
    return SmartMet::Engine::Querydata::Engine::getLastConfigErrno();
  }
};

static std::vector<string> errors;

void adderror(string err)
{
  errors.push_back(err);
  cerr << "Test failed: " << err << endl;
}

// Test configs
static struct confA : public SmartMet::Engine::Querydata::ProducerConfig
{
  confA()
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
} confA;

static struct confB : public SmartMet::Engine::Querydata::ProducerConfig
{
  confB()
  {
    producer = "pal_skandinavia";
    aliases = {"pal"};
    directory = "../../../data/pal";
    pattern = ".*_pal_skandinavia_pinta\\.sqd$";
    isforecast = true;
    type = "grid";
    leveltype = "surface";
    refresh_interval_secs = 5;
    number_to_keep = 2;
  }
} confB;

static struct confX : public SmartMet::Engine::Querydata::ProducerConfig
{
  confX()
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
} confX;

static string generateConfigFile(std::vector<SmartMet::Engine::Querydata::ProducerConfig> configs)
{
  std::stringstream strstr;

  strstr << "# Autogenerated test config" << endl << "producers =" << endl << "[" << endl;

  for (auto conf : configs)
  {
    strstr << "       \"" << conf.producer << "\"";
    if (conf.producer != configs.back().producer)
      strstr << ",";  // Do not put , on the list of producers for last producer
    strstr << endl;
  }

  strstr << "];" << endl << endl;

  for (auto conf : configs)
    strstr << configToStr(conf) << endl;

  return strstr.str();
}

int main()
{
  signal(SIGINT, &sighandler);
  signal(SIGQUIT, &sighandler);
  signal(SIGHUP, &sighandler);
  signal(SIGABRT, &sighandler);
  signal(SIGTERM, &sighandler);

  std::vector<SmartMet::Engine::Querydata::ProducerConfig> configs;
  configs.push_back(confA);
  configs.push_back(confB);

  // Testing compiler support for atomic and shared pointers
  // Should output nothing
  {
    boost::shared_ptr<SmartMet::Spine::Exception> ptr1 =
        boost::make_shared<SmartMet::Spine::Exception>();
    //	  tmp.store(ptr1);
    boost::shared_ptr<SmartMet::Spine::Exception> ptr2;
    boost::atomic_store(&ptr2, ptr1);
    cout << boost::atomic_load(&ptr2)->what() << endl;
  }

  EngineW* engine = nullptr;

  cout << "Testing non-existing config file ";
  try
  {
    cout << "... ";
    // engine = std::make_shared<EngineW>("/A file which surely does not exist");
    engine = new EngineW("/A file which surely does not exist");
    adderror("init should have failed with non-existing file");
    exit(1);
  }
  catch (std::exception& e)
  {
    if (strstr(e.what(), "No such file or directory") == nullptr)
    {
      cout << "failed" << endl;
      adderror((std::string) "non-existent file should have given ENOENT but was " + e.what());
    }
    else
      cout << "done" << endl;
  }
  delete engine;
  engine = nullptr;

  // Generate a temporary config file to modify
  boost::filesystem::path configfile((boost::filesystem::path(__FILE__).filename()).string() +
                                     "_autogentest.conf");

  try
  {
    cout << "Generating " << configfile.string() << endl;
    // Open config file for future modifications
    std::fstream conff(configfile.string(), fstream::out);
    conff << generateConfigFile(configs) << endl;
    conff.flush();

    cout << "Creating engine" << endl;

    // Initialize with config file
    //    engine = std::make_shared<EngineW>(configfile.string());
    engine = new EngineW(configfile.string());
    //_eng = EngineW(configfile.string());

    // Previous update time of config file
    std::time_t prevstamp = 0;

    for (int i = 0; i < 4; i++)
    {
      cout << endl;
      // Run iterations with the same file and test various things
      // 0: run tests and change file trivially so that real config will not change
      // 1: run tests and check that config has not changed. Change file more substantially.
      // 2: run tests and check that config has actually changed. Remove file.
      // 3: run tests ... should have the same configs still

      // Wait for config to reload
      std::time_t etime;
      int c = 0;
      int err = 0;
      do
      {
        etime = engine->getConfigModTime();
        err = engine->getLastConfigErrno();
        cout << "// Test iteration " << i << ": prevstamp=" << prevstamp
             << " configmodtime=" << etime << " errno=" << err << " " << std::strerror(err) << endl;
        if (prevstamp > 0)
          this_thread::sleep_for(chrono::seconds(1));
        c++;
      } while (prevstamp >= etime && c <= maxiter &&
               err != ENOENT);  //&& engine->isConfigFileOK() == true);
                                /*      if (!engine->isConfigFileOK())
                                      {
                                        // Config file not ok i.e. lost or broken
                                        // Wait a bit for it to settle, should still be same
                                        this_thread::sleep_for(chrono::seconds(1));
                                      } */
      prevstamp = engine->getConfigModTime();

      // Compare configurations
      for (auto cfgorig : configs)
      {
        auto cfgwritten = engine->getProducerConfig(cfgorig.producer);
        if (cfgwritten != cfgorig)
          adderror((std::string) "configuration for producer " + cfgorig.producer +
                   " not read correctly");
      }

      // Run init: only on first iteration - should init file change watcher as well
      if (i == 0)
      {
        if (engine->producers().size() != 0)
          adderror((std::string) "non-zero producer list before init (was " +
                   std::to_string(engine->producers().size()) + ")");
        if (engine->getLastConfigErrno() != EINPROGRESS)
        {
          adderror(
              (std::string) "before initialization error should be set to EINPROGRESS but is " +
              std::to_string(engine->getLastConfigErrno()));
        }
        cout << "Initializing engine ... ";
        engine->initMe();
        cout << "done" << endl;
      }

      cout << "Checking configuration read correctly ... ";
      // Check that the number of producers match
      if (engine->producers().size() != configs.size())
        adderror((std::string) "producer list size " + std::to_string(engine->producers().size()) +
                 " different than what was expected " + std::to_string(configs.size()));

      // Step 2 should have the extra config
      if (i >= 2)
      {
        ProducerConfig test = engine->getProducerConfig(confX.producer);

        if (configToStr(test).length() < 1)
          adderror((std::string) "Producer config for " + confX.producer +
                   " appears not loaded corractly");
      }

      // Step 3 should be missing the config and thus have flag set
      if (i == 3)
      {
        if (engine->getLastConfigErrno() != ENOENT)
          adderror((std::string) "Config file has been removed but status still showing it as ok");
      }
      else if (engine->getLastConfigErrno() != 0)
      {
        adderror((std::string) "Config reported error " +
                 std::to_string(engine->getLastConfigErrno()) + " but should be ok ");
      }
      cout << "done" << endl;
      // The end part, modify file accordingly for next step
      cout << "Config preparation for next iteration ... ";
      this_thread::sleep_for(
          chrono::seconds(1));  // Have to sleep before modification to get a new timestamp
      if (i == 0)
      {
        conff.seekp(0, ios_base::end);
        conff << "# Dummy line to force change of file"
              << endl;  // Just a dummy line to modify something
        conff.flush();  // Force modifications to disk
      }
      if (i == 1)
      {
        configs.push_back(confX);
        conff.seekp(0, ios_base::beg);
        conff << "# A new config" << endl << endl << generateConfigFile(configs) << endl;
        conff.flush();
      }
      if (i == 2)
      {
        conff.close();
        boost::filesystem::remove(configfile);
      }

      cout << "done" << endl;
    }
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
    cerr << endl << errors.size() << " tests failed:" << endl;
    for (string e : errors)
    {
      cerr << "  " << e << endl;
    }
    exit(errors.size());
  }
  cout << endl << "All tests ok." << endl;
  exit(0);
  // return 0; // Currently this will segfault
}
