#include "ParameterTranslations.h"

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
void ParameterTranslations::setDefaultLanguage(const std::string& theLanguage)
{
  m_language = theLanguage;
}

const std::string& ParameterTranslations::getDefaultLanguage() const
{
  return m_language;
}

void ParameterTranslations::addTranslation(const std::string& theParam,
                                           int theValue,
                                           const std::string& theLanguage,
                                           const std::string& theTranslation)
{
  m_translations[theParam][theValue][theLanguage] = theTranslation;
}

boost::optional<std::string> ParameterTranslations::getTranslation(
    const std::string& theParam, int theValue, const std::string& theLanguage) const
{
  auto param = m_translations.find(theParam);
  if (param == m_translations.end())
    return {};

  auto lang = param->second.find(theValue);
  if (lang == param->second.end())
    return {};

  auto trans = lang->second.find(theLanguage);
  if (trans != lang->second.end())
    return trans->second;

  // No translation found for the requested language, use default language instead
  trans = lang->second.find(m_language);
  if (trans == lang->second.end())
    return {};

  return trans->second;
}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
