// ======================================================================
/*!
 * \brief Model repository
 */
// ======================================================================

#include "Repository.h"
#include "MetaQueryFilters.h"

#include <spine/Exception.h>
#include <spine/TableFormatter.h>
#include <spine/ParameterFactory.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiQueryData.h>
#include <macgyver/String.h>
#include <boost/foreach.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <boost/algorithm/string/join.hpp>
#include <stdexcept>
#include <sstream>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
// ----------------------------------------------------------------------
/*!
 * \brief Repository constructor
 *
 * We just initialize the map of models to be empty
 */
// ----------------------------------------------------------------------

Repository::Repository() : itsProducers()
{
}
// ----------------------------------------------------------------------
/*!
 * \brief Add a new producer configuration
 */
// ----------------------------------------------------------------------

void Repository::add(const ProducerConfig& config)
{
  try
  {
    itsProducerConfigs.insert(ProducerConfigs::value_type(config.producer, config));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Add a new model for the given producer
 */
// ----------------------------------------------------------------------

void Repository::add(const Producer& producer, SharedModel model)
{
  try
  {
    Producers::iterator producer_model = itsProducers.find(producer);

    // Establish the model map for the producer

    bool ok;
    if (producer_model == itsProducers.end())
    {
      // Insert an empty map of models for a new producer
      boost::tie(producer_model, ok) =
          itsProducers.insert(Producers::value_type(producer, SharedModels()));

      if (!ok)
        throw SmartMet::Spine::Exception(
            BCP, "Failed to add new model for producer '" + producer + "'!");
    }

    // And insert the model for the producer

    SharedModels& models = producer_model->second;

    SharedModels::iterator iter;

    boost::tie(iter, ok) = models.insert(std::make_pair(model->originTime(), model));

    if (!ok)
    {
      // The origin time must have matched. Choose the data with the
      // newer modification time.

      if (model->modificationTime() > iter->second->modificationTime())
      {
#if 0
        std::cerr << "QEngine: Replacing "
            << iter->second->path()
            << " by "
            << model->path()
            << " due to later modification time" << std::endl;
#endif
        // Replace old data with new one
        iter->second = model;
      }
      else
      {
        // This could happen for example during start up when newer data is read
        // before older data - we just ignore the old data
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get available origin times for the producer
 */
// ----------------------------------------------------------------------

OriginTimes Repository::originTimes(const Producer& producer) const
{
  try
  {
    OriginTimes times;

    const auto it = itsProducers.find(producer);
    if (it != itsProducers.end())
    {
      const SharedModels& models = it->second;

      BOOST_FOREACH (const auto& time_model, models)
      {
        times.insert(time_model.first);
      }
    }
    return times;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get newest data for the given producer
 */
// ----------------------------------------------------------------------

Q Repository::get(const Producer& producer) const
{
  try
  {
    // If the data is multifile return all of them instead of just the latest file

    ProducerConfigs::const_iterator prod_config = itsProducerConfigs.find(producer);
    if (prod_config != itsProducerConfigs.end())
    {
      if (prod_config->second.ismultifile)
        return getAll(producer);
    }

    // Return the latest model only

    Producers::const_iterator producer_model = itsProducers.find(producer);

    if (producer_model == itsProducers.end())
      throw SmartMet::Spine::Exception(
          BCP, "Repository get (1): No data available for producer '" + producer + "'!");

    const SharedModels& time_model = producer_model->second;

    if (time_model.empty())
      throw SmartMet::Spine::Exception(
          BCP, "Repository get (2): No data available for producer '" + producer + "'!");

    // newest origintime is at the end
    auto last = --time_model.end();

    return boost::make_shared<QImpl>(last->second);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get data for the given producer with given origintime
 */
// ----------------------------------------------------------------------

Q Repository::get(const Producer& producer, const OriginTime& origintime) const
{
  try
  {
    Producers::const_iterator producer_model = itsProducers.find(producer);

    if (producer_model == itsProducers.end())
      throw SmartMet::Spine::Exception(
          BCP, "Repository get (3): No data available for producer '" + producer + "'!");

    const SharedModels& models = producer_model->second;

    if (models.empty())
      throw SmartMet::Spine::Exception(
          BCP, "Repository get (4): No data available for producer '" + producer + "'!");

    SharedModels::const_iterator time_model;
    if (origintime.is_pos_infinity())
    {
      // newest origintime is at the end
      auto time_model = --models.end();
      return boost::make_shared<QImpl>(time_model->second);
    }
    if (origintime.is_neg_infinity())
    {
      // oldest origintime is at the beginning
      auto time_model = models.begin();
      return boost::make_shared<QImpl>(time_model->second);
    }

#if 1
    auto iter = models.find(origintime);
    if (iter != models.end())
      return boost::make_shared<QImpl>(iter->second);
#else
    // This was deprecated 27.4.2015 in favour of exact origintime requests

    // Find latest model with origintime <= given limit. This is potentially
    // slow if one is searching for very old radar data, but then again
    // one should use the radar data as a multifile instead
    for (auto iter = models.crbegin(); iter != models.crend(); ++iter)
      if (iter->first <= origintime)
        return boost::make_shared<QImpl>(iter->second);

#endif

    throw SmartMet::Spine::Exception(BCP,
                                     "Repository get: No data available for producer '" + producer +
                                         "' with origintime == " + to_simple_string(origintime));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get all data for the given time period
 */
// ----------------------------------------------------------------------

Q Repository::getAll(const Producer& producer) const
{
  try
  {
    // Find the models

    Producers::const_iterator producer_model = itsProducers.find(producer);

    if (producer_model == itsProducers.end())
      throw SmartMet::Spine::Exception(
          BCP, "Repository getPeriod: No data available for producer '" + producer + "'");

    const SharedModels& models = producer_model->second;

    if (models.empty())
      throw SmartMet::Spine::Exception(
          BCP, "Repository getPeriod: No data available for producer '" + producer + "'");

    // Construct a vector of datas

    std::vector<SharedModel> okmodels;
    BOOST_FOREACH (const auto& otime_model, models)
    {
      okmodels.push_back(otime_model.second);
    }

    // Construct a view of the data

    return boost::make_shared<QImpl>(okmodels);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Remove the specified model
 */
// ----------------------------------------------------------------------

void Repository::remove(const Producer& producer, const boost::filesystem::path& path)
{
  try
  {
    Producers::iterator producer_model = itsProducers.find(producer);

    if (producer_model == itsProducers.end())
      throw SmartMet::Spine::Exception(
          BCP, "Repository remove: No data available for producer '" + producer + "'");

    SharedModels& models = producer_model->second;

    if (models.empty())
      throw SmartMet::Spine::Exception(
          BCP, "Repository remove: No data available for producer '" + producer + "'");

    for (SharedModels::iterator time_model = models.begin(), end = models.end(); time_model != end;
         ++time_model)
    {
      if (time_model->second->path() == path)
      {
#ifdef MYDEBUG
        std::cout << boost::posix_time::second_clock::local_time() << " Unloading "
                  << time_model->second.path() << std::endl;
#endif
        models.erase(time_model);
        break;
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Resize producer
 */
// ----------------------------------------------------------------------

void Repository::resize(const Producer& producer, std::size_t limit)
{
  try
  {
    Producers::iterator producer_model = itsProducers.find(producer);

    if (producer_model == itsProducers.end())
      throw SmartMet::Spine::Exception(
          BCP, "Repository resize: no data available for producer '" + producer + "'");

    SharedModels& models = producer_model->second;

    // Usually only the oldest (1) file is deleted, so
    // the speed of this erase loop is of no concern.
    // Typically we load a new file, and then delete
    // the oldest one. During the startup phase we may
    // have multiple deletions, but that is to be expected,
    // since we have to scan all the files for their origintimes.

    while (models.size() > limit)
    {
#ifdef MYDEBUG
      std::cout << boost::posix_time::second_clock::local_time() << " Unloading old file "
                << models.begin()->second.path() << std::endl;
#endif
      // the oldest file is the one first sorted by origintime
      models.erase(models.begin());
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Match leveltypes
 *
 * Leveltype is OK if desired type is the same, or the desired
 * type is "" implying first match is OK.
 *
 */
// ----------------------------------------------------------------------

bool leveltype_ok(const std::string& modeltype, const std::string& wantedtype)
{
  try
  {
    if (wantedtype.empty())
      return true;
    return (modeltype == wantedtype);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Utility subroutine
 */
// ----------------------------------------------------------------------

bool Repository::contains(const Repository::SharedModels& models,
                          double lon,
                          double lat,
                          double maxdist,
                          const std::string& levelname) const
{
  try
  {
    if (models.empty())
      return false;

    SharedModel smodel = (--models.end())->second;
    const Model& model = *smodel;

    if (!leveltype_ok(model.levelName(), levelname))
      return false;

    auto qinfo = model.info();
    bool result = qinfo->IsInside(NFmiPoint(lon, lat), 1000 * maxdist);
    model.release(qinfo);

    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Find the model containing the given point
 */
// ----------------------------------------------------------------------

Producer Repository::find(const ProducerList& producerlist,
                          const ProducerList& producerorder,
                          double lon,
                          double lat,
                          double maxdist,
                          bool usedatamaxdist,
                          const std::string& leveltype) const
{
  try
  {
    BOOST_FOREACH (const Producer& producer, producerlist)
    {
      // Try primary names first
      Producers::const_iterator producer_model = itsProducers.find(producer);

      if (producer_model != itsProducers.end())
      {
        ProducerConfigs::const_iterator prod_config = itsProducerConfigs.find(producer);
        // Use data maxdistance if allowed and it is set
        double chosenmaxdist =
            (usedatamaxdist && prod_config->second.maxdistance > 0 ? prod_config->second.maxdistance
                                                                   : maxdist);

        if (contains(producer_model->second, lon, lat, chosenmaxdist, leveltype))
          return producer;
      }
    }

    // Try aliases next in the given order

    BOOST_FOREACH (const Producer& producer, producerorder)
    {
      ProducerConfigs::const_iterator prod_config = itsProducerConfigs.find(producer);
      if (prod_config == itsProducerConfigs.end())
        continue;

      // Use data maxdistance if allowed and it is set
      double chosenmaxdist =
          (usedatamaxdist && prod_config->second.maxdistance > 0 ? prod_config->second.maxdistance
                                                                 : maxdist);

      const std::set<std::string>& aliases = prod_config->second.aliases;

      BOOST_FOREACH (const Producer& alias, producerlist)
      {
        if (aliases.find(alias) != aliases.end())
        {
          Producers::const_iterator producer_model = itsProducers.find(producer);
          if (producer_model != itsProducers.end())
          {
            if (contains(producer_model->second, lon, lat, chosenmaxdist, leveltype))
              return producer;
          }
        }
      }
    }

    return Producer();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

Repository::ContentTable Repository::getRepoContents(const std::string& timeFormat,
                                                     const std::string& projectionFormat) const
{
  try
  {
    static const std::vector<std::string>& ContentTableHeaders =
        *new std::vector<std::string>{"Producer",
                                      "Aliases",
                                      "RI",
                                      "Path",
                                      "Parameters",
                                      "Projection",
                                      "OriginTime",
                                      "MinTime",
                                      "MaxTime"};

    boost::scoped_ptr<Fmi::TimeFormatter> timeFormatter(Fmi::TimeFormatter::create(timeFormat));

    boost::shared_ptr<SmartMet::Spine::Table> resultTable(new SmartMet::Spine::Table);

    int row = 0;

    for (auto prodit = itsProducers.begin(); prodit != itsProducers.end(); ++prodit)
    {
      const SharedModels& theseModels = prodit->second;

      const ProducerConfig thisConfig = itsProducerConfigs.find(prodit->first)->second;

      for (auto modit = theseModels.begin(); modit != theseModels.end(); ++modit)
      {
        auto model = modit->second;
        auto qi = model->info();

        // Time range
        qi->FirstTime();
        boost::posix_time::ptime time1 = qi->ValidTime();
        qi->LastTime();
        boost::posix_time::ptime time2 = qi->ValidTime();

        // Get the parameter list from querydatainfo
        std::list<std::string> params;
        for (qi->ResetParam(); qi->NextParam(false);)
        {
          int paramID = boost::numeric_cast<int>(qi->Param().GetParamIdent());
          const std::string paramName = SmartMet::Spine::ParameterFactory::instance().name(paramID);
          params.push_back(std::string(paramName));
        }

        // Get projection string
        std::string projectionText;
        if (qi->Area() == NULL)
        {
          projectionText = "nan";
        }
        else if (projectionFormat == "wkt")
        {
          projectionText = qi->Area()->WKT();
        }
        // Defaults to newbase form
        else
        {
          projectionText = qi->Area()->AreaStr();
        }

        int column = 0;

        // insert producer name
        resultTable->set(column, row, prodit->first);
        ++column;

        // Insert aliases
        std::string aliases = boost::algorithm::join(thisConfig.aliases, ", ");
        resultTable->set(column, row, aliases);
        ++column;

        // Insert refresh interval
        resultTable->set(column, row, Fmi::to_string(thisConfig.refresh_interval_secs));
        ++column;

        // insert model file path
        resultTable->set(column, row, modit->second->path().string());
        ++column;

        // Insert parameters
        std::string parameters = boost::algorithm::join(params, ", ");
        resultTable->set(column, row, parameters);
        ++column;

        // Insert projection string
        resultTable->set(column, row, projectionText);
        ++column;

        // insert origin time
        resultTable->set(column, row, timeFormatter->format(modit->first));
        ++column;

        // Insert min time
        resultTable->set(column, row, timeFormatter->format(time1));
        ++column;

        // Insert max time
        resultTable->set(column, row, timeFormatter->format(time2));
        ++column;

        ++row;
      }
    }

    // Insert headers

    SmartMet::Spine::TableFormatter::Names headers;

    BOOST_FOREACH (const auto& p, ContentTableHeaders)
    {
      headers.push_back(p);
    }

    return std::make_pair(resultTable, headers);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::list<MetaData> Repository::getRepoMetadata(const MetaQueryOptions& theOptions) const
{
  try
  {
    // Get initial list of forecast metadata
    auto propertyList = getRepoMetadata();

    // Filter according to the given options
    for (auto iter = propertyList.begin(); iter != propertyList.end();)
    {
      if (!filterProducer(*iter, theOptions))
      {
        propertyList.erase(iter++);
        continue;
      }
      if (!filterOriginTime(*iter, theOptions))
      {
        propertyList.erase(iter++);
        continue;
      }

      if (!filterFirstTime(*iter, theOptions))
      {
        propertyList.erase(iter++);
        continue;
      }

      if (!filterLastTime(*iter, theOptions))
      {
        propertyList.erase(iter++);
        continue;
      }

      if (!filterParameters(*iter, theOptions))
      {
        propertyList.erase(iter++);
        continue;
      }

      if (!filterLevelTypes(*iter, theOptions))
      {
        propertyList.erase(iter++);
        continue;
      }

      if (!filterLevelValues(*iter, theOptions))
      {
        propertyList.erase(iter++);
        continue;
      }

      if (!filterBoundingBox(*iter, theOptions))
      {
        propertyList.erase(iter++);
        continue;
      }

      ++iter;
    }

    return propertyList;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::list<MetaData> Repository::getRepoMetadata() const
{
  try
  {
    std::list<MetaData> props;

    for (auto prodit = itsProducers.begin(); prodit != itsProducers.end(); ++prodit)
    {
      const SharedModels& theseModels = prodit->second;

      const ProducerConfig thisConfig = itsProducerConfigs.find(prodit->first)->second;

      for (auto modit = theseModels.begin(); modit != theseModels.end(); ++modit)
      {
        QImpl q(modit->second);
        props.push_back(q.metaData());
      }
    }

    return props;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

Repository::MetaObject Repository::getSynchroInfos() const
{
  try
  {
    Repository::MetaObject props;

    for (auto prodit = itsProducers.begin(); prodit != itsProducers.end(); ++prodit)
    {
      const SharedModels& theseModels = prodit->second;

      const ProducerConfig thisConfig = itsProducerConfigs.find(prodit->first)->second;

      std::vector<bp::ptime> originTimes;

      for (auto modit = theseModels.begin(); modit != theseModels.end(); ++modit)
      {
        // Get querydata origintime

        originTimes.push_back(modit->first);
      }

      props.insert(std::make_pair(thisConfig.producer, originTimes));
    }

    return props;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

Repository::SharedModels Repository::getAllModels(const Producer& producer) const
{
  try
  {
    Producers::const_iterator producer_model = itsProducers.find(producer);

    if (producer_model == itsProducers.end())
      return SharedModels();

    return producer_model->second;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Q
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
