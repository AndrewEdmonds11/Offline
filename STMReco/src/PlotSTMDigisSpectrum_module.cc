//
// Analyzer module to create a histogram of the STMMWDDigi energies
//
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "cetlib_except/exception.h"
#include "fhiclcpp/types/Atom.h"
#include "canvas/Utilities/InputTag.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "art_root_io/TFileService.h"
#include "Offline/GlobalConstantsService/inc/GlobalConstantsHandle.hh"
#include "Offline/GlobalConstantsService/inc/ParticleDataList.hh"

#include "Offline/MCDataProducts/inc/StepPointMC.hh"
#include <utility>
// root
#include "TH1F.h"
#include "TF1.h"
#include "TTree.h"

#include "Offline/RecoDataProducts/inc/STMMWDDigi.hh"

using namespace std;
using CLHEP::Hep3Vector;
namespace mu2e {

  class PlotSTMDigisSpectrum : public art::EDAnalyzer {
    public:
      using Name=fhicl::Name;
      using Comment=fhicl::Comment;
      struct Config {
       fhicl::Atom<art::InputTag> stmDigisTag{ Name("stmDigisTag"), Comment("InputTag for STMMWDDigiCollection")};
      };
      using Parameters = art::EDAnalyzer::Table<Config>;
      explicit PlotSTMDigisSpectrum(const Parameters& conf);

    private:
    void beginJob() override;
      void analyze(const art::Event& e) override;

    void endJob() override;

    art::InputTag _stmDigisTag;
    TH1D* _adcSpectrum;
    TH1D* _timeSpectrum;
    TH1D* _adcSpectrumOn;
    TH1D* _adcSpectrumOff;
  };

  PlotSTMDigisSpectrum::PlotSTMDigisSpectrum(const Parameters& config )  :
    art::EDAnalyzer{config},
    _stmDigisTag(config().stmDigisTag())
  {
    consumes<STMMWDDigiCollection>(_stmDigisTag);
  }

  void PlotSTMDigisSpectrum::beginJob() {
    art::ServiceHandle<art::TFileService> tfs;
    // create TTree
    _timeSpectrum = tfs->make<TH1D>("timeSpectrum", "ADC Time Difference; Time Difference (ns); Counts", 5000,-300,300);
    _adcSpectrumOn=tfs->make<TH1D>("adcSpectrumOn", "ADC Spectrum Beam On; ADC Sample Number; Counts", 50000,0,50000.0);
    _adcSpectrumOff=tfs->make<TH1D>("adcSpectrumOff", "ADC Spectrum Beam Off; ADC Sample Number; Counts", 50000, 0, 50000.0);
    _adcSpectrum=tfs->make<TH1D>("adcSpectrum", "ADC Spectrum Full; ADC Sample Number; Counts", 50000, 0, 50000.0);
}

  void PlotSTMDigisSpectrum::analyze(const art::Event& event) {
    art::ServiceHandle<art::TFileService> tfs;
    auto digisHandle = event.getValidHandle<STMMWDDigiCollection>(_stmDigisTag);
    //for (const auto& digi : *digisHandle) {
    const auto& digi = *digisHandle;
    int digSize = int (digi.size());
    for(int i = 0; i < digSize - 1; i++) {
      float timeSpectrum = std::fmod(digi[i+1].time() - digi[i].time(), 185);
      _timeSpectrum->Fill(timeSpectrum);
      float adcSpectrum = std::fabs(digi[i].energy());
      _adcSpectrum->Fill(adcSpectrum);

    //float adcSpectrum = std::fabs(digi.energy().at(0));
    // 260 for low rate. 500k for high rate
    //for(int i = 0; i < digSize - 1; i++) {
      if (std::fmod(digi[i+1].time() - digi[i].time(),185) < 100) {
        float adcSpectrum = std::fabs(digi[i].energy());
        _adcSpectrumOn->Fill(adcSpectrum);
      }
      else if (std::fmod(digi[i+1].time() - digi[i].time(),185) > 100) {
        float adcSpectrum = std::fabs(digi[i].energy());
        _adcSpectrumOff->Fill(adcSpectrum);
        }
        }
  }
  void PlotSTMDigisSpectrum::endJob() {
    // Rebin based on number of entries to have at least 10 entries per bin
    /*double histSize = _adcSpectrum->GetEntries();
    double newBinNumber = histSize/10.0;
    int newBinFactor = int (5000.0/newBinNumber);
    std::cout << "New bin factor: " << newBinFactor << std::endl;
    if (newBinFactor > 0)
    {_adcSpectrum->Rebin(newBinFactor); }*/
      }
}

DEFINE_ART_MODULE(mu2e::PlotSTMDigisSpectrum)
