//
// Analyzer module to create a histogram of the STMDigi energies
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

#include "Offline/RecoDataProducts/inc/STMDigi.hh"

using namespace std;
using CLHEP::Hep3Vector;
namespace mu2e {

  class PlotSTMMWDDigis : public art::EDAnalyzer {
    public:
      using Name=fhicl::Name;
      using Comment=fhicl::Comment;
      struct Config {
       fhicl::Atom<art::InputTag> stmDigisTag{ Name("stmDigisTag"), Comment("InputTag for STMDigiCollection")};
      };
      using Parameters = art::EDAnalyzer::Table<Config>;
      explicit PlotSTMMWDDigis(const Parameters& conf);

    private:
    void beginJob() override;
      void analyze(const art::Event& e) override;

    void endJob() override;

    art::InputTag _stmDigisTag;
    TH1D* _baselineMean;
  };

  PlotSTMMWDDigis::PlotSTMMWDDigis(const Parameters& config )  :
    art::EDAnalyzer{config},
    _stmDigisTag(config().stmDigisTag())
  {
    consumes<STMDigiCollection>(_stmDigisTag);
  }

  void PlotSTMMWDDigis::beginJob() {
    art::ServiceHandle<art::TFileService> tfs;
    // create TTree
    _baselineMean=tfs->make<TH1D>("baselineMean", "Energy Spectrum", 5000,0,5000.0);
  }

  void PlotSTMMWDDigis::analyze(const art::Event& event) {
    art::ServiceHandle<art::TFileService> tfs;
    auto digisHandle = event.getValidHandle<STMDigiCollection>(_stmDigisTag);
    //int j = 0;
    const auto& digi = *digisHandle;
    int digiSize = int (digi.size());
    TString fnameOn = Form("DigiSpectrumOn_%d", event.event());
    TString fnameOff = Form("DigiSpectrumOff_%d", event.event());


    // Create On and Off Histograms explicitly
    TH1D* _hWaveformOn = tfs->make<TH1D>(fnameOn,fnameOn+";Time [ns];Samples", 5500928,30701490521,3.07187e10);
    TH1D* _hWaveformOff = tfs->make<TH1D>(fnameOff,fnameOff+";Time [ns];Samples", 5500928,30701490521,3.07187e10);

    // Double check the separation histogram
    TH1D* _hTimeSeparation = tfs->make<TH1D>("hTimeSeparation", "hTimeSeparation; Time [ns];Samples", 6e5,0,6e6);

    for (int j = 0; j < digiSize - 1; j++) {
      //     float baselineMean = digi.baselineMean();
      //_baselineMean->Fill(baselineMean);
      _hTimeSeparation->Fill(digi[j+1].trigTime()-digi[j].trigTime());

      //if(digi[j].trigType().mode() == STMTriggerMode::kExternal)
      // 5e5 for high rate, 260 for low rate
      if((digi[j+1].trigTime() - digi[j].trigTime() < 5e5))
        {
           _hWaveformOn->Fill(digi[j].trigTime(), digi[j].adcs().at(0));
        }
      else
        {
           _hWaveformOff->Fill(digi[j].trigTime(), digi[j].adcs().at(0));
           std::cout << digi[j].trigTime()*3.125<< std::endl;
        }
      /*
      std::cout << "Trig type = " << digi.trigType().data() << std::endl;
      std::cout << "Mode = " << digi.trigType().mode() << std::endl;
      std::cout << "Trig time = " << digi.trigTime() << std::endl;
      */

    }
  }
  void PlotSTMMWDDigis::endJob() {
    // Insert pain (fits) here
    // Goal is to automatically find peaks, these peaks are from the Eu152 source, so...
    /*
      double peaks[4] = [0.123,0.162,0.245,0.344];
    // double peak_energy = 0.344;
      for (int j = 0; j < 4; j++)
      {
       TString fname(Form("fgaus_%d", j));
       TF1* fitGaus = new TF1(fname, "[0]*TMath::Gaus(x,[1],[2])",peaks[j]-0.005,peaks[j]+0.005);
       fitGaus->SetParameters(1700,peaks[j],0.001);
       _baselineMean->Fit(fitGaus,"R+");
      }
    */
  }
}

DEFINE_ART_MODULE(mu2e::PlotSTMMWDDigis)
