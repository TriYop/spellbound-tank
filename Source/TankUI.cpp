#include "TankUI.h"

#include "FactoryPresets.h"

#include <cstdlib>
#include <string>

START_NAMESPACE_DISTRHO

namespace {

namespace hui = audioplugins::common::hui;

// Bronze/brown "MMORPG tank" palette, ported verbatim from the JUCE-era
// TankCol (see Source/_juce_reference/UI/TankLookAndFeel.h).
const hui::dgl::RotaryKnobPalette kTankKnobPalette = {
    /* track        */ {0x3a, 0x2c, 0x1a, 0xff}, // TankCol::trackArc()     0xff3a2c1a
    /* valueArc     */ {0xcc, 0x88, 0x33, 0xff}, // TankCol::valueArc()     0xffcc8833
    /* valueArcGlow */ {0xff, 0xaa, 0x44, 0xff}, // TankCol::valueArcGlow() 0xffffaa44
    /* knobTop      */ {0x5a, 0x45, 0x30, 0xff}, // TankCol::knobTop()      0xff5a4530
    /* knobBottom   */ {0x1c, 0x14, 0x10, 0xff}, // TankCol::knobBottom()   0xff1c1410
    /* knobRim      */ {0x6a, 0x53, 0x40, 0xff}, // TankCol::knobRim()      0xff6a5340
};

const hui::dgl::ToggleSwitchPalette kTankTogglePalette = {
    /* bezel      */ {0x1c, 0x14, 0x10, 0xff}, // TankCol::knobBottom()
    /* track      */ {0x3a, 0x2c, 0x1a, 0xff}, // TankCol::trackArc()
    /* activeGlow */ {0xcc, 0x88, 0x33, 0xff}, // TankCol::valueArc()
    /* thumbTop   */ {0x5a, 0x45, 0x30, 0xff}, // TankCol::knobTop()
    /* thumbBottom*/ {0x1c, 0x14, 0x10, 0xff}, // TankCol::knobBottom()
    /* thumbRim   */ {0x6a, 0x53, 0x40, 0xff}, // TankCol::knobRim()
};

const hui::dgl::VuMeterPalette kTankGrMeterPalette = {
    /* background */ {0x12, 0x0c, 0x08, 0xff}, // TankCol::tbBg()        0xff120c08
    /* border     */ {0x4a, 0x3a, 0x28, 0xff}, // TankCol::panelBorder() 0xff4a3a28
    /* fillLow    */ {0xcc, 0x88, 0x33, 0xff}, // TankCol::valueArc()
    /* fillHigh   */ {0xff, 0xaa, 0x44, 0xff}, // TankCol::valueArcGlow()
    /* peakLine   */ {0xff, 0xff, 0xff, 0xff}, // white (VuMeterPalette default)
};

const hui::dgl::ButtonPalette kTankButtonPalette = {
    /* background         */ {0x3a, 0x2c, 0x1a, 0xff}, // TankCol::trackArc()
    /* backgroundDisabled */ {0x1c, 0x14, 0x10, 0xff}, // TankCol::knobBottom()
    /* border             */ {0x6a, 0x53, 0x40, 0xff}, // TankCol::knobRim()
    /* text               */ {0xd8, 0xc8, 0xa8, 0xff}, // TankCol::textPrimary()
    /* textDisabled       */ {0x7a, 0x6a, 0x55, 0xff}, // TankCol::textDim()
};

const hui::dgl::PresetSelectorPalette kTankPresetSelectorPalette = {
    /* closedBackground */ {0x3a, 0x2c, 0x1a, 0xff}, // TankCol::trackArc()
    /* listBackground   */ {0x1c, 0x14, 0x10, 0xff}, // TankCol::bg()
    /* border           */ {0x6a, 0x53, 0x40, 0xff}, // TankCol::knobRim()
    /* text             */ {0xd8, 0xc8, 0xa8, 0xff}, // TankCol::textPrimary()
    /* textFactory      */ {0x7a, 0x6a, 0x55, 0xff}, // TankCol::textDim()
    /* rowHighlight     */ {0xcc, 0x88, 0x33, 0x40}, // TankCol::valueArc() at low alpha
};

constexpr hui::Colour kBackgroundColor{0x1c, 0x14, 0x10, 0xff};  // TankCol::bg()          0xff1c1410
constexpr hui::Colour kTextPrimaryColor{0xd8, 0xc8, 0xa8, 0xff}; // TankCol::textPrimary() 0xffd8c8a8

// DPF's idle callback runs at ~62.5Hz (16ms, both CLAP/VST3 timer
// intervals -- see Hex's HexUI.cpp derivation). VuMeterModel's peak-hold
// decays at kPeakDecayPerSecond=0.3/s from a max pushed level of 1.0 (Tank
// pushes a normalized 0..1 reduction ratio, never above 1.0, unlike a raw
// audio level that can exceed 1.0): 1.0 / 0.3 = 3.33s of decay, so
// 3.33 * 62.5 = ~208 ticks. Reusing Hex's more conservative 320-tick
// constant (derived for kMaxLevel=1.5) is safe here too -- it only means
// the GR meter keeps refreshing slightly longer than strictly necessary
// after gain reduction returns to 0, never less.
constexpr int kMeterIdleTicks = 320;

inline DGL_NAMESPACE::Color toDglColor(const hui::Colour& c) noexcept
{
    return DGL_NAMESPACE::Color(static_cast<int>(c.r), static_cast<int>(c.g),
                                 static_cast<int>(c.b), static_cast<float>(c.a) / 255.f);
}

// Canvas is DISTRHO_UI_DEFAULT_WIDTH x DISTRHO_UI_DEFAULT_HEIGHT (500x284).
// Layout ported from the JUCE-era resized() (Source/_juce_reference/
// PluginEditor.cpp), stacked top-to-bottom: preset bar (new, see Task 7),
// bypass toggle, GR meter (full width, horizontal), then the 4-knob row --
// same vertical order the JUCE editor used (top row bypass, then meter row,
// then knob row).
constexpr float kPresetBarX = 16.0f;
constexpr float kPresetBarY = 8.0f;
constexpr uint  kPresetBarRowH = 24;
constexpr uint  kPresetSelectorW = 200;
constexpr uint  kPresetButtonW = 56;
constexpr float kPresetButtonGap = 6.0f;
constexpr float kSaveButtonX = kPresetBarX + static_cast<float>(kPresetSelectorW) + 8.0f;
constexpr float kDeleteButtonX = kSaveButtonX + static_cast<float>(kPresetButtonW) + kPresetButtonGap;

constexpr int   kBypassX = 16;
constexpr int   kBypassY = 44;
constexpr uint  kBypassSwitchW = 30;
constexpr uint  kBypassSwitchH = 40;
constexpr float kBypassLabelCenterY = 96.0f;
constexpr float kLabelFontSize = 11.0f;

constexpr float kMeterLabelCenterY = 118.0f;
constexpr int   kMeterX = 16;
constexpr int   kMeterY = 126;
constexpr uint  kMeterW = 468; // 500 - 16 margin each side
constexpr uint  kMeterH = 22;

constexpr int kKnobY = 168;
constexpr uint kKnobSize = 84;
constexpr int kKnobGap = 20;
constexpr int kLabelY = kKnobY + static_cast<int>(kKnobSize) + 6; // 258
constexpr int kLabelH = 16;

// 4 knobs, centered in the 468px usable width (x=16..484).
constexpr int kKnobRowContentW = 4 * static_cast<int>(kKnobSize) + 3 * kKnobGap; // 396
constexpr int kKnobRowX0 = 16 + (static_cast<int>(kMeterW) - kKnobRowContentW) / 2; // 52

constexpr int kDepthX        = kKnobRowX0;                                            // 52
constexpr int kAnticipationX = kDepthX + static_cast<int>(kKnobSize) + kKnobGap;      // 156
constexpr int kReleaseX      = kAnticipationX + static_cast<int>(kKnobSize) + kKnobGap; // 260
constexpr int kSensitivityX  = kReleaseX + static_cast<int>(kKnobSize) + kKnobGap;      // 364

struct KnobSpec
{
    uint32_t parameterIndex;
    int x;
    float rangeMin, rangeMax, defaultValue;
    const char* label;
};

constexpr KnobSpec kDepthSpec        { kParameterDepth,        kDepthX,        TANK_PARAM_DEPTH_MIN,        TANK_PARAM_DEPTH_MAX,        TANK_PARAM_DEPTH_DEFAULT,        "Mitigation" };
constexpr KnobSpec kAnticipationSpec { kParameterAnticipation, kAnticipationX, TANK_PARAM_ANTICIPATION_MIN, TANK_PARAM_ANTICIPATION_MAX, TANK_PARAM_ANTICIPATION_DEFAULT, "Reflex" };
constexpr KnobSpec kReleaseSpec      { kParameterRelease,      kReleaseX,      TANK_PARAM_RELEASE_MIN,      TANK_PARAM_RELEASE_MAX,      TANK_PARAM_RELEASE_DEFAULT,      "Cooldown" };
constexpr KnobSpec kSensitivitySpec  { kParameterSensitivity,  kSensitivityX,  TANK_PARAM_SENSITIVITY_MIN,  TANK_PARAM_SENSITIVITY_MAX,  TANK_PARAM_SENSITIVITY_DEFAULT,  "Aggro Trigger" };

constexpr const KnobSpec* kKnobSpecs[4] = { &kDepthSpec, &kAnticipationSpec, &kReleaseSpec, &kSensitivitySpec };

} // namespace

