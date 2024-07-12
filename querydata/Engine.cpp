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

const ProducerList& Engine::producers() const
{
  REPORT_DISABLED;
}

OriginTimes Engine::origintimes(const Producer& /* producer */) const
{
  REPORT_DISABLED;
}

bool Engine::hasProducer(const Producer& /* producer */) const
{
  REPORT_DISABLED;
}

CacheReportingStruct Engine::getCacheSizes() const
{
  REPORT_DISABLED;
}

Producer Engine::find(double /* longitude */,
                      double /* latitude */,
                      double /* maxdistance */,
                      bool /* usedatamaxdistance */,
                      const std::string& /* leveltype */) const
{
  REPORT_DISABLED;
}

Producer Engine::find(const ProducerList& /* producerlist */,
                      double /* longitude */,
                      double /* latitude */,
                      double /* maxdistance */,
                      bool /* usedatamaxdistance */,
                      const std::string& /* leveltype */) const
{
  REPORT_DISABLED;
}

Q Engine::get(const Producer& /* producer */) const
{
  REPORT_DISABLED;
}

Q Engine::get(const Producer& /* producer */, const OriginTime& /* origintime */) const
{
  REPORT_DISABLED;
}

Q Engine::get(const Producer& /* producer */,
              const Fmi::TimePeriod& /* timePeriod */) const
{
  REPORT_DISABLED;
}

Repository::ContentTable Engine::getProducerInfo(
    const std::string& /* timeFormat */, const std::optional<std::string>& /* producer */) const
{
  REPORT_DISABLED;
}

Repository::ContentTable Engine::getParameterInfo(
    const std::optional<std::string>& /* producer */) const
{
  REPORT_DISABLED;
}

Fmi::TimePeriod Engine::getProducerTimePeriod(const Producer& /* producer */) const
{
  REPORT_DISABLED;
}

const ProducerConfig& Engine::getProducerConfig(const std::string& /* producer */) const
{
  REPORT_DISABLED;
}

Repository::ContentTable Engine::getEngineContentsForAllProducers(
    const std::string& /* timeFormat */, const std::string& /* projectionFormat */) const
{
  REPORT_DISABLED;
}

Repository::ContentTable Engine::getEngineContentsForProducer(
    const std::string& /* producer */,
    const std::string& /* timeFormat */,
    const std::string& /* projectionFormat */) const
{
  REPORT_DISABLED;
}

std::list<MetaData> Engine::getEngineMetadataBasic() const
{
  REPORT_DISABLED;
}

std::list<MetaData> Engine::getEngineMetadataWithOptions(
    const MetaQueryOptions& /* theOptions */) const
{
  REPORT_DISABLED;
}

CoordinatesPtr Engine::getWorldCoordinatesDefault(const Q& /* theQ */) const
{
  REPORT_DISABLED;
}

CoordinatesPtr Engine::getWorldCoordinatesForSR(const Q& /* theQ */,
                                                const Fmi::SpatialReference& /* theSR */) const
{
  REPORT_DISABLED;
}

ValuesPtr Engine::getValuesDefault(const Q& /* theQ */,
                                   std::size_t /* theValuesHash */,
                                   Fmi::DateTime /* theTime */) const
{
  REPORT_DISABLED;
}

ValuesPtr Engine::getValuesForParam(const Q& /* theQ */,
                                    const Spine::Parameter& /* theParam */,
                                    std::size_t /* theValuesHash */,
                                    Fmi::DateTime /* theTime */) const
{
  REPORT_DISABLED;
}

void Engine::init() {}

void Engine::shutdown() {}
