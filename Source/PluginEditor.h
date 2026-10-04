#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class StemCleanerProEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    StemCleanerProEditor(StemCleanerProAudioProcessor&);
    ~StemCleanerProEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
private:
    StemCleanerProAudioProcessor& proc;
    juce::ComboBox stemBox;
    juce::TextButton learnBtn{"LEARN NOISE"};
    juce::ToggleButton bypassBtn{"Bypass"};
    juce::Label problemsLabel;

    struct SpecComp : public juce::Component {
        StemCleanerProAudioProcessor& p; SpecComp(StemCleanerProAudioProcessor& pr):p(pr){}
        void paint(juce::Graphics& g) override;
    } specComp;

    juce::Slider sGate,sClean,sLow,sLowMid,sMid,sHighMid,sHigh,sAir,sCla,sWar,sAtt,sSus,sWid,sMix,sOut;
    juce::Label lGate,lClean,lLow,lLowMid,lMid,lHighMid,lHigh,lAir,lCla,lWar,lAtt,lSus,lWid,lMix,lOut;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> aStem;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aBypass;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sAtt;
    void setupSlider(juce::Slider&,juce::Label&,juce::String name,juce::String pid);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemCleanerProEditor)
};