namespace {

std::unique_ptr<hui::dgl::RotaryKnob> makeKnob(TankUI& ui, const KnobSpec& spec)
{
    std::unique_ptr<hui::dgl::RotaryKnob> knob(new hui::dgl::RotaryKnob(&ui));
    knob->setSize(kKnobSize, kKnobSize);
    knob->setAbsolutePos(spec.x, kKnobY);
    knob->setPalette(kTankKnobPalette);
    knob->setRange(spec.rangeMin, spec.rangeMax);
    knob->setDefaultValue(spec.defaultValue);
    knob->setValue(spec.defaultValue);

    const uint32_t paramIndex = spec.parameterIndex;
    hui::dgl::RotaryKnob* const rawKnob = knob.get();
    rawKnob->onDragStateChanged = [&ui, paramIndex](const bool started) { ui.editParameter(paramIndex, started); };
    rawKnob->onValueChanged = [&ui, paramIndex](const float value) { ui.setParameterValue(paramIndex, value); };

    return knob;
}

std::unique_ptr<hui::dgl::ToggleSwitch> makeBypassSwitch(TankUI& ui)
{
    std::unique_ptr<hui::dgl::ToggleSwitch> sw(new hui::dgl::ToggleSwitch(&ui));
    sw->setSize(kBypassSwitchW, kBypassSwitchH);
    sw->setAbsolutePos(kBypassX, kBypassY);
    sw->setPalette(kTankTogglePalette);
    sw->setPosition(static_cast<int>(TANK_PARAM_BYPASS_DEFAULT));

    hui::dgl::ToggleSwitch* const rawSwitch = sw.get();
    rawSwitch->onDragStateChanged = [&ui](const bool started) { ui.editParameter(kParameterBypass, started); };
    rawSwitch->onPositionChanged = [&ui](const int position) { ui.setParameterValue(kParameterBypass, static_cast<float>(position)); };

    return sw;
}

std::unique_ptr<hui::dgl::VuMeter> makeGrMeter(TankUI& ui)
{
    std::unique_ptr<hui::dgl::VuMeter> meter(new hui::dgl::VuMeter(&ui));
    meter->setSize(kMeterW, kMeterH);
    meter->setAbsolutePos(kMeterX, kMeterY);
    meter->setPalette(kTankGrMeterPalette);
    meter->setOrientation(hui::dgl::VuMeter::Orientation::Horizontal);
    return meter;
}

std::unique_ptr<hui::dgl::PresetSelector> makePresetSelector(TankUI& ui)
{
    std::unique_ptr<hui::dgl::PresetSelector> selector(new hui::dgl::PresetSelector(&ui));
    selector->setPalette(kTankPresetSelectorPalette);
    selector->setClosedSize(kPresetSelectorW, kPresetBarRowH);
    selector->setAbsolutePos(static_cast<int>(kPresetBarX), static_cast<int>(kPresetBarY));
    return selector;
}

std::unique_ptr<hui::dgl::Button> makeButton(TankUI& ui, const char* label, float x)
{
    std::unique_ptr<hui::dgl::Button> button(new hui::dgl::Button(&ui));
    button->setPalette(kTankButtonPalette);
    button->setLabel(label);
    button->setSize(kPresetButtonW, kPresetBarRowH);
    button->setAbsolutePos(static_cast<int>(x), static_cast<int>(kPresetBarY));
    return button;
}

// Linux-only for now, matching Hex's precedent -- Tank's Windows/macOS
// builds aren't verified yet either (see Task 8's CI note).
std::string tankUserPresetsDirectory()
{
    const char* home = std::getenv("HOME");
    if (home == nullptr)
        return "/tmp/Tank/presets";
    return std::string(home) + "/.config/Tank/presets";
}

} // namespace

