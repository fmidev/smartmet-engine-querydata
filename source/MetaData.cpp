
#include "MetaData.h"

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
ModelParameter::ModelParameter(const std::string& theName,
                               const std::string& theDesc,
                               int thePrecision)
    : name(theName), description(theDesc), precision(thePrecision)
{
}

ModelLevel::ModelLevel(const std::string& theType, const std::string& theName, float theValue)
    : type(theType), name(theName), value(theValue)
{
}

MetaData::MetaData()
{
}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
