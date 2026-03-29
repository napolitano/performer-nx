#include "Config.h"
#include "SystemPage.h"

#include "ui/pages/Pages.h"
#include "ui/painters/WindowPainter.h"

#include "core/utils/StringBuilder.h"

#ifdef PLATFORM_STM32
#include "drivers/System.h"
#endif

enum Function {
    Calibration = 0,
#ifdef CONFIG_ADVANCED_SETTINGS
    Advanced    = 2,
#endif
    Utilities   = 3,
    Update      = 4,
};

static const char *functionNames[] = {
    TXT_MENU_SYSTEM_CALIBRATION,
    nullptr,
    TXT_MENU_SYSTEM_ADVANCED,
    TXT_MENU_SYSTEM_UTILS,
    TXT_MENU_SYSTEM_UPDATE
};

enum CalibrationEditFunction {
    Auto        = 0,
};

#ifdef CONFIG_ADVANCED_SETTINGS
enum AdvancedEditFunction {
    Default        = 0,
    Cancel         = 3,
    Commit         = 4,
};
#endif


static const char *calibrationEditFunctionNames[] = {
    TXT_MENU_AUTO,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

#ifdef CONFIG_ADVANCED_SETTINGS
static const char *advancedEditFunctionNames[] = {
    TXT_MENU_DEFAULT,
    nullptr,
    nullptr,
    TXT_MENU_REVERT,
    TXT_MENU_COMMIT
};
#endif

enum class ContextAction {
    Init,
    Save,
    Backup,
    Restore,
    Last
};

static const ContextMenuModel::Item contextMenuItems[] = {
    { TXT_MENU_INIT },
    { TXT_MENU_SAVE },
    { TXT_MENU_BACKUP },
    { TXT_MENU_RESTORE }
};

SystemPage::SystemPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _cvOutputListModel),
    _settings(context.model.settings())
{
    setOutputIndex(0);
}

void SystemPage::enter() {
    setOutputIndex(_project.selectedTrackIndex());

    _engine.suspend();
    _engine.setGateOutput(0xff);
    _engine.setGateOutputOverride(true);
    _engine.setCvOutputOverride(true);

    _encoderDownTicks = 0;

    updateOutputs();
}

void SystemPage::exit() {
    _engine.setGateOutputOverride(false);
    _engine.setCvOutputOverride(false);
    _engine.resume();
}

void SystemPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, TXT_MODE_SYSTEM);

    switch (_mode) {
    case Mode::Calibration: {
        FixedStringBuilder<8> str(TXT_FUNCTION_CALIBRATION, _outputIndex + 1);
        WindowPainter::drawActiveFunction(canvas, str);
        if (edit()) {
            WindowPainter::drawFooter(canvas, calibrationEditFunctionNames, pageKeyState());
        } else {
            WindowPainter::drawFooter(canvas, functionNames, pageKeyState(), int(_mode));
        }
        ListPage::draw(canvas);
        break;
    }

#ifdef CONFIG_ADVANCED_SETTINGS
    case Mode::Advanced: {
        WindowPainter::drawActiveFunction(canvas, TXT_FUNCTION_ADVANCED_SETTING);
        WindowPainter::drawFooter(canvas, functionNames, pageKeyState(), int(_mode));
        ListPage::draw(canvas);
        if (edit()) {
            WindowPainter::drawFooter(canvas, advancedEditFunctionNames, pageKeyState());
        } else {
            WindowPainter::drawFooter(canvas, functionNames, pageKeyState(), int(_mode));
        }
        break;
    }
#endif

    case Mode::Utilities: {
        WindowPainter::drawActiveFunction(canvas, TXT_FUNCTION_UTILITIES);
        WindowPainter::drawFooter(canvas, functionNames, pageKeyState(), int(_mode));
        ListPage::draw(canvas);
        break;
    }
    case Mode::Update: {
        WindowPainter::drawActiveFunction(canvas, TXT_FUNCTION_UPDATE);
        WindowPainter::drawFooter(canvas, functionNames, pageKeyState(), int(_mode));
        canvas.setBlendMode(BlendMode::Set);
        canvas.setColor(UI_COLOR_ACTIVE);
        FixedStringBuilder<16> str(TXT_INFO_CURRENT_VERSION, CONFIG_VERSION_MAJOR, CONFIG_VERSION_MINOR, CONFIG_VERSION_REVISION);
        canvas.drawText(4, 24, str);
        canvas.drawText(4, 40, TXT_PRESS_ENCODER_TO_RESET_TO_BOOTLOADER);

#ifdef PLATFORM_STM32
        if (_encoderDownTicks != 0 && os::ticks() - _encoderDownTicks > os::time::ms(2000)) {
            System::reset();
        }
#endif
        break;
    }
    }
}

