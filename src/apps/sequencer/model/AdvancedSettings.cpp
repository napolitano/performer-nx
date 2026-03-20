#include "AdvancedSettings.h"

void AdvancedSettings::clear() {
    _flags = 0;
    _language = LanguageEnglish;
}

void AdvancedSettings::write(VersionedSerializedWriter &writer) const {
    writer.write(_flags);
    writer.write(_language);
}

void AdvancedSettings::read(VersionedSerializedReader &reader) {
    clear();

    // AdvancedSettings existieren erst ab Settings::Version >= 2
    reader.read(_flags);
    reader.read(_language);

    if (_language >= LanguageCount) {
        _language = LanguageEnglish;
    }
}