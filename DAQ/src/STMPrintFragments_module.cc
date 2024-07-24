// ======================================================================
//
// STMPrintFragments_plugin:  Add CRV data products to the event
//
// ======================================================================

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "fhiclcpp/ParameterSet.h"

#include "Offline/ProditionsService/inc/ProditionsHandle.hh"
#include "art/Framework/Principal/Handle.h"
#include "artdaq-core-mu2e/Overlays/STMFragment.hh"
#include <artdaq-core/Data/Fragment.hh>

#include <iostream>
#include <string>
#include <memory>

namespace art
{
  class STMPrintFragments;
}

using art::STMPrintFragments;

// ======================================================================

class art::STMPrintFragments : public EDAnalyzer
{
  public:
  struct Config
   {
  //   //    fhicl::Atom<int> diagLevel{fhicl::Name("diagLevel"), fhicl::Comment("diagnostic Level")};
  //   //    fhicl::Atom<art::InputTag> CRVDataDecodersTag{fhicl::Name("crvTag"),
  //   //                                               fhicl::Comment("crv Fragments Tag")};
  };

  // --- C'tor/d'tor:
  explicit STMPrintFragments(const art::EDAnalyzer::Table<Config>& config);

  // --- Production:
  virtual void analyze(const Event&);

  private:
  //  int decompressCrvDigi(uint8_t adc);
  //  int16_t decompressCrvDigi(int16_t adc);

  //  int                                      _diagLevel;
  //  art::InputTag                            _CRVDataDecodersTag;
  //  mu2e::ProditionsHandle<mu2e::CRVOrdinal> _channelMap_h;

}; // STMPrintFragments

// ======================================================================

STMPrintFragments::STMPrintFragments(const art::EDAnalyzer::Table<Config>& config) :
    art::EDAnalyzer{config}
{
  //  produces<mu2e::CrvDigiCollection>();
}

// ----------------------------------------------------------------------

void STMPrintFragments::analyze(const Event& event)
{
  art::EventNumber_t eventNumber = event.event();

  auto STMFragments = event.getValidHandle<artdaq::Fragments>("daq:STM");

  std::cout << std::dec << "Analyzer: Run " << event.run() << ", subrun " << event.subRun()
	    << ", event " << eventNumber << " has " << std::endl;
  std::cout << STMFragments->size() << " STM fragments." << std::endl;

  int frag_counter = 0;
  for (auto& frag : *STMFragments) {
    ++frag_counter;
    // const auto dataBegin = frag.dataBegin();
    // const auto stmDataBegin = reinterpret_cast<int16_t const*>(dataBegin);
    // for (size_t i = 0; i < 10; ++i) {
    //   std::cout << "*(stmDataBegin+" << i << ") = " << *(stmDataBegin+i) << std::endl;
    // }
    auto stm_frag = static_cast<mu2e::STMFragment>(frag);
    //    std::cout << "Trigger Header Address: " << stm_frag.GetTHdr() << std::endl;
    std::cout << "Frag #" << frag_counter << ": First 32 int16s: ";
    for (size_t i = 0; i < 32; ++i) {
      std::cout << *(stm_frag.GetTHdr()+i) << " ";
    }
    std::cout << std::endl;
    //    std::cout << "Trigger Header Channel: " << *(stm_frag.GetTHdr()) << std::endl;
    //    std::cout << "Trigger Header EvNum: " << *(stm_frag.GetTHdr()+8) << std::endl;
  }

} // produce()

// ======================================================================

DEFINE_ART_MODULE(STMPrintFragments)
