#pragma once

#include "ListPage.h"

#include "ui/model/CalibrationCvOutputListModel.h"
#include "ui/model/UtilitiesListModel.h"
#ifdef CONFIG_ADVANCED_SETTINGS
#include "ui/model/AdvancedSettingsListModel.h"
#endif

#include "model/Settings.h"

class SystemPage : public ListPage {
public:
    SystemPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void updateLeds(Leds &leds) override;

    virtual void keyDown(KeyEvent &event) override;
    virtual void keyUp(KeyEvent &event) override;
    virtual void keyPress(KeyPressEvent &event) override;
    virtual void encoder(EncoderEvent &event) override;

private:
    enum class Mode : uint8_t {
        Calibration = 0,
#ifdef CONFIG_ADVANCED_SETTINGS
        Advanced    = 2,
#endif
        Utilities   = 3,
        Update      = 4,
    };

    void setMode(Mode mode);
    void setOutputIndex(int index);
    void updateOutputs();

    void executeUtilityItem(UtilitiesListModel::Item item);

    void contextShow();
    void contextAction(int index);
    bool contextActionEnabled(int index) const;

    void initSettings();
    void saveSettings();
    void backupSettings();
    void restoreSettings();
    void formatSdCard();

    void saveSettingsToFlash();
    void backupSettingsToFile();
    void restoreSettingsFromFile();

    Mode _mode = Mode::Calibration;
    Settings &_settings;

    int _outputIndex;
    CalibrationCvOutputListModel _cvOutputListModel;
    UtilitiesListModel _utilitiesListModel;
#ifdef CONFIG_ADVANCED_SETTINGS
    AdvancedSettingsListModel _advancedListModel;
#endif
    uint32_t _encoderDownTicks;
};