TankUI::TankUI()
    : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT),
      fPluginPtr(static_cast<TankPluginAdapter*>(getPluginInstancePointer())),
      fDepthKnob(makeKnob(*this, kDepthSpec)),
      fAnticipationKnob(makeKnob(*this, kAnticipationSpec)),
      fReleaseKnob(makeKnob(*this, kReleaseSpec)),
      fSensitivityKnob(makeKnob(*this, kSensitivitySpec)),
      fBypassSwitch(makeBypassSwitch(*this)),
      fGrMeter(makeGrMeter(*this)),
      fPresetBrowser(tankFactoryPresets(), tankUserPresetsDirectory(), "com.spellbound.tank"),
      fPresetSelector(makePresetSelector(*this)),
      fSaveButton(makeButton(*this, "SAVE", kSaveButtonX)),
      fDeleteButton(makeButton(*this, "DELETE", kDeleteButtonX))
{
    loadSharedResources();

    fPresetSelector->onIndexSelected = [this](const int index)
    {
        if (const auto* preset = fPresetBrowser.selectIndex(index))
        {
            applyPreset(*preset);
            // Only now do the live parameters actually match entry `index`.
            fDisplayedPresetIndex = index;
            refreshPresetControls();
        }
    };

    fDeleteButton->onClick = [this]()
    {
        if (fPresetBrowser.deleteCurrent())
        {
            // deleteCurrent() resets PresetBrowser's own bookkeeping to
            // index 0 without applying that (or any) preset's values to the
            // live parameters -- the live values are still whatever they
            // were before the deleted preset. Nothing is "selected" anymore.
            fDisplayedPresetIndex = -1;
            refreshPresetControls();
        }
    };

    fSaveButton->onClick = [this]()
    {
        const std::string startDir = tankUserPresetsDirectory();
        FileBrowserOptions options;
        options.saving = true;
        options.defaultName = "New Preset.xml";
        options.title = "Save Tank Preset";
        options.startDir = startDir.c_str();
        openFileBrowser(options);
    };

    refreshPresetControls();
}

