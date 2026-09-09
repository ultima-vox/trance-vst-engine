#include "GlobalHeader.h"

namespace vstengine::ui {
GlobalHeader::GlobalHeader (juce::AudioProcessorValueTreeState& s, Callbacks cb) : state(s)
{
    brand.setText("VST ENGINE",juce::dontSendNotification);styleLabel(brand,20,juce::Justification::centredLeft,colours::text);
    preset.setText("Init",juce::dontSendNotification);styleLabel(preset,14,juce::Justification::centredLeft,colours::primary);
    sync.setText("SYNC IDLE",juce::dontSendNotification);styleLabel(sync,12,juce::Justification::centred,colours::mutedText);
    for(auto*l:{&brand,&preset,&sync})addAndMakeVisible(*l);for(auto*b:{&previousButton,&nextButton,&saveButton,&seedButton,&panicButton}){styleButton(*b);addAndMakeVisible(*b);}
    styleButton(panicButton);panicButton.setColour(juce::TextButton::buttonColourId,colours::warning.darker(.45f));
    previousButton.onClick=std::move(cb.previous);nextButton.onClick=std::move(cb.next);saveButton.onClick=std::move(cb.save);panicButton.onClick=std::move(cb.panic);seedButton.onClick=[this]{randomizeSeed();};
    output.setSliderStyle(juce::Slider::LinearHorizontal);output.setTextBoxStyle(juce::Slider::TextBoxRight,false,58,22);output.setTooltip("Bass output level");addAndMakeVisible(output);
    outputAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state,"outputLevel",output);
}
void GlobalHeader::randomizeSeed(){if(auto*p=state.getParameter("rngSeed")){const auto plain=(float)juce::Random::getSystemRandom().nextInt(0x7fffffff);p->setValueNotifyingHost(p->convertTo0to1(plain));}}
void GlobalHeader::setPresetName(const juce::String& n){preset.setText(n.isEmpty()?"Unsaved":n,juce::dontSendNotification);} void GlobalHeader::setTransportActive(bool active){sync.setText(active?"SYNC PLAYING":"SYNC IDLE",juce::dontSendNotification);sync.setColour(juce::Label::textColourId,active?colours::status:colours::mutedText);}
void GlobalHeader::paint(juce::Graphics&g){g.fillAll(colours::panel);g.setColour(colours::border);g.drawLine(0,(float)getHeight()-1,(float)getWidth(),(float)getHeight()-1);}
void GlobalHeader::resized(){auto a=getLocalBounds().reduced(14,8);brand.setBounds(a.removeFromLeft(150));preset.setBounds(a.removeFromLeft(190));previousButton.setBounds(a.removeFromLeft(72).reduced(2));nextButton.setBounds(a.removeFromLeft(72).reduced(2));saveButton.setBounds(a.removeFromLeft(64).reduced(2));seedButton.setBounds(a.removeFromLeft(64).reduced(2));sync.setBounds(a.removeFromLeft(110));panicButton.setBounds(a.removeFromRight(76).reduced(2));output.setBounds(a.removeFromRight(180).reduced(8,2));}
} // namespace vstengine::ui
