//
// Create zero-suppressed STMDigis from unsuppressed STMDigis
//
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "cetlib_except/exception.h"
#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/Sequence.h"
#include "canvas/Utilities/InputTag.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "art_root_io/TFileService.h"

#include <utility>
#include <algorithm>

// root
#include "TH1F.h"
#include "TF1.h"

#include "Offline/RecoDataProducts/inc/STMDigi.hh"
#include "Offline/STMReco/inc/ZPAlg.hh"

namespace mu2e {

  class STMPedestalSubtraction : public art::EDProducer {
    public:
      using Name=fhicl::Name;
      using Comment=fhicl::Comment;
      struct Config {
        fhicl::Atom<art::InputTag> stmDigisTag{ Name("stmDigisTag"), Comment("InputTag for STMDigiCollection")};
        fhicl::Atom<int16_t> pedestal{Name("pedestal"), Comment("Pedestal value")}; // TODO: get from DB
        fhicl::Atom<int> verbosityLevel{Name("verbosityLevel"), Comment("Verbosity level")};
      };
      using Parameters = art::EDProducer::Table<Config>;
      explicit STMPedestalSubtraction(const Parameters& conf);

    private:
    void beginJob() override;
    void produce(art::Event& e) override;

    art::InputTag _stmDigisTag;
    int _verbosityLevel;
    int16_t _pedestal;
  };

  STMPedestalSubtraction::STMPedestalSubtraction(const Parameters& config )  :
    art::EDProducer{config}
    ,_stmDigisTag(config().stmDigisTag())
    ,_verbosityLevel(config().verbosityLevel())
    ,_pedestal(config().pedestal())
  {
    consumes<STMDigiCollection>(_stmDigisTag);
    produces<STMDigiCollection>();
  }

  void STMPedestalSubtraction::beginJob() {
  }

  void STMPedestalSubtraction::produce(art::Event& event) {
    // create output
    auto digisHandle = event.getValidHandle<STMDigiCollection>(_stmDigisTag);
    unique_ptr<STMDigiCollection> outputSTMDigis(new STMDigiCollection());

    //    if (_verbosityLevel > 0) {
    //    }

    for (const auto& digi : *digisHandle) {
      std::vector<int16_t> pedsub_adcs;
      pedsub_adcs.reserve(digi.adcs().size());
      for (const auto& adc : digi.adcs()) {
        // if we have truncated the pulse, then subtracting the pedestal will just roll us over
        if (adc - _pedestal > std::numeric_limits<int16_t>::min()) {
          pedsub_adcs.push_back(adc - _pedestal);
        }
        else {
          pedsub_adcs.push_back(std::numeric_limits<int16_t>::min());
        }
      }
      STMDigi stm_digi(STMTrigType(digi.trigType().mode(), digi.trigType().channel().id(), STMDataType::kUnsuppressed),digi.trigTime(),digi.trigTimeOffset(),0,STMDigiFlag::kOK, pedsub_adcs);
      outputSTMDigis->push_back(stm_digi);
    }

    if (_verbosityLevel > 0) {
      std::cout << outputSTMDigis->size() << " digis found" << std::endl;
    }
    event.put(std::move(outputSTMDigis));
  }
}

DEFINE_ART_MODULE(mu2e::STMPedestalSubtraction)
