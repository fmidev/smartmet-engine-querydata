#pragma once

#include <boost/optional.hpp>
#include <map>
#include <string>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
class ParameterTranslations
{
 public:
  void setDefaultLanguage(const std::string& theLanguage);
  const std::string& getDefaultLanguage() const;

  void addTranslation(const std::string& theParam,
                      int theValue,
                      const std::string& theLanguage,
                      const std::string& theTranslation);

  boost::optional<std::string> getTranslation(const std::string& theParam,
                                              int theValue,
                                              const std::string& theLanguage) const;

 private:
  using LanguageToWordMap = std::map<std::string, std::string>;
  using ValueToLanguageMap = std::map<int, LanguageToWordMap>;
  using ParameterToValueMap = std::map<std::string, ValueToLanguageMap>;

  ParameterToValueMap m_translations;
  std::string m_language{"en"};
};
}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
