
#include "MetaData.h"

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
ModelParameter::ModelParameter(std::string theName, std::string theDesc, int thePrecision)
    : name(std::move(theName)), description(std::move(theDesc)), precision(thePrecision)
{
}

ModelLevel::ModelLevel(std::string theType, std::string theName, float theValue)
    : type(std::move(theType)), name(std::move(theName)), value(theValue)
{
}

MetaData::MetaData() {}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
