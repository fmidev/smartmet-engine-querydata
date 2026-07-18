// ======================================================================
/*!
 * \brief Standalone test for the radar GeoTIFF reader.
 *
 * Reads the committed synthetic EPSG:3067 radar GeoTIFF fixture
 * (examples/data/202607181200_radar_test_dbz.tif) via readRadarFile() and
 * verifies grid dimensions, valid time, parameter, gain/offset scaling,
 * nodata/undetect handling and the north-up -> bottom-up row flip.
 *
 * Build:  make RadarReaderTest    Run from the examples directory:  ./RadarReaderTest
 */
// ======================================================================

#include "RadarReader.h"

#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiGlobals.h>
#include <newbase/NFmiQueryData.h>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

using namespace SmartMet::Engine::Querydata;

namespace
{
int failures = 0;

void check(bool cond, const std::string& msg)
{
  if (!cond)
  {
    std::cerr << "  FAIL: " << msg << "\n";
    ++failures;
  }
}

bool close(float a, float b)
{
  return std::fabs(a - b) < 1e-4;
}
}  // namespace

int main(int argc, char** argv)
{
  const std::string path =
      (argc > 1) ? argv[1] : "data/202607181200_radar_test_dbz.tif";

  std::shared_ptr<NFmiQueryData> data;
  try
  {
    data = readRadarFile(path);
  }
  catch (const std::exception& e)
  {
    std::cerr << "readRadarFile threw: " << e.what() << "\n";
    return 1;
  }
  check(static_cast<bool>(data), "readRadarFile returned data");
  if (!data)
    return 1;

  NFmiFastQueryInfo info(data.get());

  // Grid dimensions
  check(info.Grid() != nullptr, "has grid");
  if (info.Grid() != nullptr)
    check(info.Grid()->XNumber() == 4 && info.Grid()->YNumber() == 3, "grid is 4x3");

  info.FirstParam();
  info.FirstLevel();
  info.FirstTime();

  // Parameter (Quantity "Corrected reflectivity" -> kFmiReflectivity)
  check(info.Param().GetParam()->GetIdent() == kFmiReflectivity, "parameter is reflectivity");

  // Valid time from the filename / metadata
  const NFmiMetTime& vt = info.ValidTime();
  check(vt.GetYear() == 2026 && vt.GetMonth() == 7 && vt.GetDay() == 18 && vt.GetHour() == 12 &&
            vt.GetMin() == 0,
        "valid time is 2026-07-18 12:00 UTC");

  const unsigned long nx = 4;
  auto val = [&](unsigned long i, unsigned long j) -> float
  {
    info.LocationIndex(j * nx + i);
    return info.FloatValue();
  };

  // Fixture raw (north row first): row0 [0(undetect),1,2,3], row1 [10,11,12,13],
  // row2 [20,21,22,255(nodata)]. gain=0.5, offset=-32. newbase j=0 is south (=row2).
  check(close(val(0, 0), -22.0f), "south-west (0,0) = 0.5*20-32 = -22");
  check(close(val(1, 0), -21.5f), "(1,0) = 0.5*21-32 = -21.5");
  check(val(3, 0) == kFloatMissing, "(3,0) nodata -> kFloatMissing");
  check(close(val(2, 1), -26.0f), "(2,1) = 0.5*12-32 = -26");
  check(close(val(0, 2), -32.0f), "north-west (0,2) undetect -> offset (-32)");
  check(close(val(1, 2), -31.5f), "(1,2) = 0.5*1-32 = -31.5");

  // Orientation: south value must exceed the north value in the same column
  check(val(0, 0) > val(0, 2), "row flip correct (south warmer than north)");

  // Scratch .sqd round-trip: write the decoded querydata and read it back,
  // mirroring the engine's convert-to-scratch + memory-map step.
  {
    const std::string sqd = "radarreadertest_scratch.sqd";
    {
      std::ofstream out(sqd, std::ios::out | std::ios::binary | std::ios::trunc);
      out << *data;
    }
    NFmiQueryData qd2(sqd, false);
    NFmiFastQueryInfo info2(&qd2);
    info2.FirstParam();
    info2.FirstLevel();
    info2.FirstTime();
    auto val2 = [&](unsigned long i, unsigned long j) -> float
    {
      info2.LocationIndex(j * nx + i);
      return info2.FloatValue();
    };
    check(close(val2(0, 0), -22.0f), "scratch round-trip preserves (0,0)");
    check(val2(3, 0) == kFloatMissing, "scratch round-trip preserves nodata");
    check(info2.Grid() != nullptr && info2.Grid()->XNumber() == 4 && info2.Grid()->YNumber() == 3,
          "scratch round-trip preserves grid");
    std::remove(sqd.c_str());
  }

  // --- ODIM HDF5 (Cartesian) path ---
  {
    const std::string opath = (argc > 2) ? argv[2] : "data/radar_test_odim_dbz.h5";
    std::shared_ptr<NFmiQueryData> odim;
    try
    {
      odim = readRadarFile(opath);
    }
    catch (const std::exception& e)
    {
      std::cerr << "  FAIL: readRadarFile(ODIM) threw: " << e.what() << "\n";
      ++failures;
    }
    if (odim)
    {
      NFmiFastQueryInfo oi(odim.get());
      check(oi.Grid() != nullptr && oi.Grid()->XNumber() == 500 && oi.Grid()->YNumber() == 500,
            "ODIM grid is 500x500");
      oi.FirstParam();
      oi.FirstLevel();
      oi.FirstTime();
      check(oi.Param().GetParam()->GetIdent() == kFmiReflectivity,
            "ODIM parameter is reflectivity (TH)");
      const NFmiMetTime& ot = oi.ValidTime();
      check(ot.GetYear() == 2011 && ot.GetMonth() == 9 && ot.GetDay() == 12 && ot.GetHour() == 6,
            "ODIM valid time is 2011-09-12 06:xx UTC");
      int nonmissing = 0;
      for (oi.ResetLocation(); oi.NextLocation();)
        if (oi.FloatValue() != kFloatMissing)
          ++nonmissing;
      check(nonmissing > 0, "ODIM has non-missing values");
    }
  }

  if (failures == 0)
    std::cout << "RadarReaderTest: ALL PASSED\n";
  else
    std::cout << "RadarReaderTest: " << failures << " FAILURE(S)\n";
  return failures == 0 ? 0 : 1;
}

// ======================================================================