void TankUI::parameterChanged(const uint32_t index, const float value)
{
    // Programmatic path: setValue()/setPosition() deliberately don't fire
    // onValueChanged/onPositionChanged, so host automation / preset recall
    // can't loop back out to the host.
    switch (index)
    {
    case kParameterDepth:        fDepthKnob->setValue(value); break;
    case kParameterAnticipation: fAnticipationKnob->setValue(value); break;
    case kParameterRelease:      fReleaseKnob->setValue(value); break;
    case kParameterSensitivity:  fSensitivityKnob->setValue(value); break;
    case kParameterBypass:       fBypassSwitch->setPosition(static_cast<int>(value + 0.5f)); break;
    default:
        break;
    }
}

void TankUI::uiIdle()
{
    if (fPluginPtr == nullptr)
        return;

    const float reductionDb = fPluginPtr->getCurrentReductionDb();
    const float normalized = reductionDb / (TankPluginAdapter::kMaxDepthDb > 0.001f ? TankPluginAdapter::kMaxDepthDb : 0.001f);

    if (normalized > 0.0f)
        fMeterIdleCountdown = kMeterIdleTicks;
    else if (fMeterIdleCountdown > 0)
        --fMeterIdleCountdown;
    else
        return;

    fGrMeter->pushLevel(normalized);
}

void TankUI::uiFileBrowserSelected(const char* filename)
{
    if (filename == nullptr)
        return; // user cancelled the dialog

    std::string path(filename);
    const size_t slash = path.find_last_of("/\\");
    std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
    const size_t dot = base.find_last_of('.');
    if (dot != std::string::npos)
        base = base.substr(0, dot);

    if (fPresetBrowser.saveAs(base, captureCurrentParameters()))
    {
        // saveAs() writes the current live values out as a new user preset
        // and selects it in PresetBrowser's own bookkeeping, but it never
        // applies anything back to the live parameters (there is nothing to
        // apply -- the file was just written from them). Per the same rule
        // as DELETE, only onIndexSelected's applyPreset() is allowed to mark
        // a preset as displayed-selected, so treat this as "no preset"
        // rather than special-casing the fact that the values happen to
        // still match what was just saved.
        fDisplayedPresetIndex = -1;
        refreshPresetControls();
    }
}