void SystemPage::updateLeds(Leds &leds) {
    int selectedTrack = _project.selectedTrackIndex();

    for (int track = 0; track < 8; ++track) {
        bool selected = track == selectedTrack;
        leds.set(MatrixMap::fromTrack(track), selected, selected);
    }

    for (int step = 0; step < 16; ++step) {
        leds.set(MatrixMap::fromStep(step), false, false);
    }
}

void SystemPage::keyDown(KeyEvent &event) {
    const auto &key = event.key();

    switch (_mode) {
    case Mode::Update:
        if (key.isEncoder()) {
            _encoderDownTicks = os::ticks();
        }
        break;
    default:
        break;
    }
}

void SystemPage::keyUp(KeyEvent &event) {
    const auto &key = event.key();

    switch (_mode) {
    case Mode::Update:
        if (key.isEncoder()) {
            _encoderDownTicks = 0;
        }
        break;
    default:
        break;
    }
}

void SystemPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isContextMenu()) {
        if (_mode == Mode::Calibration) {
            contextShow();
            event.consume();
            return;
        }
    }

    if (key.isFunction()) {
        if (_mode == Mode::Calibration && edit()) {
            if (CalibrationEditFunction(key.function()) == CalibrationEditFunction::Auto) {
                _settings.calibration().cvOutput(_project.selectedTrackIndex()).setUserDefined(selectedRow(), false);
                setEdit(false);
            }
        } else {
            switch (Function(key.function())) {
            case Function::Calibration:
                setMode(Mode::Calibration);
                break;
#ifdef CONFIG_ADVANCED_SETTINGS
            case Function::Advanced:
                setMode(Mode::Advanced);
                break;
#endif
            case Function::Utilities:
                setMode(Mode::Utilities);
                break;
            case Function::Update:
                setMode(Mode::Update);
                break;
            }
        }
    }

    switch (_mode) {
    case Mode::Calibration:
        if (key.isTrack()) {
            setOutputIndex(key.track());
        }
        if (!edit() && key.isEncoder()) {
            _settings.calibration().cvOutput(_project.selectedTrackIndex()).setUserDefined(selectedRow(), true);
        }
        ListPage::keyPress(event);
        updateOutputs();
        break;
#ifdef CONFIG_ADVANCED_SETTINGS
    case Mode::Advanced:
        return;
#endif
    case Mode::Utilities:
        if (key.isEncoder()) {
            executeUtilityItem(UtilitiesListModel::Item(selectedRow()));
        } else {
            ListPage::keyPress(event);
        }
        break;
    case Mode::Update:
        break;
    }
}

void SystemPage::encoder(EncoderEvent &event) {
    switch (_mode) {
    case Mode::Calibration:
        ListPage::encoder(event);
        updateOutputs();
        break;
    case Mode::Advanced:
        break;
    case Mode::Utilities:
        ListPage::encoder(event);
        break;
    case Mode::Update:
        break;
    }
}

void SystemPage::setMode(Mode mode) {
    _mode = mode;
    switch (_mode) {
    case Mode::Calibration:
        setListModel(_cvOutputListModel);
        break;
#ifdef CONFIG_ADVANCED_SETTINGS
    case Mode::Advanced:
        setListModel(_advancedListModel);
        break;
#endif
    case Mode::Utilities:
        setListModel(_utilitiesListModel);
        break;
    default:
        break;
    }
}

void SystemPage::setOutputIndex(int index) {
    _outputIndex = index;
    _cvOutputListModel.setCvOutput(_settings.calibration().cvOutput(index));
}

void SystemPage::updateOutputs() {
    float volts = Calibration::CvOutput::itemToVolts(selectedRow());
    for (int i = 0; i < CONFIG_CV_OUTPUT_CHANNELS; ++i) {
        _engine.setCvOutput(i, volts);
    }
}

void SystemPage::executeUtilityItem(UtilitiesListModel::Item item) {
    switch (item) {
    case UtilitiesListModel::FormatSdCard:
        formatSdCard();
        break;
    case UtilitiesListModel::Last:
        break;
    }
}

void SystemPage::contextShow() {
    showContextMenu(ContextMenu(
        contextMenuItems,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return contextActionEnabled(index); }
    ));
}

