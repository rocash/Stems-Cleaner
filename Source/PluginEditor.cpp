#include "PluginEditor.h"

StemCleanerProEditor::StemCleanerProEditor(StemCleanerProAudioProcessor& p)
: AudioProcessorEditor(&p), proc(p), specComp(p)
{
    setSize(1020,700);
    auto& ap = p.getAPVTS();
    stemBox.addItemList(StemCleanerProAudioProcessor::getStemNames(),1);
    stemBox.setSelectedId(1); addAndMakeVisible(stemBox);
    aStem = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(ap,"stemType",stemBox);
    stemBox.onChange=[this]{ int id=stemBox.getSelectedItemIndex(); if(id>=0) proc.applyStemPreset(id); };

    learnBtn.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff0f3460));
    learnBtn.onClick=[this]{ if(proc.isLearningNoise()) proc.stopNoiseLearn(); else proc.startNoiseLearn(); };
    addAndMakeVisible(learnBtn);

    bypassBtn.setClickingTogglesState(true); addAndMakeVisible(bypassBtn);
    aBypass = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(ap,"bypass",bypassBtn);

    problemsLabel.setColour(juce::Label::backgroundColourId,juce::Colour(0xff111122));
    problemsLabel.setColour(juce::Label::textColourId,juce::Colour(0xffff5a5a));
    problemsLabel.setFont(juce::Font(13.0f)); addAndMakeVisible(problemsLabel);
    addAndMakeVisible(specComp);

    setupSlider(sGate,lGate,"Gate","gateThresh"); setupSlider(sClean,lClean,"Clean","cleanAmt");
    setupSlider(sLow,lLow,"Low","lowGain"); setupSlider(sLowMid,lLowMid,"LowMid","lowMidGain");
    setupSlider(sMid,lMid,"Mid","midGain"); setupSlider(sHighMid,lHighMid,"HighMid","highMidGain");
    setupSlider(sHigh,lHigh,"High","highGain"); setupSlider(sAir,lAir,"AirEQ","airEqGain");
    setupSlider(sCla,lCla,"Clarity","clarity"); setupSlider(sWar,lWar,"Warmth","warmth");
    setupSlider(sAtt,lAtt,"Attack","attack"); setupSlider(sSus,lSus,"Sustain","sustain");
    setupSlider(sWid,lWid,"Width","width"); setupSlider(sMix,lMix,"Mix","mix"); setupSlider(sOut,lOut,"Out","outGain");
    startTimerHz(30);
}
StemCleanerProEditor::~StemCleanerProEditor(){ stopTimer(); }
void StemCleanerProEditor::setupSlider(juce::Slider& s,juce::Label& l,juce::String name,juce::String pid)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,64,18);
    s.setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xffe94560));
    addAndMakeVisible(s); l.setText(name,juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred); l.attachToComponent(&s,false);
    sAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.getAPVTS(),pid,s));
}
void StemCleanerProEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));
    g.setColour(juce::Colour(0xffe94560)); g.setFont(juce::Font(22.0f,juce::Font::bold));
    g.drawText("STEM CLEANER PRO",15,8,400,26,juce::Justification::left);
}
void StemCleanerProEditor::resized()
{
    auto b=getLocalBounds(); b.removeFromTop(40);
    auto top=b.removeFromTop(40); top= top.reduced(8,2);
    stemBox.setBounds(top.removeFromLeft(160)); top.removeFromLeft(8);
    learnBtn.setBounds(top.removeFromLeft(140)); top.removeFromLeft(8);
    bypassBtn.setBounds(top.removeFromLeft(90));
    problemsLabel.setBounds(b.removeFromTop(28).reduced(8,2));
    specComp.setBounds(b.removeFromTop(220).reduced(8,4));
    auto grid=b.reduced(8,4);
    std::vector<juce::Slider*> sl={&sGate,&sClean,&sLow,&sLowMid,&sMid,&sHighMid,&sHigh,&sAir,&sCla,&sWar,&sAtt,&sSus,&sWid,&sMix,&sOut};
    int n=(int)sl.size(); int w=grid.getWidth()/5; int h=grid.getHeight()/3;
    for(int i=0;i<n;++i){ int r=i/5,c=i%5; sl[i]->setBounds(grid.getX()+c*w+4,grid.getY()+r*h+12,w-8,h-24); }
}
void StemCleanerProEditor::timerCallback()
{
    specComp.repaint();
    problemsLabel.setText(proc.getProblemsText(),juce::dontSendNotification);
    learnBtn.setButtonText(proc.isLearningNoise()?"LEARNING... (click stop)":"LEARN NOISE");
    learnBtn.setColour(juce::TextButton::buttonColourId, proc.isLearningNoise()?juce::Colour(0xffe94560):juce::Colour(0xff0f3460));
}
void StemCleanerProEditor::SpecComp::paint(juce::Graphics& g)
{
    auto b=getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff0a0a1a)); g.fillRoundedRectangle(b,6.0f);
    const int N=1024; static float mags[1024];
    p.getSpectrumData(mags,N);
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    for(auto f: {50.f,100.f,200.f,500.f,1000.f,2000.f,5000.f,10000.f}){
        float x=b.getX()+b.getWidth()*(std::log10(f/20.f)/std::log10(20000.f/20.f));
        g.drawVerticalLine((int)x,b.getY(),b.getBottom());
    }
    juce::Path path; bool st=false;
    double sr=44100.0;
    for(int i=2;i<N/2;++i){
        float freq=(float)i* (float)sr/(float)StemCleanerProAudioProcessor::fftSize;
        if(freq<20||freq>20000) continue;
        float x=b.getX()+b.getWidth()*(std::log10(freq/20.f)/std::log10(20000.f/20.f));
        float db=juce::jlimit(-100.f,0.f,mags[i]);
        float y=b.getBottom()-(db+100.f)/100.f*b.getHeight();
        if(!st){path.startNewSubPath(x,y);st=true;} else path.lineTo(x,y);
    }
    g.setColour(juce::Colour(0xffe94560)); g.strokePath(path,juce::PathStrokeType(2.0f));
}
