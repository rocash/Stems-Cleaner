#include "PluginProcessor.h"
#include "PluginEditor.h"

StemCleanerProAudioProcessor::StemCleanerProAudioProcessor()
: AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
  apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    fifo.fill(0); fftData.fill(0); spectrumMags.fill(-100.0f);
}
StemCleanerProAudioProcessor::~StemCleanerProAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout StemCleanerProAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("stemType",1),"Stem Type",getStemNames(),0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("gateThresh",1),"Gate Thresh",juce::NormalisableRange<float>(-70,0,0.1f),-50.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("cleanAmt",1),"Clean",juce::NormalisableRange<float>(0,100,0.1f),40.0f));
    auto eqRange = juce::NormalisableRange<float>(-12,12,0.1f);
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("lowGain",1),"Low",eqRange,0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("lowMidGain",1),"LowMid",eqRange,0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("midGain",1),"Mid",eqRange,0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("highMidGain",1),"HighMid",eqRange,0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("highGain",1),"High",eqRange,0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("airEqGain",1),"AirEQ",eqRange,0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("clarity",1),"Clarity",juce::NormalisableRange<float>(0,100,0.1f),25.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("warmth",1),"Warmth",juce::NormalisableRange<float>(0,100,0.1f),20.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("attack",1),"Attack",juce::NormalisableRange<float>(-100,100,0.1f),0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("sustain",1),"Sustain",juce::NormalisableRange<float>(-100,100,0.1f),0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("width",1),"Width",juce::NormalisableRange<float>(0,200,0.1f),100.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mix",1),"Mix",juce::NormalisableRange<float>(0,100,0.1f),100.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("outGain",1),"Output",juce::NormalisableRange<float>(-12,12,0.1f),0.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("bypass",1),"Bypass",false));
    return { p.begin(), p.end() };
}

void StemCleanerProAudioProcessor::prepareToPlay(double sr, int bs)
{
    currentSampleRate = sr;
    juce::dsp::ProcessSpec spec{ sr, (juce::uint32)bs, 2 };
    hp.prepare(spec); lowShelfF.prepare(spec); peakLowMid.prepare(spec);
    peakMid.prepare(spec); peakHighMid.prepare(spec); highShelfF.prepare(spec);
    airF.prepare(spec); hissLP.prepare(spec);
    dryBuffer.setSize(2, bs);
    gateEnv=0; gateGain=1; fastEnv[0]=fastEnv[1]=slowEnv[0]=slowEnv[1]=0;
}
void StemCleanerProAudioProcessor::releaseResources() {}
bool StemCleanerProAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet()!=juce::AudioChannelSet::mono() && l.getMainOutputChannelSet()!=juce::AudioChannelSet::stereo()) return false;
    if (l.getMainOutputChannelSet()!=l.getMainInputChannelSet()) return false;
    return true;
}

void StemCleanerProAudioProcessor::updateFilterCoefs(float lowG,float lowMidG,float midG,float highMidG,float highG,float airG,float clarity,float warmth,float cleanAmt)
{
    double sr = currentSampleRate;
    *hp.state = *Coefs::makeHighPass(sr, 30.0f, 0.7f);
    float lowTotal = lowG + warmth*0.05f;
    *lowShelfF.state = *Coefs::makeLowShelf(sr, 120.0, 0.7f, juce::Decibels::decibelsToGain(lowTotal));
    *peakLowMid.state = *Coefs::makePeakFilter(sr, 350.0, 1.0f, juce::Decibels::decibelsToGain(lowMidG));
    float midTotal = midG + clarity*0.04f;
    *peakMid.state = *Coefs::makePeakFilter(sr, 2500.0, 1.0f, juce::Decibels::decibelsToGain(midTotal));
    *peakHighMid.state = *Coefs::makePeakFilter(sr, 6000.0, 1.0f, juce::Decibels::decibelsToGain(highMidG));
    float highTotal = highG + clarity*0.03f;
    *highShelfF.state = *Coefs::makeHighShelf(sr, 10000.0, 0.7f, juce::Decibels::decibelsToGain(highTotal));
    *airF.state = *Coefs::makeHighShelf(sr, 14000.0, 0.7f, juce::Decibels::decibelsToGain(airG));
    float lpFreq = 20000.0f - cleanAmt*150.0f; // 20k -> 5k
    lpFreq = juce::jlimit(4000.0f, 20000.0f, lpFreq);
    *hissLP.state = *Coefs::makeLowPass(sr, lpFreq, 0.7f);
}

void StemCleanerProAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals d;
    int chans = getTotalNumOutputChannels();
    int n = buffer.getNumSamples();
    for (int i=getTotalNumInputChannels(); i<chans; ++i) buffer.clear(i,0,n);

    if (auto* bp = apvts.getRawParameterValue("bypass")) if (bp->load()>0.5f) return;

    inLevel.store(buffer.getRMSLevel(0,0,n));
    dryBuffer.makeCopyOf(buffer,true);
    // spectrum + learn usan mono del ch0
    if (buffer.getNumChannels()>0) pushSpectrum(buffer.getReadPointer(0), n);

    float gateThresh = apvts.getRawParameterValue("gateThresh")->load();
    float cleanAmt = apvts.getRawParameterValue("cleanAmt")->load();
    float lowG = apvts.getRawParameterValue("lowGain")->load();
    float lowMidG = apvts.getRawParameterValue("lowMidGain")->load();
    float midG = apvts.getRawParameterValue("midGain")->load();
    float highMidG = apvts.getRawParameterValue("highMidGain")->load();
    float highG = apvts.getRawParameterValue("highGain")->load();
    float airG = apvts.getRawParameterValue("airEqGain")->load();
    float clarity = apvts.getRawParameterValue("clarity")->load();
    float warmth = apvts.getRawParameterValue("warmth")->load();
    float attackAmt = apvts.getRawParameterValue("attack")->load()/100.0f;
    float sustainAmt = apvts.getRawParameterValue("sustain")->load()/100.0f;
    float width = apvts.getRawParameterValue("width")->load()/100.0f;
    float mix = apvts.getRawParameterValue("mix")->load()/100.0f;
    float outDb = apvts.getRawParameterValue("outGain")->load();

    // LEARN: mide nivel medio 2 seg y auto-ajusta gate
    if (learning.load())
    {
        float rms = buffer.getRMSLevel(0,0,n);
        learnSum += rms; learnCount++;
        if (++learnSamples > (int)(currentSampleRate*2.0/n))
        {
            float avg = learnSum / juce::jmax(1,learnCount);
            float db = juce::Decibels::gainToDecibels(avg, -80.0f);
            learnedNoiseDb.store(db);
            float newThresh = juce::jlimit(-70.0f,0.0f, db+8.0f);
            if (auto* par = apvts.getParameter("gateThresh"))
                par->setValueNotifyingHost(par->getNormalisableRange().convertTo0to1(newThresh));
            learning.store(false);
        }
    }

    updateFilterCoefs(lowG,lowMidG,midG,highMidG,highG,airG,clarity,warmth,cleanAmt);

    // 1. GATE simple
    {
        float thrLin = juce::Decibels::decibelsToGain(gateThresh);
        float att = expf(-1.0f/(float)currentSampleRate*1000.0f/2.0f);
        float rel = expf(-1.0f/(float)currentSampleRate*1000.0f/100.0f);
        float rangeLin = juce::Decibels::decibelsToGain(-60.0f);
        for (int s=0;s<n;++s){
            float peak=0; for(int c=0;c<buffer.getNumChannels();++c) peak=juce::jmax(peak,std::abs(buffer.getSample(c,s)));
            if (peak>gateEnv) gateEnv = att*gateEnv+(1-att)*peak; else gateEnv = rel*gateEnv+(1-rel)*peak;
            float target = gateEnv>thrLin?1.0f:rangeLin;
            float coeff = target<gateGain?rel:att;
            gateGain = coeff*gateGain+(1-coeff)*target;
            // cleanAmt cierra mas el gate en silencios
            float gateMix = 0.5f + cleanAmt/200.0f;
            float g = 1.0f-(1.0f-gateGain)*gateMix;
            for(int c=0;c<buffer.getNumChannels();++c) buffer.setSample(c,s,buffer.getSample(c,s)*g);
        }
    }

    // 2. EQ + DeHiss via dsp chain
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        hp.process(ctx); lowShelfF.process(ctx); peakLowMid.process(ctx);
        peakMid.process(ctx); peakHighMid.process(ctx); highShelfF.process(ctx);
        airF.process(ctx);
        if (cleanAmt>1.0f) hissLP.process(ctx);
    }

    // 3. Warmth saturacion suave + Transient
    for(int c=0;c<buffer.getNumChannels();++c){
        auto* dta = buffer.getWritePointer(c);
        int chIdx = juce::jmin(c,1);
        for(int s=0;s<n;++s){
            float x = dta[s];
            if (warmth>1.0f){ float k=warmth/100.0f*0.6f; float wet=std::tanh(x*(1.0f+k)); x=x*(1.0f-k*0.4f)+wet*(k*0.4f); }
            float ax=std::abs(x);
            float fa=expf(-1.0f/(float)currentSampleRate*1000.0f/1.0f);
            float fr=expf(-1.0f/(float)currentSampleRate*1000.0f/8.0f);
            float sa=expf(-1.0f/(float)currentSampleRate*1000.0f/20.0f);
            float sr_=expf(-1.0f/(float)currentSampleRate*1000.0f/120.0f);
            fastEnv[chIdx] = ax>fastEnv[chIdx]?fa*fastEnv[chIdx]+(1-fa)*ax:fr*fastEnv[chIdx]+(1-fr)*ax;
            slowEnv[chIdx] = ax>slowEnv[chIdx]?sa*slowEnv[chIdx]+(1-sa)*ax:sr_*slowEnv[chIdx]+(1-sr_)*ax;
            float gain=1.0f;
            if (slowEnv[chIdx]>0.0001f){
                float tr = juce::jmax(0.0f,fastEnv[chIdx]-slowEnv[chIdx])/juce::jmax(0.001f,slowEnv[chIdx]);
                float tg = juce::jlimit(0.3f,2.5f,1.0f+attackAmt*tr);
                float sg = juce::jlimit(0.3f,2.0f,1.0f+sustainAmt*0.4f);
                float ratio = juce::jlimit(0.0f,1.0f,tr);
                gain = tg*ratio+sg*(1.0f-ratio);
            }
            dta[s]=x*gain;
        }
    }

    // 4. Stereo width Mid/Side
    if (buffer.getNumChannels()>=2 && std::abs(width-1.0f)>0.01f){
        auto* L=buffer.getWritePointer(0); auto* R=buffer.getWritePointer(1);
        for(int s=0;s<n;++s){ float m=(L[s]+R[s])*0.5f; float sd=(L[s]-R[s])*0.5f*width; L[s]=m+sd; R[s]=m-sd; }
    }

    // 5. Mix + Output
    if (mix<0.999f) for(int c=0;c<buffer.getNumChannels();++c){
        auto* w=buffer.getWritePointer(c); auto* dr=dryBuffer.getReadPointer(c);
        for(int s=0;s<n;++s) w[s]=dr[s]*(1.0f-mix)+w[s]*mix;
    }
    buffer.applyGain(juce::Decibels::decibelsToGain(outDb));
    outLevel.store(buffer.getRMSLevel(0,0,n));
}