void SystemPage::contextAction(int index) {
    switch (ContextAction(index)) {
    case ContextAction::Init:
        initSettings();
        break;
    case ContextAction::Save:
        saveSettings();
        break;
    case ContextAction::Backup:
        backupSettings();
        break;
    case ContextAction::Restore:
        restoreSettings();
        break;
    case ContextAction::Last:
        break;
    }
}

bool SystemPage::contextActionEnabled(int index) const {
    switch (ContextAction(index)) {
    case ContextAction::Backup:
    case ContextAction::Restore:
        return FileManager::volumeMounted();
    default:
        return true;
    }
}

void SystemPage::initSettings() {
    _manager.pages().confirmation.show(TXT_ARE_YOU_SURE, [this] (bool result) {
        if (result) {
            _settings.clear();
            showMessage(TXT_MESSAGE_SETTINGS_INITIALIZED);
        }
    });
}

void SystemPage::saveSettings() {
    _manager.pages().confirmation.show(TXT_ARE_YOU_SURE, [this] (bool result) {
        if (result) {
            saveSettingsToFlash();
        }
    });
}

void SystemPage::backupSettings() {
    _manager.pages().confirmation.show(TXT_ARE_YOU_SURE, [this] (bool result) {
        if (result) {
            backupSettingsToFile();
        }
    });
}

void SystemPage::restoreSettings() {
    if (fs::exists(Settings::Filename)) {
        _manager.pages().confirmation.show(TXT_ARE_YOU_SURE, [this] (bool result) {
            if (result) {
                restoreSettingsFromFile();
            }
        });
    }
}

void SystemPage::saveSettingsToFlash() {
    _engine.suspend();
    _manager.pages().busy.show(TXT_STATUS_SAVING_SETTINGS);

    FileManager::task([this] () {
        _model.settings().writeToFlash();
        return fs::OK;
    }, [this] (fs::Error result) {
        showMessage(TXT_MESSAGE_SETTINGS_SAVED);
        // TODO lock ui mutex
        _manager.pages().busy.close();
        _engine.resume();
    });
}

void SystemPage::backupSettingsToFile() {
    _engine.suspend();
    _manager.pages().busy.show(TXT_STATUS_BACKING_UP_SETTINGS);

    FileManager::task([this] () {
        return FileManager::writeSettings(_model.settings(), Settings::Filename);
    }, [this] (fs::Error result) {
        if (result == fs::OK) {
            showMessage(TXT_MESSAGE_SETTINGS_BACKED_UP);
        } else {
            showMessage(FixedStringBuilder<32>(TXT_ERROR_FAILED, fs::errorToString(result)));
        }
        // TODO lock ui mutex
        _manager.pages().busy.close();
        _engine.resume();
    });
}

void SystemPage::restoreSettingsFromFile() {
    _engine.suspend();
    _manager.pages().busy.show(TXT_STATUS_RESTORING_SETTINGS);

    FileManager::task([this] () {
        return FileManager::readSettings(_model.settings(), Settings::Filename);
    }, [this] (fs::Error result) {
        if (result == fs::OK) {
            showMessage(TXT_MESSAGE_SETTINGS_RESTORED);
        } else if (result == fs::INVALID_CHECKSUM) {
            showMessage(TXT_ERROR_INVALID_SETTINGS_FILE);
            _model.settings().readFromFlash();
        } else {
            showMessage(FixedStringBuilder<32>(TXT_ERROR_FAILED, fs::errorToString(result)));
            _model.settings().readFromFlash();
        }
        // TODO lock ui mutex
        _manager.pages().busy.close();
        _engine.resume();
    });
}

void SystemPage::formatSdCard() {
    if (!FileManager::volumeAvailable()) {
        showMessage(TXT_MESSAGE_NO_SD_CARD_DETECTED);
        return;
    }

    _manager.pages().confirmation.show(TXT_REALLY_FORMAT_SD_CARD, [this] (bool result) {
        if (result) {
            _manager.pages().busy.show(TXT_STATUS_FORMATTING_SD_CARD);

            FileManager::task([] () {
                return FileManager::format();
            }, [this] (fs::Error result) {
                if (result == fs::OK) {
                    showMessage(TXT_MESSAGE_SD_CARD_FORMATTED);
                } else {
                    showMessage(FixedStringBuilder<32>(TXT_ERROR_FAILED, fs::errorToString(result)));
                }
                // TODO lock ui mutex
                _manager.pages().busy.close();
            });
        }
    });
}
