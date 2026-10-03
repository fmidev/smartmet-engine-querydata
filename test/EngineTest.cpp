// ======================================================================
/*!
 * \brief Regression tests for the querydata engine
 *
 * The expected values have been cross-checked against the timeseries
 * plugin tests, which use the same test data.
 */
// ======================================================================

#include "Engine.h"
#include "Q.h"
#include "Range.h"
#include <fmt/format.h>
#include <macgyver/DateTime.h>
#include <newbase/NFmiMetTime.h>
#include <newbase/NFmiPoint.h>
#include <regression/tframe.h>
#include <spine/Options.h>
#include <spine/Reactor.h>
#include <cmath>
#include <iostream>

using namespace std;
using SmartMet::Engine::Querydata::OriginTime;
using SmartMet::Engine::Querydata::Q;

std::shared_ptr<SmartMet::Engine::Querydata::Engine> qengine;

namespace Tests
{
// Helsinki as returned by the geonames engine in the timeseries tests
const NFmiPoint helsinki(24.9354496, 60.1695213);

bool close_enough(double value, double expected, double tolerance)
{
  return std::abs(value - expected) <= tolerance;
}

NFmiMetTime utc(int year, int month, int day, int hour, int minute = 0)
{
  return NFmiMetTime(year, month, day, hour, minute, 0, 1);
}

// ----------------------------------------------------------------------

void range()
{
  SmartMet::Engine::Querydata::Range r(5, 1);
  if (r.getMin() != 1 || r.getMax() != 5)
    TEST_FAILED("Range(5,1) should be [1,5]");

  r.set(-2, -7);
  if (r.getMin() != -7 || r.getMax() != -2)
    TEST_FAILED("set(-2,-7) should give [-7,-2]");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void producers()
{
  const auto& list = qengine->producers();
  const std::list<std::string> expected{"pal_skandinavia",
                                        "tutka_suomi_rr",
                                        "ecmwf_eurooppa_pinta",
                                        "ecmwf_maailma_piste",
                                        "ecmwf_skandinavia_painepinta"};
  if (list != expected)
    TEST_FAILED("Producer list is not in the configured order");

  if (!qengine->hasProducer("pal_skandinavia"))
    TEST_FAILED("pal_skandinavia should be available");
  if (qengine->hasProducer("no_such_producer"))
    TEST_FAILED("no_such_producer should not be available");

  const auto& config = qengine->getProducerConfig("tutka_suomi_rr");
  if (!config.ismultifile)
    TEST_FAILED("tutka_suomi_rr should be a multifile producer");
  if (config.number_to_keep != 8)
    TEST_FAILED("tutka_suomi_rr should keep 8 files, not " +
                std::to_string(config.number_to_keep));
  if (config.aliases.count("tutka") == 0)
    TEST_FAILED("tutka_suomi_rr should have the alias 'tutka'");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void find()
{
  // The first producer covering the point is selected
  auto p = qengine->find(24.94, 60.17);
  if (p != "pal_skandinavia")
    TEST_FAILED("Helsinki should be covered by pal_skandinavia, got '" + p + "'");

  // London is outside pal_skandinavia and the radar
  p = qengine->find(-0.1, 51.5);
  if (p != "ecmwf_eurooppa_pinta")
    TEST_FAILED("London should be covered by ecmwf_eurooppa_pinta, got '" + p + "'");

  // Tokyo is only in the point data
  p = qengine->find(139.6917, 35.6895);
  if (p != "ecmwf_maailma_piste")
    TEST_FAILED("Tokyo should be covered by ecmwf_maailma_piste, got '" + p + "'");

  // Level type restricts the choice
  p = qengine->find(24.94, 60.17, 60, true, "pressure");
  if (p != "ecmwf_skandinavia_painepinta")
    TEST_FAILED("Pressure level data for Helsinki should be ecmwf_skandinavia_painepinta, got '" +
                p + "'");

  // A restricted producer list
  p = qengine->find({"ecmwf_eurooppa_pinta", "pal_skandinavia"}, 24.94, 60.17);
  if (p != "ecmwf_eurooppa_pinta")
    TEST_FAILED("The order of the given producer list should be obeyed, got '" + p + "'");

  // Nothing covers the middle of the Pacific
  p = qengine->find(-150, 0);
  if (!p.empty())
    TEST_FAILED("Nothing should cover 150W,0N, got '" + p + "'");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void origintimes()
{
  auto times = qengine->origintimes("pal_skandinavia");
  if (times.size() != 1)
    TEST_FAILED("Expected one pal_skandinavia origin time, got " + std::to_string(times.size()));

  auto q1 = qengine->get("pal_skandinavia");
  auto q2 = qengine->get("pal_skandinavia", *times.begin());
  if (SmartMet::Engine::Querydata::hash_value(q1) != SmartMet::Engine::Querydata::hash_value(q2))
    TEST_FAILED("Latest data and data for the only origin time should be the same");

  try
  {
    qengine->get("pal_skandinavia", OriginTime(Fmi::Date(2000, 1, 1), Fmi::Hours(0)));
    TEST_FAILED("Requesting a missing origin time should throw");
  }
  catch (...)
  {
  }

  try
  {
    qengine->get("no_such_producer");
    TEST_FAILED("Requesting an unknown producer should throw");
  }
  catch (...)
  {
  }

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void timeperiod()
{
  auto period = qengine->getProducerTimePeriod("pal_skandinavia");
  const Fmi::DateTime first(Fmi::Date(2008, 8, 5), Fmi::Hours(3));
  const Fmi::DateTime last(Fmi::Date(2008, 8, 9), Fmi::Hours(6));

  if (period.begin() != first)
    TEST_FAILED("pal_skandinavia should start at " + Fmi::date_time::to_iso_string(first) + ", not " +
                Fmi::date_time::to_iso_string(period.begin()));
  // The period is half open, end() is the last valid time
  if (period.end() != last)
    TEST_FAILED("pal_skandinavia should end at " + Fmi::date_time::to_iso_string(last) + ", not " +
                Fmi::date_time::to_iso_string(period.end()));

  auto q = qengine->get("pal_skandinavia");
  if (q->validTimes()->size() != 100)
    TEST_FAILED("pal_skandinavia should have 100 hourly times, got " +
                std::to_string(q->validTimes()->size()));

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void interpolation()
{
  auto q = qengine->get("pal_skandinavia");
  if (!q->param(kFmiTemperature))
    TEST_FAILED("pal_skandinavia should have Temperature");

  // Exact timestep. timeseries: Helsinki 2008-08-05 12:00 local = 14.9011659622192383
  auto value = q->interpolate(helsinki, utc(2008, 8, 5, 9));
  if (!close_enough(value, 14.90117, 0.0001))
    TEST_FAILED(fmt::format("Helsinki 09 UTC temperature should be 14.90117, got {}", value));

  // Time interpolation. timeseries: 12:15 local = 14.8036565780639648
  value = q->interpolate(helsinki, utc(2008, 8, 5, 9, 15));
  if (!close_enough(value, 14.80366, 0.0001))
    TEST_FAILED(fmt::format("Helsinki 09:15 UTC temperature should be 14.80366, got {}", value));

  // Outside the data period
  value = q->interpolate(helsinki, utc(2008, 8, 20, 0));
  if (value != kFloatMissing)
    TEST_FAILED(fmt::format("A time after the data should be missing, got {}", value));

  // Outside the data area
  value = q->interpolate(NFmiPoint(-150, 0), utc(2008, 8, 5, 9));
  if (value != kFloatMissing)
    TEST_FAILED(fmt::format("A point outside the area should be missing, got {}", value));

  if (!q->isInside(24.94, 60.17, 0))
    TEST_FAILED("Helsinki should be inside pal_skandinavia");
  if (q->isInside(-150, 0, 0))
    TEST_FAILED("150W,0N should not be inside pal_skandinavia");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void pressurelevels()
{
  auto q = qengine->get("ecmwf_skandinavia_painepinta");
  if (!q->param(kFmiTemperature))
    TEST_FAILED("Pressure level data should have Temperature");

  // timeseries: Helsinki 850 hPa 2008-09-09 12:00 local = 4.9, 500 hPa = -13.1
  if (!q->selectLevel(850))
    TEST_FAILED("Failed to select the 850 hPa level");
  auto value = q->interpolate(helsinki, utc(2008, 9, 9, 9));
  if (!close_enough(value, 4.9, 0.05))
    TEST_FAILED(fmt::format("850 hPa temperature should be 4.9, got {}", value));

  if (!q->selectLevel(500))
    TEST_FAILED("Failed to select the 500 hPa level");
  value = q->interpolate(helsinki, utc(2008, 9, 9, 9));
  if (!close_enough(value, -13.1, 0.05))
    TEST_FAILED(fmt::format("500 hPa temperature should be -13.1, got {}", value));

  if (q->selectLevel(123))
    TEST_FAILED("Selecting a nonexistent level should fail");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void multifile()
{
  auto q = qengine->get("tutka_suomi_rr");
  auto times = q->validTimes();
  if (times->size() != 8)
    TEST_FAILED("Radar multifile should have 8 times, got " + std::to_string(times->size()));

  const Fmi::DateTime first(Fmi::Date(2013, 9, 10), Fmi::Hours(10));
  if (*times->begin() != first)
    TEST_FAILED("The first radar time should be " + Fmi::date_time::to_iso_string(first) + ", not " +
                Fmi::date_time::to_iso_string(*times->begin()));

  // A subperiod of the multifile
  Fmi::TimePeriod period(Fmi::DateTime(Fmi::Date(2013, 9, 10), Fmi::Minutes(600)),
                         Fmi::DateTime(Fmi::Date(2013, 9, 10), Fmi::Minutes(610)));
  auto qsub = qengine->get("tutka_suomi_rr", period);
  auto subtimes = qsub->validTimes();
  if (subtimes->size() < 2 || subtimes->size() > times->size())
    TEST_FAILED("A 10 minute radar period should have at least 2 times, got " +
                std::to_string(subtimes->size()));

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void hashes()
{
  auto q = qengine->get("pal_skandinavia");
  auto hash = SmartMet::Engine::Querydata::hash_value(q);
  if (qengine->getModelHashValue("pal_skandinavia").hash != hash)
    TEST_FAILED("getModelHashValue should return the same hash as hash_value(get())");

  auto q2 = qengine->get("ecmwf_eurooppa_pinta");
  if (SmartMet::Engine::Querydata::hash_value(q2) == hash)
    TEST_FAILED("Different models should have different hash values");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void grids()
{
  auto q = qengine->get("pal_skandinavia");
  auto coords = qengine->getWorldCoordinates(q);
  if (!coords)
    TEST_FAILED("No coordinates for pal_skandinavia");

  const auto nx = q->grid().XNumber();
  const auto ny = q->grid().YNumber();
  if (coords->width() != nx || coords->height() != ny)
    TEST_FAILED(fmt::format("Coordinate matrix should be {}x{}, got {}x{}",
                            nx,
                            ny,
                            coords->width(),
                            coords->height()));

  q->param(kFmiTemperature);
  auto values =
      qengine->getValues(q, 1234, Fmi::DateTime(Fmi::Date(2008, 8, 5), Fmi::Hours(9)));
  if (!values)
    TEST_FAILED("No values for pal_skandinavia temperature");
  if (values->NX() != nx || values->NY() != ny)
    TEST_FAILED(fmt::format(
        "Value matrix should be {}x{}, got {}x{}", nx, ny, values->NX(), values->NY()));

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void metadata()
{
  auto list = qengine->getEngineMetadata();
  // One entry per loaded file: 1 + 8 radar files + 1 + 1 + 1
  if (list.size() != 12)
    TEST_FAILED("Expected metadata for 12 models, got " + std::to_string(list.size()));

  for (const auto& meta : list)
  {
    if (meta.producer != "pal_skandinavia")
      continue;
    if (meta.timeStep != 60)
      TEST_FAILED("pal_skandinavia timestep should be 60 minutes, got " +
                  std::to_string(meta.timeStep));
    if (meta.nTimeSteps != 100)
      TEST_FAILED("pal_skandinavia should have 100 timesteps, got " +
                  std::to_string(meta.nTimeSteps));
    TEST_PASSED();
  }
  TEST_FAILED("No metadata for pal_skandinavia");
}

// ----------------------------------------------------------------------

class tests : public tframe::tests
{
  const char* error_message_prefix() const override { return "\n\t"; }
  void test() override
  {
    TEST(range);
    TEST(producers);
    TEST(find);
    TEST(origintimes);
    TEST(timeperiod);
    TEST(interpolation);
    TEST(pressurelevels);
    TEST(multifile);
    TEST(hashes);
    TEST(grids);
    TEST(metadata);
  }
};

}  // namespace Tests

int main()
{
  SmartMet::Spine::Options opts;
  opts.configfile = "cnf/reactor.conf";
  opts.parseConfig();

  SmartMet::Spine::Reactor reactor(opts);
  reactor.init();
  qengine = reactor.getEngine<SmartMet::Engine::Querydata::Engine>("Querydata", nullptr);

  cout << endl << "Querydata engine tester" << endl << "=======================" << endl;
  Tests::tests t;
  auto result = t.run();
  qengine.reset();
  reactor.shutdown();
  return result;
}
