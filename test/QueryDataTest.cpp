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

struct testerror
{
  int step;
  int line;
  string err;
};

static std::vector<testerror> errors;

void adderrorreal(int line, int step, string err)
{
  struct testerror e = {step, line, err};
  errors.push_back(e);
  cerr << "Test " << step << " failed(" << __FILE__ << ":" << line << "): " << err << endl;
}

// We have to have this as macro since the variable is not defined at this point
#define adderror(str) adderrorreal(__LINE__, test, str)

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

enum testcase
{
  missingfile = 1,
  create,
  initialize,
  modifynull,
  addproducer,
  mutilate,
  deleted,
  rewrite,
  endtest
};

static struct test
{
  string name;
} testcases[] = {"config file missing",
                 "create engine instance",
                 "initialize engine",
                 "null modify config",
                 "add new producer",
                 "malformat config",
                 "remove config file",
                 "rewrite config",
                 "shutdown"};

static const string& testcasename(int i)
{
  static const string unknown = "UNKNOWN TEST - PLEASE FIX";

  switch (i)
  {
    case missingfile:
      return testcases[missingfile - 1].name;
    case create:
      return testcases[create - 1].name;
    case initialize:
      return testcases[initialize - 1].name;
    case modifynull:
      return testcases[modifynull - 1].name;
    case addproducer:
      return testcases[addproducer - 1].name;
    case mutilate:
      return testcases[mutilate - 1].name;
    case deleted:
      return testcases[deleted - 1].name;
    case rewrite:
      return testcases[rewrite - 1].name;
    case endtest:
      return testcases[endtest - 1].name;
    default:
      return unknown;
  }
}

