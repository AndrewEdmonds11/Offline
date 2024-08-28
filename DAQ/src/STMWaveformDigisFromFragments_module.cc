// ======================================================================
//
// STMWaveformDigisFromFragments: create STMWaveformDigis from STMFragments
//
// ======================================================================

#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "fhiclcpp/ParameterSet.h"

#include "Offline/ProditionsService/inc/ProditionsHandle.hh"
#include "Offline/RecoDataProducts/inc/STMWaveformDigi.hh"
#include "art/Framework/Principal/Handle.h"
#include "artdaq-core-mu2e/Overlays/STMFragment.hh"
#include <artdaq-core/Data/Fragment.hh>

#include <iostream>
#include <string>
#include <memory>

namespace art
{
  class STMWaveformDigisFromFragments;
}

using art::STMWaveformDigisFromFragments;

// ======================================================================

class art::STMWaveformDigisFromFragments : public EDProducer
{
  public:
  struct Config
  {
    fhicl::Atom<int> diagLevel{fhicl::Name("diagLevel"), fhicl::Comment("diagnostic Level")};
    // fhicl::Atom<art::InputTag> CRVDataDecodersTag{fhicl::Name("crvTag"),
    //                                            fhicl::Comment("crv Fragments Tag")};
  };

  // --- C'tor/d'tor:
  explicit STMWaveformDigisFromFragments(const art::EDProducer::Table<Config>& config);

  // --- Production:
  virtual void produce(Event&);

  private:
    int diagLevel_;
}; // STMWaveformDigisFromFragments

// ======================================================================

STMWaveformDigisFromFragments::STMWaveformDigisFromFragments(const art::EDProducer::Table<Config>& config) :
    art::EDProducer{config}, diagLevel_(config().diagLevel())
{
  produces<mu2e::STMWaveformDigiCollection>("Raw");
  produces<mu2e::STMWaveformDigiCollection>("ZeroSuppressed");
}

// ----------------------------------------------------------------------


void STMWaveformDigisFromFragments::produce(Event& event)
{

  auto STMFragments = event.getValidHandle<artdaq::Fragments>("daq:STM"); // TODO: make this a fhicl parameter
//  std::unique_ptr<mu2e::STMWaveformDigiCollection> stm_waveform_digis(new mu2e::STMWaveformDigiCollection);

  std::unique_ptr<mu2e::STMWaveformDigiCollection> stm_waveform_digis_raw(new mu2e::STMWaveformDigiCollection);
  std::unique_ptr<mu2e::STMWaveformDigiCollection> stm_waveform_digis_zp(new mu2e::STMWaveformDigiCollection);

  for (auto& frag : *STMFragments) {
    auto stm_frag = static_cast<mu2e::STMFragment>(frag);

    unsigned long int n_adc_samples = 30000; // TODO: change this hard-coded length to something else
    std::vector<int16_t> adcs;
    adcs.reserve(n_adc_samples);
    for (unsigned long int i_adc_sample = 0; i_adc_sample < n_adc_samples; ++i_adc_sample) {
      auto next_adc = *(stm_frag.DataBegin()+i_adc_sample);
      adcs.emplace_back(next_adc);
    }

    // Create the STMWaveformDigi and put it in the event
    uint32_t trig_time_offset = 0;
    mu2e::STMWaveformDigi stm_waveform(trig_time_offset, adcs);
//    stm_waveform_digis->push_back(stm_waveform);

    int16_t zpflag = *(stm_frag.DataType());
    if (zpflag == 1){
      //std::cout <<  "zero supressed waveform digi, zpflag: " << zpflag << std::endl;
      stm_waveform_digis_zp->push_back(stm_waveform);
    }
    else if(zpflag == 0){
      //std::cout <<  "raw waveform digi, zpflag: " << zpflag << std::endl;
      stm_waveform_digis_raw->push_back(stm_waveform);
    }

// temporary prototype testing
    if (diagLevel_ > 0){
      std::cout << "EvNum #" << *(stm_frag.EvNum()) << ": ZPFlag: " << *(stm_frag.DataType()) << std::endl;
//    std::cout << "raw collection" << stm_waveform_digis_raw->Print() << std::endl;
      for (auto i: *stm_waveform_digis_zp){
        //std::cout << i.adcs() << std::endl;
        for (int j=0; j<4; ++j) {
          std::cout << i.adcs()[j] << std::endl;
        }
      }
    }
  }

//  event.put(std::move(stm_waveform_digis));
  event.put(std::move(stm_waveform_digis_raw), "Raw");
  event.put(std::move(stm_waveform_digis_zp), "ZeroSuppressed");

} // produce()

// ======================================================================

DEFINE_ART_MODULE(STMWaveformDigisFromFragments)
