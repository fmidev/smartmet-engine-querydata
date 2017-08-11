/*
 * StackAllocationTest.h
 *
 *  Created on: Aug 11, 2017
 *
 *  A test that effectively does nothing.
 *  It merely demonstrates a seg fault bug which occurs on current versions as of Aug 2017 when
 *  Querydata engine structures are allocated on stack.
 *  Hopefully, it will go away in the future.
 */

#include "Engine.h"
#include <iostream>
#include <string>

extern "C" {
#include <signal.h>
}

using namespace std;

const string configfile = "querydata.conf";

const int timeout = 5 ;

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

    cout << endl
         << "\tThere are " << timeout << " seconds before the program will" << endl
         << "\texit automatically." << endl
         << endl;

    // We'll run the method in a thread. If the user does not interrupt
    // the program fast enough, we'll abort

    SmartMet::Engine::Querydata::Engine engine(configfile);

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
  catch (std::exception& e)
  {
    cerr << "Exception: " << e.what();
    return 1;
  }

  return 0;
}