int main(int argc, char* argv[])
{
  int maxiter = 10;  // Max iterations of wait cycles

  if (argc > 1)
  {
    int i = atoi(argv[1]);
    if (i < 1 || argc > 2)
    {
      cerr << "usage: " << argv[0] << " [max number of iterations to wait for changes]" << endl;
      exit(127);
    }
    maxiter = i;
  }

  signal(SIGINT, &sighandler);
  signal(SIGQUIT, &sighandler);
  signal(SIGHUP, &sighandler);
  signal(SIGABRT, &sighandler);
  signal(SIGTERM, &sighandler);

  std::vector<SmartMet::Engine::Querydata::ProducerConfig> configs;

  // Testing compiler support for atomic and shared pointers
  // Should output nothing, mainly test compiler and boost compatibility
  {
    boost::shared_ptr<SmartMet::Spine::Exception> ptr1 =
        boost::make_shared<SmartMet::Spine::Exception>();
    //	  tmp.store(ptr1);
    boost::shared_ptr<SmartMet::Spine::Exception> ptr2;
    boost::atomic_store(&ptr2, ptr1);
    cout << boost::atomic_load(&ptr2)->what() << endl;
  }

  // Holder for future engine
  EngineW* engine = nullptr;
  // Do not use shared pointers: more difficult to track when destructor is actually called
  // Also, shared_ptr:s don't work well in debugger ...

  // Generate a temporary config file to modify
  boost::filesystem::path configfile((boost::filesystem::path(__FILE__).filename()).string() +
                                     "_autogentest.conf");
  cout << "Test configuration will be written to " << configfile.string() << endl << endl;

  // Share the stream class, reopening is not necessary on every iteration
  std::fstream conff;

  try
  {
    for (int test = missingfile; test <= endtest; test++)
    {
      // All tests go through these steps:
      // - create/change/delete file
      // In a loop while waiting for updater thread to detect changes
      // 	- check that Engine is still valid and working
      // - check that the actual changes are there
      // Of course, there are case specific differences ...

      cout << "Test #" << test << ": " << testcasename(test) << endl;
      if (test == missingfile)
      {
        // A special case, just tests working with non-existent file (used to be non-existent error
        // output)
        try
        {
          engine = new EngineW("/A file which surely does not exist");
          adderror("init should have failed with non-existing file");
        }
        catch (std::exception& e)
        {
          if (strstr(e.what(), "No such file or directory") == nullptr)
            adderror((std::string) "non-existent file should have given ENOENT but was " +
                     e.what());
        }

        if (engine != nullptr)
          delete engine;
        engine = nullptr;

        continue;  // Go to next testcase immediately, there is not much we can do
      }
      else
      {
        std::time_t prevstamp = 0;  // Timestamp of config file before changes
        int err = 0;

        if (engine != nullptr)
        {
          prevstamp = engine->getConfigModTime();
          err = engine->getLastConfigErrno();
        }

        if (test == create)
        {
          // Generating config file
          configs.push_back(confA);
          configs.push_back(confB);
          conff.open(configfile.c_str(), std::fstream::out | std::fstream::trunc);
          conff << generateConfigFile(configs) << endl;
          conff.flush();  // Force modifications to disc
          // File will be left open for future modifications by other tests

          // Crete instance of the engine if not already created
          if (engine != nullptr)
          {
            adderror("engine already created! (Errors in test programs?)");
            delete engine;
            engine = nullptr;
          }

          engine = new EngineW(configfile.string());
          err = engine->getLastConfigErrno();
        }
        // Some things to test before init
        if (test <= initialize && engine->producers().size() != 0)
          adderror((std::string) "non-zero producer list before init (was " +
                   std::to_string(engine->producers().size()) + ")");
        if (test <= initialize && err != EINPROGRESS)
          adderror(
              (std::string) "before initialization error should be set to EINPROGRESS but is " +
              std::to_string(err));

        if (test == initialize)
        {
          engine->initMe();
          prevstamp = 0;
        }

        if (test == modifynull)
        {
          this_thread::sleep_for(
              chrono::seconds(1));  // Have to sleep before modification to get a new timestamp
          conff.seekp(0, ios_base::end);
          conff << "# Dummy line to force change of file"
                << endl;  // Just a dummy line to modify something
          conff.flush();  // Force modifications to disk
        }

        if (test == addproducer)
        {
          this_thread::sleep_for(
              chrono::seconds(1));  // Have to sleep before modification to get a new timestamp
          configs.push_back(confX);
          conff.seekp(0, ios_base::beg);
          conff << "# A new config" << endl << endl << generateConfigFile(configs) << endl;
          conff << endl << "# End of working file" << endl;
          conff.flush();
        }

        if (test == mutilate)
        {
          this_thread::sleep_for(
              chrono::seconds(1));  // Have to sleep before modification to get a new timestamp
          conff.seekp(0, ios_base::end);
          conff << "# Mutilated non-working config" << endl
                << "skldfjöskldjföklsajfklösdajf klödaj" << endl;
          conff.flush();
        }

        if (test == deleted)
        {
          conff.close();
          boost::filesystem::remove(configfile);
        }

        if (test == rewrite)
        {
          configs.clear();
          configs.push_back(confA);
          configs.push_back(confX);
          conff.open(configfile.c_str(), std::fstream::out | std::fstream::trunc);
          conff << generateConfigFile(configs) << endl;
          conff.flush();
        }

        if (test == endtest)
        {
          engine->shutdownEngine();
        }

        // Wait loop, wait for new configuration to actually load
        std::time_t etime;
        int c = 0;
        do
        {
          if (prevstamp > 0)
            this_thread::sleep_for(chrono::seconds(1));
          etime = engine->getConfigModTime();
          err = engine->getLastConfigErrno();
          cout << "  wait loop: prevstamp=" << prevstamp << " configmodtime=" << etime
               << " errno=" << err << " " << std::strerror(err) << endl;
          // Compare configurations on every iteration, should still be valid
          for (auto cfgorig : configs)
          {
            // On adding new producer: new producer might not be available yet
            if (test == addproducer && cfgorig.producer == confX.producer)
              continue;

            auto cfgwritten = engine->getProducerConfig(cfgorig.producer);
            if (cfgwritten != cfgorig)
              adderror((std::string) "configuration for producer " + cfgorig.producer +
                       " not read correctly");
          }
          if (test > addproducer)
          {
            ProducerConfig testcfg = engine->getProducerConfig(confX.producer);

            if (configToStr(testcfg).length() < 1)
              adderror((std::string) "Producer config for " + confX.producer +
                       " appears not loaded correctly");
          }
          c++;
        } while (prevstamp >= etime && c <= maxiter && (test != mutilate || err != ENOEXEC) &&
                 (test != deleted || err != ENOENT) && (test != endtest || err != ESHUTDOWN));

        prevstamp = engine->getConfigModTime();

        // These should still be the same even after (possibly) waiting
        if (test < initialize && engine->producers().size() != 0)
          adderror((std::string) "non-zero producer list before init (was " +
                   std::to_string(engine->producers().size()) + ")");

        // Check that the number of producers match
        if (test >= initialize && engine->producers().size() != configs.size())
          adderror((std::string) "producer list size " +
                   std::to_string(engine->producers().size()) +
                   " different than what was expected " + std::to_string(configs.size()));

        // Check some other specific producers
        if (test >= addproducer)
        {
          ProducerConfig testcfg = engine->getProducerConfig(confX.producer);

          if (configToStr(testcfg).length() < 1)
            adderror((std::string) "Producer config for " + confX.producer +
                     " appears not loaded correctly");
        }

        // Check some error codes
        if (test == endtest && err != ESHUTDOWN)
          adderror((std::string) "errno should be ESHUTDOWN but is " + std::strerror(err));
        else if (test == deleted && err != ENOENT)
          adderror((std::string) "errno should be ENOENT but is " + std::strerror(err));
        else if (test == mutilate && err != ENOEXEC)
          adderror((std::string) "errno should be NOEXEC but is " + std::strerror(err));
        else if (test < initialize && err != EINPROGRESS)
          adderror((std::string) "errno should be EINPROGRESS but is " + std::to_string(err));
        else if (err != 0 && test != deleted && test != mutilate && test != create &&
                 test != endtest)
          adderror((std::string) "errno is \'" + std::strerror(err) + "\' but should be ok ");

        cout << endl;  // End of test
      }
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

  if (errors.size() > 0)
  {
    cerr << endl << errors.size() << " tests failed:" << endl;
    for (testerror e : errors)
    {
      cerr << "  "
           << "Test " << e.step << "(" << __FILE__ << ":" << e.line << "): " << e.err << endl;
    }
    exit(errors.size());
  }
  // Should we remove the config ???
  boost::filesystem::remove(configfile);
  cout << endl << "All tests ok." << endl;
  if (engine != nullptr)
    delete engine;
  // exit(0);
  return 0;  // Currently this will segfault
}
