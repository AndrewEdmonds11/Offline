//
// This module transforms StrawDigi objects into StrawHit objects
//
// $Id: StrawHitReco_module.cc,v 1.12 2014/03/25 22:14:39 brownd Exp $
// $Author: brownd $ 
// $Date: 2014/03/25 22:14:39 $
//
// Original author David Brown, LBNL
// Merged with flag and position creation B. Echenard, CalTech
//
// framework
#include "art/Framework/Principal/Event.h"
#include "fhiclcpp/ParameterSet.h"
#include "art/Framework/Principal/Handle.h"
#include "GeometryService/inc/GeomHandle.hh"
#include "art/Framework/Core/EDProducer.h"
#include "GeometryService/inc/DetectorSystem.hh"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Services/Optional/TFileService.h"

// conditions
#include "ConditionsService/inc/ConditionsHandle.hh"
#include "ConditionsService/inc/AcceleratorParams.hh"
#include "ConditionsService/inc/TrackerCalibrations.hh"
#include "ConditionsBase/inc/TrackerCalibrationStructs.hh"
#include "ConditionsService/inc/TrackerCalibrations.hh"
#include "ConfigTools/inc/ConfigFileLookupPolicy.hh"
#include "GeometryService/inc/GeomHandle.hh"
#include "GeometryService/inc/getTrackerOrThrow.hh"
#include "TTrackerGeom/inc/TTracker.hh"
#include "TrackerConditions/inc/StrawElectronics.hh"
#include "TrackerConditions/inc/StrawPhysics.hh"
#include "TrackerConditions/inc/StrawResponse.hh"

#include "TrkHitReco/inc/PeakFit.hh"
#include "TrkHitReco/inc/PeakFitRoot.hh"
#include "TrkHitReco/inc/PeakFitFunction.hh"
#include "TrkHitReco/inc/ComboPeakFitRoot.hh"

#include "RecoDataProducts/inc/CaloClusterCollection.hh"
#include "RecoDataProducts/inc/StrawDigi.hh"
#include "RecoDataProducts/inc/ComboHit.hh"
#include "RecoDataProducts/inc/StrawHit.hh"

#include <memory>

 

namespace mu2e {
  using namespace TrkTypes;

  class StrawHitReco : public art::EDProducer 
  {
     public:
       explicit StrawHitReco(fhicl::ParameterSet const& pset);
       virtual ~StrawHitReco(); 
       virtual void produce( art::Event& e);
       virtual void beginRun( art::Run& run );
       virtual void beginJob();


     private:
       double _mbbuffer;                // buffer on that for ghost hits (wrapping)
       double _maxdt;                   // maximum time difference between end times
       bool   _singledigi;              // turn single-end digitizations into hits
       TrkHitReco::FitType _fittype; // peak Fitter
       bool   _usecc;                   // use calorimeter cluster filtering
       double _clusterDt;               // maximum hit-calo lcuster time difference
       double _minE;                    // minimum charge (units??)
       double _maxE;                    // maximum charge (units??)
       double _ctE;                     // minimum charge to flag neighbors as cross talk
       double _ctMinT;                  // time relative to proton hit to flag cross talk (ns)
       double _ctMaxT;                  // time relative to proton hit to flag cross talk (ns)
       double _minT;                    // minimum hit time
       double _maxT;                    // maximum hit time
       bool   _filter;                // trigger mode cut on dt and other thing 
       bool _flagXT; // flag cross-talk
       int    _printLevel;
       int    _diagLevel;
       StrawIdMask _mask; 
       StrawEnd _end[2]; // helper
 
       std::string strawDigis_;
       std::string caloClusterModuleLabel_;
       fhicl::ParameterSet peakfit_;       
       
       std::unique_ptr<TrkHitReco::PeakFit> pfit_;
       
 };

