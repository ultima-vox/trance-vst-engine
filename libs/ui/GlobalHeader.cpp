#include "GlobalHeader.h"

namespace vstengine::ui {
GlobalHeader::GlobalHeader (juce::AudioProcessorValueTreeState& s, Callbacks cb) : state(s)
{
    brand.setText("VOX ELECTRONIC ENGINE",juce::dontSendNotification);styleLabel(brand,19,juce::Justification::centredLeft,colours::text);
    preset.setTextWhenNothingSelected("Init");
    midi.setText("MIDI",juce::dontSendNotification);styleLabel(midi,11,juce::Justification::centred,colours::mutedText);
    cpu.setText("CPU 0%",juce::dontSendNotification);styleLabel(cpu,11,juce::Justification::centred,colours::mutedText);
    outputLabel.setText("OUTPUT",juce::dontSendNotification);styleLabel(outputLabel,10,juce::Justification::centredRight,colours::mutedText);
    for(auto*l:{&brand,&midi,&cpu,&outputLabel})addAndMakeVisible(*l);addAndMakeVisible(preset);for(auto*b:{&previousButton,&nextButton,&saveButton,&seedButton,&panicButton,&settingsButton}){styleButton(*b);addAndMakeVisible(*b);}
    styleButton(panicButton);panicButton.setColour(juce::TextButton::buttonColourId,colours::warning.darker(.45f));
    previousButton.onClick=std::move(cb.previous);nextButton.onClick=std::move(cb.next);saveButton.onClick=std::move(cb.save);panicButton.onClick=std::move(cb.panic);settingsButton.onClick=std::move(cb.settings);preset.onChange=[this,choose=std::move(cb.choosePreset)]{if(choose)choose(preset.getSelectedItemIndex());};seedButton.onClick=[this]{randomizeSeed();};
    output.setSliderStyle(juce::Slider::LinearHorizontal);output.setTextBoxStyle(juce::Slider::TextBoxRight,false,58,22);output.setTooltip("Global output level");addAndMakeVisible(output);
    outputAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state,"outputLevel",output);
}
void GlobalHeader::randomizeSeed(){if(auto*p=state.getParameter("rngSeed")){const auto plain=(float)juce::Random::getSystemRandom().nextInt(0x7fffffff);p->setValueNotifyingHost(p->convertTo0to1(plain));}}
void GlobalHeader::setPresetName(const juce::String& n){preset.setText(n.isEmpty()?"Unsaved":n,juce::dontSendNotification);} void GlobalHeader::setTransportActive(bool){}
void GlobalHeader::setPresetEntries(const juce::StringArray& names,int selectedIndex){if(presetNames!=names){presetNames=names;preset.clear(juce::dontSendNotification);preset.addItemList(names,1);}preset.setSelectedItemIndex(selectedIndex,juce::dontSendNotification);}
void GlobalHeader::setMidiActivity(bool active){midi.setText(active?"MIDI +":"MIDI",juce::dontSendNotification);midi.setColour(juce::Label::textColourId,active?colours::status:colours::mutedText);}
void GlobalHeader::setCpuLoad(float load){load=juce::jlimit(0.0f,4.0f,load);cpu.setText("CPU "+juce::String(juce::roundToInt(load*100.0f))+"%",juce::dontSendNotification);cpu.setColour(juce::Label::textColourId,load>.8f?colours::warning:colours::mutedText);if(const auto*v=state.getRawParameterValue("rngSeed"))seedButton.setButtonText("Seed "+juce::String(juce::roundToInt(v->load())));}
void GlobalHeader::paint(juce::Graphics&g){g.fillAll(colours::panel);g.setColour(colours::border);g.drawLine(0,(float)getHeight()-1,(float)getWidth(),(float)getHeight()-1);}
void GlobalHeader::resized(){auto a=getLocalBounds().reduced(14,8);brand.setBounds(a.removeFromLeft(220));previousButton.setBounds(a.removeFromLeft(38).reduced(2));preset.setBounds(a.removeFromLeft(160).reduced(2));nextButton.setBounds(a.removeFromLeft(38).reduced(2));saveButton.setBounds(a.removeFromLeft(58).reduced(2));seedButton.setBounds(a.removeFromLeft(82).reduced(2));midi.setBounds(a.removeFromLeft(52));cpu.setBounds(a.removeFromLeft(58));settingsButton.setBounds(a.removeFromRight(78).reduced(2));panicButton.setBounds(a.removeFromRight(64).reduced(2));output.setBounds(a.removeFromRight(112).reduced(4,2));outputLabel.setBounds(a.removeFromRight(54));}
} // namespace vstengine::ui