void StemCleanerProAudioProcessor::pushSpectrum(const float* s,int n)
{
    for(int i=0;i<n;++i){ fifo[fifoIndex++]=s[i]; if(fifoIndex>=fftSize){ doSpectrumFFT(); fifoIndex=0; } }
    if (learning.load()){ /* already handled */ }
}
void StemCleanerProAudioProcessor::doSpectrumFFT()
{
    std::copy(fifo.begin(),fifo.end(),fftData.begin());
    std::fill(fftData.begin()+fftSize,fftData.end(),0.0f);
    window.multiplyWithWindowingTable(fftData.data(),fftSize);
    fft.performRealOnlyForwardTransform(fftData.data(),true);
    juce::ScopedLock lk(specLock);
    for(int i=0;i<fftSize/2;++i){
        float re=fftData[i*2], im=fftData[i*2+1];
        float m=std::sqrt(re*re+im*im)/ (float)fftSize;
        float db=juce::Decibels::gainToDecibels(m,-100.0f);
        spectrumMags[i]=db;
    }
}
void StemCleanerProAudioProcessor::getSpectrumData(float* dest,int numBins)
{
    juce::ScopedLock lk(specLock);
    int m=juce::jmin(numBins,(int)spectrumMags.size());
    for(int i=0;i<m;++i) dest[i]=spectrumMags[i];
}
juce::String StemCleanerProAudioProcessor::getProblemsText()
{
    juce::ScopedLock lk(specLock);
    auto binHz = [&](float hz){ return juce::jlimit(1,(int)spectrumMags.size()-1,(int)(hz*fftSize/currentSampleRate)); };
    auto avgBand = [&](float f1,float f2){
        int b1=binHz(f1),b2=binHz(f2); float s=0; int c=0;
        for(int i=b1;i<=b2;++i){ s+=spectrumMags[i]; c++; } return c>0?s/c:-100.0f;
    };
    float rumble=avgBand(25,45), mud=avgBand(200,500), hiss=avgBand(8000,16000), low=avgBand(40,120);
    juce::String t;
    if (rumble>-55) t+="RUMBLE <45Hz ";
    if (mud>-45 && mud>low-6) t+="BARRO 200-500Hz ";
    if (hiss>-58) t+="HISS >8kHz ";
    if (t.isEmpty()) t="OK - sin problemas graves";
    else t="Detectado: "+t;
    if (learning.load()) t="APRENDIENDO RUIDO... toca una parte con solo ruido";
    return t;
}