  StrawHitReco::StrawHitReco(fhicl::ParameterSet const& pset) :
      _mbbuffer(pset.get<double>(    "TimeBuffer",100.0)), 
      _maxdt(pset.get<double>(       "MaxTimeDifference",8.0)), 
      _singledigi(pset.get<bool>(    "UseSingleDigis",false)), 
      _fittype((TrkHitReco::FitType) pset.get<unsigned>("FitType",TrkHitReco::FitType::peakminusped)),
      _usecc(pset.get<bool>(         "UseCalorimeter",false)),     
      _clusterDt(pset.get<double>(   "clusterDt",100)),
      _minE(pset.get<double>(        "minimumEnergy",0.0)), // Minimum deposited straw energy (MeV)
      _maxE(pset.get<double>(        "maximumEnergy",0.01)), // MeV
      _ctE(pset.get<double>(         "crossTalkEnergy",0.007)), // MeV
      _ctMinT(pset.get<double>(      "crossTalkMinimumTime",-1)), // nsec
      _ctMaxT(pset.get<double>(      "crossTalkMaximumTime",100)), // nsec
      _minT(pset.get<double>(        "minimumTime",500)), // nsec
      _maxT(pset.get<double>(        "maximumTime",2000)), // nsec
      _filter(pset.get<bool>(      "FilterHits",true)),
      _flagXT(pset.get<bool>(      "FlagCrossTalk",false)),
      _printLevel(pset.get<int>(     "printLevel",0)),
      _diagLevel(pset.get<int>(      "diagLevel",0)),
      _end{TrkTypes::cal,TrkTypes::hv},
      strawDigis_(pset.get<std::string>("StrawDigis","makeSD")),
      caloClusterModuleLabel_(pset.get<std::string>("caloClusterModuleLabel","CaloClusterFast")),
      peakfit_(pset.get<fhicl::ParameterSet>("PeakFitter",fhicl::ParameterSet()))
  {
      produces<StrawHitCollection>();
      produces<ComboHitCollection>();
      // each hit is a unique straw
      std::vector<StrawIdMask::field> fields{StrawIdMask::plane,StrawIdMask::panel,StrawIdMask::straw};
      _mask = StrawIdMask(fields);

      if (_printLevel > 0) std::cout << "In StrawHitReco constructor " << std::endl;
  }

  StrawHitReco::~StrawHitReco() {}

  
  //------------------------------------------------------------------------------------------
  void StrawHitReco::beginJob()
  {
  }

  void StrawHitReco::beginRun(art::Run& run)
  {    
      ConditionsHandle<StrawElectronics> strawele = ConditionsHandle<StrawElectronics>("ignored");
                 
      // this must be done here because strawele is not accessible at startup and it 
      // contains a const refenence to pfit_, so this can't be instanciated earliere
      if (_fittype == TrkHitReco::FitType::peakminusped)
         pfit_ = std::unique_ptr<TrkHitReco::PeakFit>(new TrkHitReco::PeakFit(*strawele,peakfit_) );
      else if (_fittype == TrkHitReco::FitType::combopeakfit)
	 pfit_ = std::unique_ptr<TrkHitReco::PeakFit>(new TrkHitReco::ComboPeakFitRoot(*strawele,peakfit_) );
      else
	 pfit_ = std::unique_ptr<TrkHitReco::PeakFit>(new TrkHitReco::PeakFitRoot(*strawele,peakfit_) );
                      
      if (_printLevel > 0) std::cout << "In StrawHitReco begin Run " << std::endl;
  }



