// ======================================================================
/*!
 * \brief Model repository
 */
// ======================================================================

#include "Repository.h"
#include "MetaQueryFilters.h"
#include <boost/algorithm/string/join.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <boost/tuple/tuple.hpp>
#include <macgyver/StringConversion.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiQueryData.h>
#include <spine/Convenience.h>
#include <macgyver/Exception.h>
#include <spine/ParameterFactory.h>
#include <spine/TableFormatter.h>
#include <sstream>
#include <stdexcept>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    auto producer_model = itsProducers.find(producer);

    // Establish the model map for the producer

    bool ok;
    if (producer_model == itsProducers.end())
    {
      // Insert an empty map of models for a new producer
      boost::tie(producer_model, ok) =
          itsProducers.insert(Producers::value_type(producer, SharedModels()));

      if (!ok)
        throw Fmi::Exception(BCP, "Failed to add new model for producer '" + producer + "'!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

      for (const auto& time_model : models)
      {
        times.insert(time_model.first);
      }
    }
    return times;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the producer name is known as done in get()
 */
// ----------------------------------------------------------------------

bool Repository::hasProducer(const Producer& producer) const
{
  try
  {
    const auto pos = itsProducerConfigs.find(producer);
    if (pos != itsProducerConfigs.end())
      return true;

    // Check aliases

    for (const auto& prod_config : itsProducerConfigs)
    {
      const auto& config = prod_config.second;
      const auto& aliases = config.aliases;

      if (aliases.find(producer) != aliases.end())
        return true;
    }

    return false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

    const auto prod_config = itsProducerConfigs.find(producer);
    if (prod_config != itsProducerConfigs.end())
    {
      if (prod_config->second.ismultifile)
        return getAll(producer);
    }

    // Return the latest model only

    const auto producer_model = itsProducers.find(producer);

    if (producer_model == itsProducers.end())
    {
      throw Fmi::Exception(
          BCP, "Repository get (1): No data available for producer '" + producer + "'!")
          .disableStackTrace();
    }

    const SharedModels& time_model = producer_model->second;

    if (time_model.empty())
    {
      throw Fmi::Exception(
          BCP, "Repository get (2): No data available for producer '" + producer + "'!")
          .disableStackTrace();
    }

    // newest origintime is at the end
    auto last = --time_model.end();

    return boost::make_shared<QImpl>(last->second);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    const auto producer_model = itsProducers.find(producer);

    if (producer_model == itsProducers.end())
    {
      throw Fmi::Exception(
          BCP, "Repository get (3): No data available for producer '" + producer + "'!")
          .disableStackTrace();
    }

    const SharedModels& models = producer_model->second;

    if (models.empty())
      throw Fmi::Exception(
          BCP, "Repository get (4): No data available for producer '" + producer + "'!")
          .disableStackTrace();

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

    throw Fmi::Exception(BCP,
                           "Repository get: No data available for producer '" + producer +
                               "' with origintime == " + to_simple_string(origintime))
        .disableStackTrace();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

    const auto producer_model = itsProducers.find(producer);

    if (producer_model == itsProducers.end())
    {
      throw Fmi::Exception(
          BCP, "Repository getPeriod: No data available for producer '" + producer + "'")
          .disableStackTrace();
    }

    const SharedModels& models = producer_model->second;

    if (models.empty())
    {
      throw Fmi::Exception(
          BCP, "Repository getPeriod: No data available for producer '" + producer + "'")
          .disableStackTrace();
    }

    // Construct a vector of datas with similar grids only

    std::vector<SharedModel> okmodels;
    boost::optional<std::size_t> hash;

    for (const auto& otime_model : models)
    {
      auto tmphash = otime_model.second->gridHashValue();
      if (hash && *hash != tmphash)
        okmodels.clear();
      okmodels.push_back(otime_model.second);
      hash = tmphash;
    }

    // Construct a view of the data

    return boost::make_shared<QImpl>(okmodels);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    auto producer_model = itsProducers.find(producer);

    if (producer_model == itsProducers.end())
      throw Fmi::Exception(BCP,
                             "Repository remove: No data available for producer '" + producer + "'")
          .disableStackTrace();

    SharedModels& models = producer_model->second;

    if (models.empty())
      throw Fmi::Exception(BCP,
                             "Repository remove: No data available for producer '" + producer + "'")
          .disableStackTrace();

    for (auto time_model = models.begin(), end = models.end(); time_model != end; ++time_model)
    {
      if (time_model->second->path() == path)
      {
        if (itsVerbose)
          std::cout << boost::posix_time::second_clock::local_time() << " [qengine] Deleting "
                    << time_model->second->path() << std::endl;
        time_model->second->uncache();  // uncache validpoints
        models.erase(time_model);
        break;
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    auto producer_model = itsProducers.find(producer);

    if (producer_model == itsProducers.end())
      throw Fmi::Exception(BCP,
                             "Repository resize: no data available for producer '" + producer + "'")
          .disableStackTrace();

    SharedModels& models = producer_model->second;

    // Usually only the oldest (1) file is deleted, so
    // the speed of this erase loop is of no concern.
    // Typically we load a new file, and then delete
    // the oldest one. During the startup phase we may
    // have multiple deletions, but that is to be expected,
    // since we have to scan all the files for their origintimes.

    while (models.size() > limit)
    {
      if (itsVerbose)
        std::cout << boost::posix_time::second_clock::local_time()
                  << " [qengine] Resize removal of " << models.begin()->second->path() << std::endl;

      // the oldest file is the one first sorted by origintime
      models.begin()->second->uncache();  // uncache validpoints
      models.erase(models.begin());       // and erase the model
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Expire too old models
 */
// ----------------------------------------------------------------------

void Repository::expire(const Producer& producer, std::size_t max_age)
{
  // max_age is in seconds, and 0 implies no limit exists

  if (max_age == 0)
    return;

  auto now = boost::posix_time::second_clock::universal_time();
  auto time_limit = now - boost::posix_time::seconds(max_age);

  try
  {
    auto producer_model = itsProducers.find(producer);

    if (producer_model == itsProducers.end())
      return;

    SharedModels& models = producer_model->second;

    if (models.empty())
      return;

    for (auto time_model = models.begin(), end = models.end(); time_model != end;)
    {
      if (time_model->second->modificationTime() >= time_limit)
        ++time_model;
      else
      {
        if (itsVerbose)
          std::cout << boost::posix_time::second_clock::local_time() << " [qengine] Expiring "
                    << time_model->second->path() << std::endl;
        time_model->second->uncache();  // uncache validpoints
        models.erase(time_model++);
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Expiring model failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    for (const Producer& producer : producerlist)
    {
      // Try primary names first
      const auto producer_model = itsProducers.find(producer);

      if (producer_model != itsProducers.end())
      {
        const auto prod_config = itsProducerConfigs.find(producer);
        // Use data maxdistance if allowed and it is set
        double chosenmaxdist =
            (usedatamaxdist && prod_config->second.maxdistance > 0 ? prod_config->second.maxdistance
                                                                   : maxdist);

        if (contains(producer_model->second, lon, lat, chosenmaxdist, leveltype))
          return producer;
      }
    }

    // Try aliases next in the given order

    for (const Producer& producer : producerorder)
    {
      const auto prod_config = itsProducerConfigs.find(producer);
      if (prod_config == itsProducerConfigs.end())
        continue;

      // Use data maxdistance if allowed and it is set
      double chosenmaxdist =
          (usedatamaxdist && prod_config->second.maxdistance > 0 ? prod_config->second.maxdistance
                                                                 : maxdist);

      const auto& aliases = prod_config->second.aliases;

      for (const Producer& alias : producerlist)
      {
        if (aliases.find(alias) != aliases.end())
        {
          const auto producer_model = itsProducers.find(producer);
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Repository::ContentTable Repository::getRepoContents(const std::string& timeFormat,
                                                     const std::string& projectionFormat) const
{
  std::string producer = "";
  return getRepoContents(producer, timeFormat, projectionFormat);
}

Repository::ContentTable Repository::getRepoContents(const std::string& producer,
                                                     const std::string& timeFormat,
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
                                      "Descriptions",
                                      "Levels",
                                      "Projection",
                                      "OriginTime",
                                      "MinTime",
                                      "MaxTime"};

    std::unique_ptr<Fmi::TimeFormatter> timeFormatter(Fmi::TimeFormatter::create(timeFormat));

    boost::shared_ptr<Spine::Table> resultTable(new Spine::Table);

    int row = 0;

    for (const auto& prodit : itsProducers)
    {
      // Skip all but the wanted producer
      if (!producer.empty() && producer != prodit.first)
        continue;

      const SharedModels& theseModels = prodit.second;

      const ProducerConfig thisConfig = itsProducerConfigs.find(prodit.first)->second;

      for (const auto& modit : theseModels)
      {
        auto model = modit.second;
        auto qi = model->info();

        // Time range
        qi->FirstTime();
        boost::posix_time::ptime time1 = qi->ValidTime();
        qi->LastTime();
        boost::posix_time::ptime time2 = qi->ValidTime();

        // Get the parameter list from querydatainfo
        std::list<std::string> params;
        std::list<std::string> descriptions;
        for (qi->ResetParam(); qi->NextParam(false);)
        {
          int paramID = boost::numeric_cast<int>(qi->Param().GetParamIdent());
          std::string paramName = Spine::ParameterFactory::instance().name(paramID);
          if (!paramName.empty())
            params.push_back(paramName);
          else
            params.emplace_back(Fmi::to_string(paramID));

          descriptions.emplace_back(qi->Param().GetParamName().CharPtr());
        }

        // Get the available levelvalues
        std::list<std::string> levels;
        for (qi->ResetLevel(); qi->NextLevel();)
        {
          float level = qi->Level()->LevelValue();
          if (level != kFloatMissing)
            levels.emplace_back(Fmi::to_string(level));
          else
            levels.emplace_back("-");
        }

        // Get projection string
        std::string projectionText;
        if (qi->Area() == nullptr)
          projectionText = "nan";
        else if (projectionFormat == "wkt")
          projectionText = qi->Area()->WKT();
        // Defaults to newbase form
        else
          projectionText = qi->Area()->ProjStr();

        // For nicer output in browsers we replace for example ",PROJCS" with ", PROJCS"
        boost::regex rex(",([A-Z])");
        projectionText = boost::regex_replace(projectionText, rex, ", $1");

        // Create the table

        int column = 0;

        // insert producer name
        resultTable->set(column, row, prodit.first);
        ++column;

        // Insert aliases
        std::string aliases = boost::algorithm::join(thisConfig.aliases, ", ");
        resultTable->set(column, row, aliases);
        ++column;

        // Insert refresh interval
        resultTable->set(column, row, Fmi::to_string(thisConfig.refresh_interval_secs));
        ++column;

        // insert model file path
        resultTable->set(column, row, modit.second->path().string());
        ++column;

        // Insert parameters
        std::string parameters = boost::algorithm::join(params, ", ");
        resultTable->set(column, row, parameters);
        ++column;

        // Insert parameter descriptions
        std::string descs = boost::algorithm::join(descriptions, ", ");
        resultTable->set(column, row, descs);
        ++column;

        // Insert levels
        std::string levs = boost::algorithm::join(levels, ", ");
        resultTable->set(column, row, levs);
        ++column;

        // Insert projection string
        resultTable->set(column, row, projectionText);
        ++column;

        // insert origin time
        resultTable->set(column, row, timeFormatter->format(modit.first));
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

    Spine::TableFormatter::Names headers;

    for (const auto& p : ContentTableHeaders)
    {
      headers.push_back(p);
    }

    return std::make_pair(resultTable, headers);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::list<MetaData> Repository::getRepoMetadata(const MetaQueryOptions& theOptions) const
{
  try
  {
    std::list<MetaData> propertyList;

    // Avoid processing all metadata if producer and possibly origintime too are known,
    // collecting all metadata may be quite slow

    if (theOptions.hasProducer())
    {
      if (theOptions.hasOriginTime())
        propertyList = getRepoMetadata(theOptions.getProducer(), theOptions.getOriginTime());
      else
        propertyList = getRepoMetadata(theOptions.getProducer());
    }
    else
      propertyList = getRepoMetadata();

    // Filter according to the given options. Producer and origintime filters
    // might have been used already, but at this point the tests fail quickly,
    // not worth optimizing.

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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::list<MetaData> Repository::getRepoMetadata(const std::string& producer) const
{
  try
  {
    std::list<MetaData> props;

    const auto producerpos = itsProducers.find(producer);
    if (producerpos == itsProducers.end())
      return props;

    const auto& models = producerpos->second;

    for (const auto& origintime_model : models)
    {
      QImpl q(origintime_model.second);
      props.push_back(q.metaData());
    }
    return props;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::list<MetaData> Repository::getRepoMetadata(const std::string& producer,
                                                const boost::posix_time::ptime& origintime) const
{
  try
  {
    std::list<MetaData> props;

    const auto producerpos = itsProducers.find(producer);
    if (producerpos == itsProducers.end())
      return props;

    const auto& models = producerpos->second;
    const auto modelpos = models.find(origintime);

    if (modelpos == models.end())
      return props;

    QImpl q(modelpos->second);
    props.push_back(q.metaData());

    return props;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::list<MetaData> Repository::getRepoMetadata() const
{
  try
  {
    std::list<MetaData> props;

    for (const auto& producer_models : itsProducers)
    {
      const auto& models = producer_models.second;

      for (const auto& origintime_model : models)
      {
        QImpl q(origintime_model.second);
        props.push_back(q.metaData());
      }
    }

    return props;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Repository::MetaObject Repository::getSynchroInfos() const
{
  try
  {
    Repository::MetaObject props;

    for (const auto& prodit : itsProducers)
    {
      const SharedModels& theseModels = prodit.second;

      const ProducerConfig thisConfig = itsProducerConfigs.find(prodit.first)->second;

      std::vector<bp::ptime> originTimes;

      for (const auto& modit : theseModels)
      {
        // Get querydata origintime

        originTimes.push_back(modit.first);
      }

      props.insert(std::make_pair(thisConfig.producer, originTimes));
    }

    return props;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

SharedModel Repository::getModel(const Producer& producer,
                                 const boost::filesystem::path& path) const
{
  try
  {
    const auto producer_model = itsProducers.find(producer);

    if (producer_model == itsProducers.end())
      return {};

    const auto& models = producer_model->second;

    for (const auto& origintime_model : models)
    {
      const auto& model = origintime_model.second;
      if (model->path() == path)
        return model;
    }

    return {};
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Repository::SharedModels Repository::getAllModels(const Producer& producer) const
{
  try
  {
    const auto producer_model = itsProducers.find(producer);

    if (producer_model == itsProducers.end())
      return SharedModels();

    return producer_model->second;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Repository::verbose(bool flag)
{
  itsVerbose = flag;
}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