void StemCleanerProAudioProcessor::startNoiseLearn(){ learnSamples=0; learnSum=0; learnCount=0; learning.store(true); }
void StemCleanerProAudioProcessor::stopNoiseLearn(){ learning.store(false); }

struct PresetVals{ float gt,cl,lo,lm,mi,hm,hi,air,cla,war,atk,sus,wid; };
void StemCleanerProAudioProcessor::applyStemPreset(int idx)
{
    PresetVals v{ -50,40,0,0,0,0,0,0,25,20,0,0,100 };
    switch(idx){
        case 0: v={-40,40,3,-3,2,0,0,0,15,40,30,-10,50}; break; // Kick
        case 1: v={-55,35,2.5f,-2,1.5f,1,0,0,20,50,15,10,60}; break; // Bass
        case 2: v={-50,45,-1,-1.5f,2,1.5f,0,-1,35,25,10,5,110}; break; // Guitar
        case 3: v={-60,30,1,-1.5f,1.5f,0,2,1,40,15,5,15,140}; break; // Synth
        case 4: v={-45,55,0,-3,-1,2,3,1.5f,50,0,40,-20,130}; break; // Hats
        case 5: v={-45,45,2,-2.5f,1.5f,2,1.5f,0,30,20,25,-5,120}; break; // Drums
        case 6: v={-60,35,1,-1.5f,1.5f,1,0.5f,0,30,35,10,20,120}; break; // Piano
        case 7: v={-48,50,-1,-1,2.5f,1.5f,1,0,45,25,5,10,90}; break; // Voice
        default: break;
    }
    auto setF=[this](juce::String id,float val){ if(auto*par=apvts.getParameter(id)) par->setValueNotifyingHost(par->getNormalisableRange().convertTo0to1(val)); };
    setF("gateThresh",v.gt); setF("cleanAmt",v.cl); setF("lowGain",v.lo); setF("lowMidGain",v.lm);
    setF("midGain",v.mi); setF("highMidGain",v.hm); setF("highGain",v.hi); setF("airEqGain",v.air);
    setF("clarity",v.cla); setF("warmth",v.war); setF("attack",v.atk); setF("sustain",v.sus); setF("width",v.wid);
}

juce::AudioProcessorEditor* StemCleanerProAudioProcessor::createEditor() { return new StemCleanerProEditor(*this); }
bool StemCleanerProAudioProcessor::hasEditor() const { return true; }
const juce::String StemCleanerProAudioProcessor::getName() const { return JucePlugin_Name; }
bool StemCleanerProAudioProcessor::acceptsMidi() const { return false; }
bool StemCleanerProAudioProcessor::producesMidi() const { return false; }
bool StemCleanerProAudioProcessor::isMidiEffect() const { return false; }
double StemCleanerProAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int StemCleanerProAudioProcessor::getNumPrograms() { return 1; }
int StemCleanerProAudioProcessor::getCurrentProgram() { return 0; }
void StemCleanerProAudioProcessor::setCurrentProgram(int) {}
const juce::String StemCleanerProAudioProcessor::getProgramName(int) { return {}; }
void StemCleanerProAudioProcessor::changeProgramName(int, const juce::String&) {}
void StemCleanerProAudioProcessor::getStateInformation(juce::MemoryBlock& d){ auto s=apvts.copyState(); std::unique_ptr<juce::XmlElement> x(s.createXml()); copyXmlToBinary(*x,d); }
void StemCleanerProAudioProcessor::setStateInformation(const void* data,int sz){ std::unique_ptr<juce::XmlElement> x(getXmlFromBinary(data,sz)); if(x&&x->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*x)); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){ return new StemCleanerProAudioProcessor(); }
