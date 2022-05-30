#include "Engine.h"
#include <macgyver/Exception.h>
#include <macgyver/TypeName.h>

#define DEBUG_ENGINE_DISABLING

#ifdef DEBUG_ENGINE_DISABLING
#define DISABLE_STACKTRACE
#else
#define DISABLE_STACKTRACE .disableStackTrace()
#endif

#define DISABLED_MSG ": engine is disabled"
#define REPORT_DISABLED throw Fmi::Exception(BCP, METHOD_NAME + DISABLED_MSG) DISABLE_STACKTRACE

using namespace SmartMet::Engine::Querydata;

Engine::Engine() = default;

Engine::~Engine() = default;

const ProducerList&
Engine::producers() const
{
    REPORT_DISABLED;
}

OriginTimes
Engine::origintimes(const Producer& producer) const
{
    (void)producer;
    REPORT_DISABLED;
}

bool Engine::hasProducer(const Producer& producer) const
{
    (void)producer;
    REPORT_DISABLED;
}

CacheReportingStruct Engine::getCacheSizes() const
{
    REPORT_DISABLED;
}

Producer
Engine::find(double longitude,
             double latitude,
             double maxdistance,
             bool usedatamaxdistance,
             const std::string& leveltype) const
{
    (void)longitude;
    (void)latitude;
    (void)maxdistance;
    (void)usedatamaxdistance;
    (void)leveltype;
    REPORT_DISABLED;
}

Producer
Engine::find(const ProducerList& producerlist,
             double longitude,
             double latitude,
             double maxdistance,
             bool usedatamaxdistance,
             const std::string& leveltype) const
{   (void)producerlist;
    (void)longitude;
    (void)latitude;
    (void)maxdistance;
    (void)usedatamaxdistance;
    (void)leveltype;
    REPORT_DISABLED;
}

Q
Engine::get(const Producer& producer) const
{
    (void)producer;
    REPORT_DISABLED;
}

Q
Engine::get(const Producer& producer, const OriginTime& origintime) const
{
    (void)producer;
    (void)origintime;
    REPORT_DISABLED;
}

Repository::ContentTable
Engine::getProducerInfo(const std::string& timeFormat,
                        boost::optional<std::string> producer) const
{
    (void)timeFormat;
    (void)producer;
    REPORT_DISABLED;
}

Repository::ContentTable
Engine::getParameterInfo(boost::optional<std::string> producer) const
{
    (void)producer;
    REPORT_DISABLED;
}

boost::posix_time::time_period
Engine::getProducerTimePeriod(const Producer& producer) const
{
    (void)producer;
    REPORT_DISABLED;
}

boost::optional<ProducerMap>
Engine::getSyncProducers(const std::string& syncGroup) const
{
    (void)syncGroup;
    REPORT_DISABLED;
}

void
Engine::startSynchronize(Spine::Reactor* theReactor)
{
    (void)theReactor;
}

const ProducerConfig&
Engine::getProducerConfig(const std::string& producer) const
{
    (void)producer;
    REPORT_DISABLED;
}

Repository::MetaObject
Engine::getSynchroInfos() const
{
    REPORT_DISABLED;
}

Repository::ContentTable
Engine::getEngineContentsForAllProducers(const std::string& timeFormat,
                                         const std::string& projectionFormat) const
{
    (void)timeFormat;
    (void)projectionFormat;
    REPORT_DISABLED;
}

Repository::ContentTable
Engine::getEngineContentsForProducer(const std::string& producer,
                                     const std::string& timeFormat,
                                     const std::string& projectionFormat) const
{
    (void)producer;
    (void)timeFormat;
    (void)projectionFormat;
    REPORT_DISABLED;
}

std::list<MetaData>
Engine::getEngineMetadataBasic() const
{
    REPORT_DISABLED;
}

std::list<MetaData>
Engine::getEngineMetadataWithOptions(const MetaQueryOptions& theOptions) const
{
    (void)theOptions;
    REPORT_DISABLED;
}

std::list<MetaData>
Engine::getEngineSyncMetadataBasic(const std::string& syncGroup) const
{
    (void)syncGroup;
    REPORT_DISABLED;
}

std::list<MetaData>
Engine::getEngineSyncMetadataWithOptions(const std::string& syncGroup,
                                         const MetaQueryOptions& theOptions) const
{
    (void)syncGroup;
    (void)theOptions;
    REPORT_DISABLED;
}

CoordinatesPtr
Engine::getWorldCoordinatesDefault(const Q& theQ) const
{
    (void)theQ;
    REPORT_DISABLED;
}

CoordinatesPtr
Engine::getWorldCoordinatesForSR(const Q& theQ, const Fmi::SpatialReference& theSR) const
{
    (void)theQ;
    (void)theSR;
    REPORT_DISABLED;
}

ValuesPtr
Engine::getValuesDefault(const Q& theQ,
                         std::size_t theValuesHash,
                         boost::posix_time::ptime theTime) const
{
    (void)theQ;
    (void)theValuesHash;
    (void)theTime;
    REPORT_DISABLED;
}

ValuesPtr
Engine::getValuesForParam(const Q& theQ,
                  const Spine::Parameter& theParam,
                  std::size_t theValuesHash,
                  boost::posix_time::ptime theTime) const
{
    (void)theQ;
    (void)theParam;
    (void)theValuesHash;
    (void)theTime;
    REPORT_DISABLED;
}

void
Engine::init()
{
}

void
Engine::shutdown()
{
}