  //------------------------------------------------------------------------------------------
  void StrawHitReco::produce(art::Event& event)
  {        
      if (_printLevel > 0) std::cout << "In StrawHitReco produce " << std::endl;

      const Tracker& tracker = getTrackerOrThrow();
      const TTracker& tt(*GeomHandle<TTracker>());
      size_t nplanes = tt.nPlanes();
      size_t npanels = tt.getPlane(0).nPanels();
      
      ConditionsHandle<StrawElectronics> strawele = ConditionsHandle<StrawElectronics>("ignored");
      ConditionsHandle<StrawPhysics> strawphys = ConditionsHandle<StrawPhysics>("ignored");
      ConditionsHandle<StrawResponse> srep = ConditionsHandle<StrawResponse>("ignored");
      
      art::Handle<StrawDigiCollection> strawdigisHandle;
      event.getByLabel(strawDigis_,strawdigisHandle);
      const StrawDigiCollection& strawdigis(*strawdigisHandle);
      
      const CaloClusterCollection* caloClusters(0);
      art::Handle<CaloClusterCollection> caloClusterHandle;
      if (event.getByLabel(caloClusterModuleLabel_, caloClusterHandle)) caloClusters = caloClusterHandle.product();

      std::unique_ptr<StrawHitCollection> shCol(new StrawHitCollection);
      shCol->reserve(strawdigis.size());
      std::unique_ptr<ComboHitCollection> chCol(new ComboHitCollection);
      chCol->reserve(strawdigis.size());      

      std::vector<std::vector<size_t> > hits_by_panel(nplanes*npanels,std::vector<size_t>());    
      std::vector<size_t> largeHits, largeHitPanels;
      largeHits.reserve(strawdigis.size());
      largeHitPanels.reserve(strawdigis.size());
      
      for (size_t isd=0;isd<strawdigis.size();++isd)
      {
	const StrawDigi& digi = strawdigis[isd];          
	TDCTimes times;
	strawele->tdcTimes(digi.TDC(),times);
	TOTTimes tots{0.0,0.0};
	for(size_t iend=0;iend<2;++iend){
	  tots[iend] = digi.TOT(_end[iend])*strawele->totLSB();
	}
	// take the earliest of the 2 end times
	float time = std::min(times[0],times[1]);
	if (_filter && ( time < _minT || time > _maxT ) )continue;

	//calorimeter filtering
	if (_usecc && caloClusters)
	{
	  bool outsideCaloTime(true);
	  for (const auto& cluster : *caloClusters) 
	    if (std::abs(time-cluster.time())<_clusterDt) {outsideCaloTime=false; break;}
	  if (outsideCaloTime) continue;
	}


	//extract energy from waveform
	// note: pedestal is being subtracting inside strawele, in the real experiment we will need
	// per-channel version of this FIXME!!!
	TrkHitReco::PeakFitParams params;
	pfit_->process(digi.adcWaveform(),params);
	double energy = strawphys->ionizationEnergy(params._charge/strawphys->strawGain());
	if (_printLevel > 1) std::cout << "Fit status = " << params._status << " NDF = " << params._ndf << " chisquared " << params._chi2
	  << " Fit charge = " << params._charge << " Fit time = " << params._time << std::endl;

	if(_filter && ( energy < _minE  || energy > _maxE) ) continue;

	//create straw hit
	const Straw& straw  = tracker.getStraw( digi.strawIndex() );
	StrawHit hit(digi.strawIndex(),times,tots,energy);
	shCol->push_back(std::move(hit));
	// create combo hit.  this include 
	ComboHit ch;
	ch._nsh = 1; // 'combo' of 1 hit
	// get distance along wire from the straw center and it's estimated error
	float dw, dwerr;
	bool td = srep->wireDistance(hit,straw.getHalfLength(),dw,dwerr);
	ch._pos = straw.getMidPoint()+dw*straw.getDirection();
	ch._wdir = straw.getDirection();
	ch._wdist = dw;
	ch._wres = dwerr;
	ch._time = time;
	ch._edep = energy;
	ch._sid = straw.id();
	ch.addIndex(isd); // reference the digi; this allows MC truth matching to work
	ch._mask = _mask;
	// crude initial estimate of the transverse error
	static const double invsqrt12 = 1.0/sqrt(12.0);
	ch._tres = straw.getRadius()*invsqrt12;
	// set flags
	if (energy > _minE && energy < _maxE) ch._flag.merge(StrawHitFlag::energysel);
	if (time > _minT && time < _maxT)     ch._flag.merge(StrawHitFlag::timesel);
	if (_usecc)                           ch._flag.merge(StrawHitFlag::calosel);
	if (td) ch._flag.merge(StrawHitFlag::tdiv); 
	if(_flagXT){
	  //buffer large hit for cross-talk analysis
	  size_t iplane       = straw.id().getPlane();
	  size_t ipnl         = straw.id().getPanel();
	  size_t global_panel = ipnl + iplane*npanels;
	  hits_by_panel[global_panel].push_back(shCol->size());          
	  if (energy >= _ctE) {largeHits.push_back(shCol->size()); largeHitPanels.push_back(global_panel);}
	}
	chCol->push_back(std::move(ch));
      }

      //flag straw and electronic cross-talk
      if(_flagXT){
	for (size_t ilarge=0; ilarge < largeHits.size();++ilarge)
	{
	  const StrawHit& sh = shCol->at(largeHits[ilarge]);
	  const Straw& straw = tracker.getStraw( sh.strawIndex() );
	  for (size_t jsh : hits_by_panel[largeHitPanels[ilarge]])
	  {
	    if (jsh==largeHits[ilarge]) continue;              
	    const StrawHit& sh2 = shCol->at(jsh);              
	    if (sh2.time()-sh.time() > _ctMinT && sh2.time()-sh.time() < _ctMaxT)
	    {
	      if (straw.isSamePreamp(sh2.strawIndex()))       chCol->at(jsh)._flag.merge(StrawHitFlag::elecxtalk);
	      if (straw.isNearestNeighbour(sh2.strawIndex())) chCol->at(jsh)._flag.merge(StrawHitFlag::strawxtalk);
	    }           
	  }
	}
      }

      event.put(std::move(shCol));
      event.put(std::move(chCol));

  }

}


using mu2e::StrawHitReco;
DEFINE_ART_MODULE(StrawHitReco);

