#pragma once

#include "Serialize.h"

#include <cstdint>

class AdvancedSettings {
public:
    enum Language : uint8_t {
        LanguageEnglish = 0,
        LanguageGerman  = 1,
        LanguageCount
    };

    enum Flag : uint32_t {
        ShowPlayCounter      = 1 << 0,
        EnhancedSongMode     = 1 << 1,
        ShowHomeScreen       = 1 << 2,
        // weitere Flags hier anhängen
    };

    void clear();

    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

    bool flag(Flag flag) const {
        return (_flags & flag) != 0;
    }

    void setFlag(Flag flag, bool value) {
        if (value) {
            _flags |= flag;
        } else {
            _flags &= ~flag;
        }
    }

    uint32_t flags() const {
        return _flags;
    }

    void setFlags(uint32_t flags) {
        _flags = flags;
    }

    uint8_t language() const {
        return _language;
    }

    void setLanguage(uint8_t language) {
        _language = (language < LanguageCount) ? language : LanguageEnglish;
    }

private:
    uint32_t _flags;
    uint8_t _language;
};
