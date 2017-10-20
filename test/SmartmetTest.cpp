// ======================================================================
/*!
 * \brief Sample program showing how QEngine works
 *
 */
// ======================================================================

#include "Engine.h"
#include <iostream>
#include <string>

#include <libconfig.h++>

extern "C" {
#include <signal.h>
}

using namespace std;

const string configfile = "cnf/smartmet.conf";

const int timeout = 60 * 60 * 100;  // 100 hours

void sighandler(int)
{
  cout << "exiting after signal catch" << endl;
  exit(0);
}

int main()
{
  try
  {
    signal(SIGINT, &sighandler);
    signal(SIGQUIT, &sighandler);
    signal(SIGHUP, &sighandler);
    signal(SIGABRT, &sighandler);
    signal(SIGTERM, &sighandler);

    cout << endl << "\tThis program will monitor test querydata." << endl
         << "\tYou can abort the program by pressing Ctrl-C." << endl << endl << "\tThere are "
         << timeout << " seconds before the program will" << endl << "\texit automatically." << endl
         << endl;

    // We'll run the method in a thread. If the user does not interrupt
    // the program fast enough, we'll abort

    SmartMet::Engine::Querydata::Engine engine(configfile);

    sleep(timeout);

    cout << endl << "Ending the program after a " << timeout << " second timeout" << endl;
  }
  catch (libconfig::ParseException& e)
  {
    std::cerr << std::endl << "Parse error on line " << e.getLine() << " of '" << configfile
              << "' : '" << e.getError() << "'" << std::endl;
    return 1;
  }
  catch (std::exception& e)
  {
    cerr << "Exception: " << e.what();
    return 1;
  }

  return 0;
}