void TankUI::applyPreset(const audioplugins::common::presets::Preset& preset)
{
    for (const auto& pv : preset.parameters)
    {
        uint32_t paramIndex;

        if (pv.id == "bypass")            { fBypassSwitch->setPosition(static_cast<int>(pv.value + 0.5f)); paramIndex = kParameterBypass; }
        else if (pv.id == "depth")        { fDepthKnob->setValue(pv.value);                                paramIndex = kParameterDepth; }
        else if (pv.id == "anticipation") { fAnticipationKnob->setValue(pv.value);                         paramIndex = kParameterAnticipation; }
        else if (pv.id == "release")      { fReleaseKnob->setValue(pv.value);                               paramIndex = kParameterRelease; }
        else if (pv.id == "sensitivity")  { fSensitivityKnob->setValue(pv.value);                           paramIndex = kParameterSensitivity; }
        else continue; // unknown id (forward-compatible with a future schema addition) -- ignore

        editParameter(paramIndex, true);
        setParameterValue(paramIndex, pv.value);
        editParameter(paramIndex, false);
    }
}

std::vector<audioplugins::common::presets::ParameterValue> TankUI::captureCurrentParameters() const
{
    return {
        {"bypass", static_cast<float>(fBypassSwitch->getPosition())},
        {"depth", fDepthKnob->getValue()},
        {"anticipation", fAnticipationKnob->getValue()},
        {"release", fReleaseKnob->getValue()},
        {"sensitivity", fSensitivityKnob->getValue()},
    };
}

void TankUI::refreshPresetControls()
{
    // Deliberately drive both the dropdown's selection and the DELETE
    // button off fDisplayedPresetIndex, NOT fPresetBrowser.getCurrentIndex():
    // the latter is PresetBrowser's own internal bookkeeping (e.g. it points
    // at index 0 right after a delete, even though no preset's values were
    // applied) and would otherwise let the dropdown claim a preset is loaded
    // when the live parameters don't actually match it.
    const auto entries = fPresetBrowser.getEntries();
    fPresetSelector->setEntries(entries);
    fPresetSelector->setCurrentIndex(fDisplayedPresetIndex);

    const bool isFactory = (fDisplayedPresetIndex >= 0 && static_cast<size_t>(fDisplayedPresetIndex) < entries.size())
                                ? entries[static_cast<size_t>(fDisplayedPresetIndex)].isFactory
                                : true;
    fDeleteButton->setEnabled(!isFactory);
}

void TankUI::onNanoDisplay()
{
    // 1. Background -----------------------------------------------------
    beginPath();
    rect(0.0f, 0.0f, static_cast<float>(getWidth()), static_cast<float>(getHeight()));
    fillColor(toDglColor(kBackgroundColor));
    fill();
    closePath();

    // 2. Bypass label -----------------------------------------------------
    fontFace(NANOVG_DEJAVU_SANS_TTF);
    fontSize(kLabelFontSize);
    fillColor(toDglColor(kTextPrimaryColor));
    textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
    text(static_cast<float>(kBypassX) + static_cast<float>(kBypassSwitchW) * 0.5f, kBypassLabelCenterY, "BYPASS", nullptr);

    // 3. GR meter label ---------------------------------------------------
    textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
    text(static_cast<float>(kMeterX), kMeterLabelCenterY, "GAIN REDUCTION", nullptr);

    // 4. Per-knob labels ----------------------------------------------------
    textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
    for (int i = 0; i < 4; ++i)
    {
        const float cx = static_cast<float>(kKnobSpecs[i]->x) + static_cast<float>(kKnobSize) * 0.5f;
        text(cx, static_cast<float>(kLabelY) + static_cast<float>(kLabelH) * 0.5f, kKnobSpecs[i]->label, nullptr);
    }
}

UI* createUI()
{
    return new TankUI();
}

END_NAMESPACE_DISTRHO
