#define BOOST_TEST_MODULE RepositoryTest
#include <boost/test/included/unit_test.hpp>

#include "Repository.h"

using namespace SmartMet::Engine::Querydata;

namespace
{
const char* const datafile =
    "/usr/share/smartmet/test/data/pal/200808050933_pal_skandinavia_pinta.sqd";
const Producer producer = "pal_skandinavia";

SharedModel load_model()
{
  return Model::create(datafile, producer, "surface", false, true, false, false, 3600, 600, true);
}
}  // namespace

// A producer whose models were all expired by max_age must accept the
// deletion of their files. Throwing here made the directory monitor retry
// the same failing change set forever, so new files were never loaded.

BOOST_AUTO_TEST_CASE(remove_after_expire)
{
  Repository repo;
  repo.add(producer, load_model());
  BOOST_CHECK_EQUAL(repo.originTimes(producer).size(), 1U);

  // The test data file is years old, a one second max_age expires it
  repo.expire(producer, 1);
  BOOST_CHECK(repo.originTimes(producer).empty());

  BOOST_CHECK_NO_THROW(repo.remove(producer, datafile));

  // and new data is accepted afterwards
  repo.add(producer, load_model());
  BOOST_CHECK_EQUAL(repo.originTimes(producer).size(), 1U);
}

BOOST_AUTO_TEST_CASE(remove_unknown)
{
  Repository repo;
  BOOST_CHECK_NO_THROW(repo.remove("no_such_producer", datafile));

  repo.add(producer, load_model());
  BOOST_CHECK_NO_THROW(repo.remove(producer, "/no/such/file.sqd"));
  BOOST_CHECK_EQUAL(repo.originTimes(producer).size(), 1U);

  repo.remove(producer, datafile);
  BOOST_CHECK(repo.originTimes(producer).empty());
}
