#pragma once

#include <string>

namespace nascade {

class I18n {
public:
    static I18n& instance();

    void setLanguage(const std::string& lang);
    const std::string& language() const { return lang_; }
    std::string t(const std::string& key) const;

private:
    I18n() = default;
    std::string lang_ = "zh";
};

inline std::string tr(const std::string& key) {
    return I18n::instance().t(key);
}

}  // namespace nascade
