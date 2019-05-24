//
// Prototype analysis module using tracks.  This module associates information from the
// Mu2e detector systems into a single coherent analysis TTree (trkana).  This module
// depends on the data products produced by reconstruction and (if requested) their MC
// counterparts.  The primary analysis object is the set of Downstream electron track fits.
// Upstream electron fit and downstream muon are also required for PID and quality selection.
// Calorimeter clusters and Track-cluster matching are used for PID. CRV coincidences are also
// included for rejecting cosmic backgrounds.
// Most of the calcluations are done by upstream modules and helper classes.
// Original author: Dave Brown (LBNL) 7/7/2016
// Updated November 2018 to run on KalSeeds only (A. Edmonds)
//

// Mu2e includes
#include "GeneralUtilities/inc/ParameterSetHelpers.hh"
#include "MCDataProducts/inc/ProtonBunchIntensity.hh"
#include "MCDataProducts/inc/EventWeight.hh"
#include "MCDataProducts/inc/KalSeedMC.hh"
#include "MCDataProducts/inc/CaloClusterMC.hh"
#include "RecoDataProducts/inc/CaloCrystalHit.hh"
#include "TrkReco/inc/TrkUtilities.hh"
#include "CalorimeterGeom/inc/DiskCalorimeter.hh"
#include "GeometryService/inc/VirtualDetector.hh"
// Framework includes.
#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Services/Optional/TFileService.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "canvas/Persistency/Common/TriggerResults.h"

// ROOT incldues
#include "Rtypes.h"
#include "TBits.h"
#include "TTree.h"
#include "TProfile.h"
#include "TH1F.h"

// BaBar includes
#include "BTrk/BaBar/BaBar.hh"
#include "BTrk/KalmanTrack/KalRep.hh"
#include "BTrk/ProbTools/ChisqConsistency.hh"
#include "BTrk/BbrGeom/BbrVectorErr.hh"
#include "BTrk/TrkBase/TrkHelixUtils.hh"
#include "Mu2eUtilities/inc/TriggerResultsNavigator.hh"
#include "Mu2eUtilities/inc/SimParticleTimeOffset.hh"
// mu2e tracking
#include "RecoDataProducts/inc/TrkFitDirection.hh"
#include "BTrkData/inc/TrkStrawHit.hh"
// diagnostics
#include "TrkDiag/inc/TrkComp.hh"
#include "TrkDiag/inc/HitCount.hh"
#include "TrkDiag/inc/TrkCount.hh"
#include "TrkDiag/inc/EventInfo.hh"
#include "TrkDiag/inc/TrkInfo.hh"
#include "TrkDiag/inc/GenInfo.hh"
#include "TrkDiag/inc/EventWeightInfo.hh"
#include "TrkDiag/inc/TrkStrawHitInfo.hh"
#include "TrkDiag/inc/TrkStrawHitInfoMC.hh"
#include "TrkDiag/inc/TrkCaloHitInfo.hh"
#include "TrkDiag/inc/CaloClusterInfoMC.hh"
#include "TrkDiag/inc/TrkQualInfo.hh"
#include "TrkDiag/inc/HelixInfo.hh"
#include "TrkDiag/inc/InfoStructHelper.hh"
#include "TrkDiag/inc/InfoMCStructHelper.hh"
// CRV info
#include "CRVAnalysis/inc/CRVAnalysis.hh"

// C++ includes.
#include <iostream>
#include <string>
#include <cmath>

using namespace std;

namespace mu2e {
// Need this for the BaBar headers.
  using CLHEP::Hep3Vector;
  typedef KalSeedCollection::const_iterator KSCIter;

  class TrackAnalysisReco : public art::EDAnalyzer {

  public:

    struct BranchConfig {
      using Name=fhicl::Name;
      using Comment=fhicl::Comment;

      fhicl::Atom<art::InputTag> input{Name("input"), Comment("KalSeedCollection input tag")};
      fhicl::Atom<std::string> branch{Name("branch"), Comment("Name of output branch")};
      fhicl::Atom<bool> fillmc{Name("fillMC"), Comment("Switch to turn on filling of MC information for this set of tracks"), false};
      fhicl::OptionalAtom<std::string> trkqual{Name("trkqual"), Comment("TrkQualCollection input tag")};
    };

    struct Config {
      using Name=fhicl::Name;
      using Comment=fhicl::Comment;

      fhicl::Table<BranchConfig> candidate{Name("candidate"), Comment("Candidate physics track info")};
      fhicl::OptionalSequence< fhicl::Table<BranchConfig> > supplements{Name("supplements"), Comment("Supplemental physics track info (TrkAna will find closest in time to candidate)")};
      fhicl::Atom<art::InputTag> rctag{Name("RecoCountTag"), Comment("RecoCount"), art::InputTag()};
      fhicl::Atom<art::InputTag> cchmtag{Name("CaloCrystalHitMapTag"), Comment("CaloCrystalHitMapTag"), art::InputTag()};
      fhicl::Atom<art::InputTag> meanPBItag{Name("MeanBeamIntensity"), Comment("Tag for MeanBeamIntensity"), art::InputTag()};
      fhicl::Atom<art::InputTag> PBIwtTag{Name("PBIWeightTag"), Comment("Tag for PBIWeight") ,art::InputTag()};
      fhicl::Atom<std::string> crvCoincidenceModuleLabel{Name("CrvCoincidenceModuleLabel"), Comment("CrvCoincidenceModuleLabel")};
      fhicl::Atom<std::string> crvCoincidenceMCModuleLabel{Name("CrvCoincidenceMCModuleLabel"), Comment("CrvCoincidenceMCModuleLabel")};
      fhicl::Atom<bool> fillmc{Name("FillMCInfo"),true};
      fhicl::Atom<bool> pempty{Name("ProcessEmptyEvents"),false};
      fhicl::Atom<bool> crv{Name("AnalyzeCRV"),false};
      fhicl::Atom<bool> helices{Name("FillHelixInfo"),false};
      fhicl::Atom<bool> filltrkqual{Name("FillTrkQualInfo"),true};
      fhicl::Atom<bool> filltrig{Name("FillTriggerInfo"),false};
      fhicl::Atom<int> diag{Name("diagLevel"),1};
      fhicl::Atom<int> debug{Name("debugLevel"),0};
      fhicl::Atom<art::InputTag> primaryParticleTag{Name("PrimaryParticleTag"), Comment("Tag for PrimaryParticle"), art::InputTag()};
      fhicl::Atom<art::InputTag> kalSeedMCTag{Name("KalSeedMCAssns"), Comment("Tag for KalSeedMCAssn"), art::InputTag()};
      fhicl::Atom<art::InputTag> caloClusterMCTag{Name("CaloClusterMCAssns"), Comment("Tag for CaloClusterMCAssns"), art::InputTag()};
      fhicl::Table<InfoMCStructHelper::Config> infoMCStructHelper{Name("InfoMCStructHelper"), Comment("Configuration for the InfoMCStructHelper")};
    };
    typedef art::EDAnalyzer::Table<Config> Parameters;

    explicit TrackAnalysisReco(const Parameters& conf);
    virtual ~TrackAnalysisReco() { }

    void beginJob();
    void beginSubRun(const art::SubRun & subrun ) override;
    void analyze(const art::Event& e);

  private:

    Config _conf;

    // track comparator
    TrkComp _tcomp;
    // main TTree
    TTree* _trkana;
    TProfile* _tht; // profile plot of track hit times: just an example
    TH1F* _trigbits; // plot of trigger bits: just an example
    // general event info branch
    double _meanPBI;
    EventInfo _einfo;
    EventWeightInfo _wtinfo;
    // hit counting
    HitCount _hcnt;
    // track counting
    TrkCount _tcnt;

    // track branches
    TrkInfo _candidateTI;
    TrkFitInfo _candidateEntTI, _candidateMidTI, _candidateXitTI;
    TrkCaloHitInfo _candidateTCHI;
    TrkQualCollection _candidateTQC;

    std::vector<TrkInfo> _supplementTIs;
    std::vector<TrkFitInfo> _supplementEntTIs, _supplementMidTIs, _supplementXitTIs;
    std::vector<TrkCaloHitInfo> _supplementTCHIs;
    std::vector<TrkQualCollection> _supplementTQCs;

    // detailed info branches for the signal candidate
    std::vector<TrkStrawHitInfo> _detsh;
    std::vector<TrkStrawMatInfo> _detsm;
    // trigger information
    unsigned _trigword;

    // MC truth branches
    TrkInfoMC _candidateMCTI;
    std::vector<TrkInfoMC> _supplementMCTIs;
    art::InputTag _primaryParticleTag;
    art::InputTag _kalSeedMCTag, _caloClusterMCTag;
    std::vector<int> _entvids, _midvids, _xitvids;

    // detailed MC truth for the signal candidate
    GenInfo _candidateMCGenTI, _candidateMCPriTI; // generator and 'primary' information
    std::vector<GenInfo> _supplementMCGenTIs, _supplementMCPriTIs;
    TrkInfoMCStep _candidateMCEntTI, _candidateMCMidTI, _candidateMCXitTI;
    CaloClusterInfoMC _candidateMCTCHI;

    std::vector<TrkInfoMCStep> _supplementMCEntTIs, _supplementMCMidTIs, _supplementMCXitTIs;
    std::vector<CaloClusterInfoMC> _supplementMCTCHIs;
    std::vector<TrkStrawHitInfoMC> _detshmc;
    // test trkqual variable branches
    TrkQualInfo _trkQualInfo;
    // helper functions
    void fillEventInfo(const art::Event& event);
    void fillTriggerBits(const art::Event& event,std::string const& process);
    void resetBranches();
    size_t findSupplementTrack(KalSeedCollection const& kcol,KalSeed const& candidate, bool sameColl);
    // CRV info
    std::vector<CrvHitInfoReco> _crvinfo;
    int _bestcrv;
    HelixInfo _hinfo;
    std::vector<CrvHitInfoMC> _crvinfomc;
    // SimParticle timing offset
    InfoStructHelper _infoStructHelper;
    InfoMCStructHelper _infoMCStructHelper;
};

  TrackAnalysisReco::TrackAnalysisReco(const Parameters& conf):
    art::EDAnalyzer(conf),
    _conf(conf()),
    _infoMCStructHelper(conf().infoMCStructHelper())
  {
    _midvids.push_back(VirtualDetectorId::TT_Mid);
    _midvids.push_back(VirtualDetectorId::TT_MidInner);
    _entvids.push_back(VirtualDetectorId::TT_FrontHollow);
    _entvids.push_back(VirtualDetectorId::TT_FrontPA);
    _xitvids.push_back(VirtualDetectorId::TT_Back);

    std::vector<BranchConfig> supps;
    if (_conf.supplements(supps)) {
      for (size_t i_supplement = 0; i_supplement < supps.size(); ++i_supplement) {
	TrkInfo ti;
	_supplementTIs.push_back(ti);
	TrkFitInfo ent, mid, xit;
	_supplementEntTIs.push_back(ent);
	_supplementMidTIs.push_back(mid);
	_supplementXitTIs.push_back(xit);

	TrkCaloHitInfo tchi;
	_supplementTCHIs.push_back(tchi);

	TrkInfoMC mcti;
	_supplementMCTIs.push_back(mcti);
	GenInfo mcgen, mcpri;
	_supplementMCGenTIs.push_back(mcgen);
	_supplementMCPriTIs.push_back(mcpri);
	TrkInfoMCStep mcent, mcmid, mcxit;
	_supplementMCEntTIs.push_back(mcent);
	_supplementMCMidTIs.push_back(mcmid);
	_supplementMCXitTIs.push_back(mcxit);

	CaloClusterInfoMC mctchi;
	_supplementMCTCHIs.push_back(mctchi);
      }
    }
  }

  void TrackAnalysisReco::beginJob( ){
    art::ServiceHandle<art::TFileService> tfs;
// create TTree
    _trkana=tfs->make<TTree>("trkana","track analysis");
    _tht=tfs->make<TProfile>("tht","Track Hit Time Profile",RecoCount::_nshtbins,-25.0,1725.0);
// add event info branch
    _trkana->Branch("evtinfo.",&_einfo,EventInfo::leafnames().c_str());
// hit counting branch
    _trkana->Branch("hcnt.",&_hcnt,HitCount::leafnames().c_str());
// track counting branch
    std::vector<std::string> trkbranches;
    trkbranches.push_back(_conf.candidate().branch());
    std::vector<BranchConfig> supps;
    if (_conf.supplements(supps)) {
      for (size_t i_supplement = 0; i_supplement < supps.size(); ++i_supplement) {
	trkbranches.push_back(supps.at(i_supplement).branch());
      }
    }
    _trkana->Branch("tcnt",&_tcnt,_tcnt.leafnames(trkbranches).c_str());
    // add candidate track branches
    std::string branch = _conf.candidate().branch();
    _trkana->Branch(branch.c_str(),&_candidateTI,TrkInfo::leafnames().c_str());
    _trkana->Branch((branch+"ent").c_str(),&_candidateEntTI,TrkFitInfo::leafnames().c_str());
    _trkana->Branch((branch+"mid").c_str(),&_candidateMidTI,TrkFitInfo::leafnames().c_str());
    _trkana->Branch((branch+"xit").c_str(),&_candidateXitTI,TrkFitInfo::leafnames().c_str());
    _trkana->Branch((branch+"tch").c_str(),&_candidateTCHI,TrkCaloHitInfo::leafnames().c_str());
    // optionally add detailed branches
    if(_conf.diag() > 1){
      _trkana->Branch((branch+"tsh").c_str(),&_detsh);
      _trkana->Branch((branch+"tsm").c_str(),&_detsm);
    }
// trigger info.  Actual names should come from the BeginRun object FIXME
    if(_conf.filltrig())_trkana->Branch("trigbits",&_trigbits,"trigbits/i");
// calorimeter information for the downstream electron track
// CRV info
    if(_conf.crv()) _trkana->Branch("crvinfo",&_crvinfo);
   // helix info
   if(_conf.helices()) _trkana->Branch("helixinfo",&_hinfo,HelixInfo::leafnames().c_str());
// optionally add MC truth branches
   if(_conf.fillmc() && _conf.candidate().fillmc()){
      _trkana->Branch((branch+"mc").c_str(),&_candidateMCTI,TrkInfoMC::leafnames().c_str());
      _trkana->Branch((branch+"mcgen").c_str(),&_candidateMCGenTI,GenInfo::leafnames().c_str());
      _trkana->Branch((branch+"mcpri").c_str(),&_candidateMCPriTI,GenInfo::leafnames().c_str());
      _trkana->Branch((branch+"mcent").c_str(),&_candidateMCEntTI,TrkInfoMCStep::leafnames().c_str());
      _trkana->Branch((branch+"mcmid").c_str(),&_candidateMCMidTI,TrkInfoMCStep::leafnames().c_str());
      _trkana->Branch((branch+"mcxit").c_str(),&_candidateMCXitTI,TrkInfoMCStep::leafnames().c_str());
      _trkana->Branch((branch+"tchmc").c_str(),&_candidateMCTCHI,CaloClusterInfoMC::leafnames().c_str());
      if(_conf.crv())_trkana->Branch("crvinfomc",&_crvinfomc);
      if(_conf.diag() > 1)_trkana->Branch((branch+"tshmc").c_str(),&_detshmc);
    }
    std::string tq;
    if (_conf.candidate().trkqual(tq)) {
      if (_conf.filltrkqual()) {
	_trkana->Branch((branch+"trkqual").c_str(), &_trkQualInfo, TrkQualInfo::leafnames().c_str());
      }
    }

    // add branches for supplement tracks
    if (_conf.supplements(supps)) {
      for (size_t i_supplement = 0; i_supplement < supps.size(); ++i_supplement) {
	const auto& supplementConfig = supps.at(i_supplement);
	std::string branch = supplementConfig.branch();
	_trkana->Branch(branch.c_str(),&_supplementTIs.at(i_supplement),TrkInfo::leafnames().c_str());
	_trkana->Branch((branch+"ent").c_str(),&_supplementEntTIs.at(i_supplement),TrkFitInfo::leafnames().c_str());
	_trkana->Branch((branch+"mid").c_str(),&_supplementMidTIs.at(i_supplement),TrkFitInfo::leafnames().c_str());
	_trkana->Branch((branch+"xit").c_str(),&_supplementXitTIs.at(i_supplement),TrkFitInfo::leafnames().c_str());
	_trkana->Branch((branch+"tch").c_str(),&_supplementTCHIs.at(i_supplement),TrkCaloHitInfo::leafnames().c_str());
	if(_conf.fillmc() && supplementConfig.fillmc()){
	  _trkana->Branch((branch+"mc").c_str(),&_supplementMCTIs.at(i_supplement),TrkInfoMC::leafnames().c_str());
	  _trkana->Branch((branch+"mcgen").c_str(),&_supplementMCGenTIs.at(i_supplement),GenInfo::leafnames().c_str());
	  _trkana->Branch((branch+"mcpri").c_str(),&_supplementMCPriTIs.at(i_supplement),GenInfo::leafnames().c_str());
	  _trkana->Branch((branch+"mcent").c_str(),&_supplementMCEntTIs.at(i_supplement),TrkInfoMCStep::leafnames().c_str());
	  _trkana->Branch((branch+"mcmid").c_str(),&_supplementMCMidTIs.at(i_supplement),TrkInfoMCStep::leafnames().c_str());
	  _trkana->Branch((branch+"mcxit").c_str(),&_supplementMCXitTIs.at(i_supplement),TrkInfoMCStep::leafnames().c_str());
	  _trkana->Branch((branch+"tchmc").c_str(),&_supplementMCTCHIs.at(i_supplement),CaloClusterInfoMC::leafnames().c_str());
	}
      }
    }
  }

  void TrackAnalysisReco::beginSubRun(const art::SubRun & subrun ) {
    // mean number of protons on target
    art::Handle<ProtonBunchIntensity> PBIHandle;
    subrun.getByLabel(_conf.meanPBItag(), PBIHandle);
    if(PBIHandle.isValid())
      _meanPBI = PBIHandle->intensity();
    // get bfield
    _infoStructHelper.updateSubRun();
  }

  void TrackAnalysisReco::analyze(const art::Event& event) {
    // update timing maps for MC
    if(_conf.fillmc()){
      _infoMCStructHelper.updateEvent(event);
    }

    // get VD positions
    mu2e::GeomHandle<VirtualDetector> vdHandle;
    mu2e::GeomHandle<DetectorSystem> det;
    const XYZVec& entpos = XYZVec(det->toDetector(vdHandle->getGlobal(*_entvids.begin())));
    const XYZVec& midpos = XYZVec(det->toDetector(vdHandle->getGlobal(*_midvids.begin())));
    const XYZVec& xitpos = XYZVec(det->toDetector(vdHandle->getGlobal(*_xitvids.begin())));

    // need to create and define the event weight branch here because we only now know the EventWeight creating modules that have been run through the Event
    if (!_trkana->GetBranch("evtwt")) { 
      std::vector<art::Handle<EventWeight> > eventWeightHandles;
      event.getManyByType(eventWeightHandles);
      if (eventWeightHandles.size()>0) {
	std::vector<std::string> labels;
	for (const auto& i_weightHandle : eventWeightHandles) {
	  std::string moduleLabel = i_weightHandle.provenance()->moduleLabel();
	  std::string instanceName = i_weightHandle.provenance()->productInstanceName();

	  std::string branchname = moduleLabel;
	  if (instanceName != "") {
	    branchname += "_" + instanceName;
	  }
	  labels.push_back(branchname);
	}
	_trkana->Branch("evtwt",&_wtinfo,_wtinfo.leafnames(labels).c_str());
      }
    }

    // Get handle to downstream electron track collection.  This also creates the final set of hit flags
    art::Handle<KalSeedCollection> candidateKSCH;
    event.getByLabel(_conf.candidate().input(),candidateKSCH);
    // get the provenance from this for trigger processing
    std::string const& process = candidateKSCH.provenance()->processName();
    auto const& candidateKSC = *candidateKSCH;

    art::Handle<TrkQualCollection> candidateTQCH;
    std::string tqtag;
    if (_conf.candidate().trkqual(tqtag)) {
      event.getByLabel(tqtag, candidateTQCH);
      _candidateTQC = *candidateTQCH;
    }
    if (_candidateTQC.size()>0 && _candidateTQC.size() != candidateKSC.size()) {
      throw cet::exception("TrackAnalysis") << "TrkQualCollection and candidate KalSeedCollection are of different sizes (" << _candidateTQC.size() << " and " << candidateKSC.size() << " respectively)" << std::endl;
    }

    // get the supplement track collections
    std::vector<art::Handle<KalSeedCollection> > supplementKSCHs;
    std::vector<BranchConfig> supps;
    if (_conf.supplements(supps)) {
      _supplementTQCs.clear();

      for (size_t i_supplement = 0; i_supplement < supps.size(); ++i_supplement) {
	art::Handle<KalSeedCollection> i_handle;
	art::InputTag i_tag = supps.at(i_supplement).input();
	event.getByLabel(i_tag, i_handle);
	supplementKSCHs.push_back(i_handle);

	art::Handle<TrkQualCollection> i_tq_handle;
	std::string i_tq_tag;
	supps.at(i_supplement).trkqual(i_tq_tag);
	event.getByLabel(i_tq_tag, i_tq_handle);
	_supplementTQCs.push_back(*i_tq_handle);
      }
    }

    art::Handle<CaloCrystalHitRemapping> cchmH;
    event.getByLabel(_conf.cchmtag(),cchmH);
    auto const& cchmap = *cchmH;

    // general reco counts
    auto rch = event.getValidHandle<RecoCount>(_conf.rctag());
    auto const& rc = *rch;
    for(size_t ibin=0;ibin < rc._nshtbins; ++ibin){
      float time = rc._shthist.binMid(ibin);
      float count  = rc._shthist.binContents(ibin);
      _tht->Fill(time,count);
    }
    // trigger information
    if(_conf.filltrig()){
      fillTriggerBits(event,process);
    }
    // MC data
    art::Handle<PrimaryParticle> pph;
    art::Handle<KalSeedMCAssns> ksmcah;
    art::Handle<CaloClusterMCAssns> ccmcah;
    if(_conf.fillmc()) { // get MC product collections
      event.getByLabel(_conf.primaryParticleTag(),pph);
      event.getByLabel(_conf.kalSeedMCTag(),ksmcah);
      event.getByLabel(_conf.caloClusterMCTag(),ccmcah);
    }
    // reset
    resetBranches();

    // fill track counts
    _tcnt._counts[0] = candidateKSC.size();
    if (_conf.supplements(supps)) {
      for (size_t i_supplement = 0; i_supplement < supps.size(); ++i_supplement) {
	_tcnt._counts[i_supplement+1] = (supplementKSCHs.at(i_supplement))->size();
      }
    }

    // fill event level info
    fillEventInfo(event);
    _infoStructHelper.fillHitCount(rc, _hcnt);

    // loop through all tracks
    for (size_t i_kseed = 0; i_kseed < candidateKSC.size(); ++i_kseed) {
      auto const& candidateKS = candidateKSC.at(i_kseed);

      _infoStructHelper.fillTrkInfo(candidateKS,_candidateTI);
      _infoStructHelper.fillTrkFitInfo(candidateKS,_candidateEntTI,entpos);
      _infoStructHelper.fillTrkFitInfo(candidateKS,_candidateMidTI,midpos);
      _infoStructHelper.fillTrkFitInfo(candidateKS,_candidateXitTI,xitpos);
      //      _tcnt._overlaps[0] = _tcomp.nOverlap(candidateKS, candidateKS);

      if(_conf.diag() > 1){ // want hit level info
	_infoStructHelper.fillHitInfo(candidateKS, _detsh);
	_infoStructHelper.fillMatInfo(candidateKS, _detsm);
      }

      if(_conf.helices()){
	_infoStructHelper.fillHelixInfo(candidateKS, _hinfo);
      }

      // calorimeter info
      if (candidateKS.hasCaloCluster()) {
	_infoStructHelper.fillCaloHitInfo(candidateKS,  _candidateTCHI);
	_tcnt._ndec = 1; // only 1 possible calo hit at the moment
	// test
	if(_conf.debug()>0){
	  auto const& tch = candidateKS.caloHit();
	  auto const& cc = tch.caloCluster();
	  std::cout << "CaloCluster has energy " << cc->energyDep()
		    << " +- " << cc->energyDepErr() << std::endl;
	  for( auto const& cchptr: cc->caloCrystalHitsPtrVector() ) { 
	    // map the crystal ptr to the reduced collection
	    auto ifnd = cchmap.find(cchptr);
	    if(ifnd != cchmap.end()){
	      auto const& scchptr = ifnd->second;
	      if(scchptr.isNonnull())
		std::cout << "CaloCrystalHit has " << scchptr->energyDep() << " energy Dep" << std::endl;
	      else
		std::cout <<"CalCrystalHitPtr is invalid! "<< std::endl;
	    } else {
	      std::cout << "CaloCrystaLhitPtr not in map!" << std::endl;
	    }
	  }
	}
      }
      if (_candidateTQC.size()>0) {
	auto const& tqual = _candidateTQC.at(i_kseed);
	_candidateTI._trkqual = tqual.MVAOutput();
	if (_conf.filltrkqual()) {
	  _infoStructHelper.fillTrkQualInfo(tqual, _trkQualInfo);
	}
      }
      // fill mC info associated with this track
      if(_conf.fillmc() && _conf.candidate().fillmc()) { 
	const PrimaryParticle& primary = *pph;
	// use Assns interface to find the associated KalSeedMC; this uses ptrs
	auto dekptr = art::Ptr<KalSeed>(candidateKSCH,i_kseed);
	//	std::cout << "KalSeedMCMatch has " << ksmcah->size() << " entries" << std::endl;
	for(auto iksmca = ksmcah->begin(); iksmca!= ksmcah->end(); iksmca++){
	  //	  std::cout << "KalSeed Ptr " << dekptr << " match Ptr " << iksmca->first << std::endl;
	  if(iksmca->first == dekptr) {
	    auto const& dekseedmc = *(iksmca->second);
	    _infoMCStructHelper.fillTrkInfoMC(dekseedmc, _candidateMCTI);
	    _infoMCStructHelper.fillTrkInfoMCStep(dekseedmc, _candidateMCEntTI, _entvids);
	    _infoMCStructHelper.fillTrkInfoMCStep(dekseedmc, _candidateMCMidTI, _midvids);
	    _infoMCStructHelper.fillTrkInfoMCStep(dekseedmc, _candidateMCXitTI, _xitvids);
	    _infoMCStructHelper.fillGenAndPriInfo(dekseedmc, primary, _candidateMCPriTI, _candidateMCGenTI);
	    
	    if (_conf.diag()>1) {
	      _infoMCStructHelper.fillHitInfoMCs(dekseedmc, _detshmc);
	    }
	    break;
	  }
	}
	if (candidateKS.hasCaloCluster()) {
	  // fill MC truth of the associated CaloCluster 
	  for(auto iccmca= ccmcah->begin(); iccmca != ccmcah->end(); iccmca++){
	    if(iccmca->first == candidateKS.caloCluster()){
	      auto const& ccmc = *(iccmca->second);
	      _infoMCStructHelper.fillCaloClusterInfoMC(ccmc,_candidateMCTCHI);
	      
	      break;
	    }
	  }
	}
      }

      // go through the supplement collections and find the track nearest to the candidate
      std::vector<BranchConfig> supps;
      if (_conf.supplements(supps)) {
	for (size_t i_supplement = 0; i_supplement < supps.size(); ++i_supplement) {
	  const auto& i_supplementKSCH = supplementKSCHs.at(i_supplement);
	  const auto& i_supplementKSC = *i_supplementKSCH;
	  auto const& i_supplementTQC = _supplementTQCs.at(i_supplement);

	  bool sameColl = false;
	  if (_conf.candidate().input() == supps.at(i_supplement).input()) {
	    sameColl = true;
	  }
	  auto i_supplementKS = findSupplementTrack(i_supplementKSC,candidateKS, sameColl);
	  
	  if(i_supplementKS < i_supplementKSC.size()) { 
	    const auto& supplementKS = i_supplementKSC.at(i_supplementKS);
	    auto& i_supplementTI = _supplementTIs.at(i_supplement);
	    _infoStructHelper.fillTrkInfo(supplementKS,i_supplementTI);

	    auto& i_supplementEntTI = _supplementEntTIs.at(i_supplement);
	    auto& i_supplementMidTI = _supplementMidTIs.at(i_supplement);
	    auto& i_supplementXitTI = _supplementXitTIs.at(i_supplement);
	    _infoStructHelper.fillTrkFitInfo(supplementKS,i_supplementEntTI,entpos);
	    _infoStructHelper.fillTrkFitInfo(supplementKS,i_supplementMidTI,midpos);
	    _infoStructHelper.fillTrkFitInfo(supplementKS,i_supplementXitTI,xitpos);

	    if (supplementKS.hasCaloCluster()) {
	      auto& i_supplementTCHI = _supplementTCHIs.at(i_supplement);
	      _infoStructHelper.fillCaloHitInfo(supplementKS,  i_supplementTCHI);
	    }

	    //	    _tcnt._overlaps[i_supplement+1] = _tcomp.nOverlap(candidateKS, supplementKS);

	    if (i_supplementTQC.size()>0 && i_supplementTQC.size() != i_supplementKSC.size()) {
	      throw cet::exception("TrackAnalysis") << "TrkQualCollection and supplemental KalSeedCollection are of different sizes (" << i_supplementTQC.size() << " and " << i_supplementKSC.size() << " respectively)" << std::endl;
	    }
	    if (i_supplementTQC.size()>0) {
	      const auto& tqual = i_supplementTQC.at(i_supplementKS);
	      i_supplementTI._trkqual = tqual.MVAOutput();
	    }

	    if (_conf.fillmc() && supps.at(i_supplement).fillmc()) {
	      const PrimaryParticle& primary = *pph;
	      // use Assns interface to find the associated KalSeedMC; this uses ptrs
	      auto kptr = art::Ptr<KalSeed>(i_supplementKSCH,i_supplementKS);
	      for(auto iksmca = ksmcah->begin(); iksmca!= ksmcah->end(); iksmca++){
		//	  std::cout << "KalSeed Ptr " << dekptr << " match Ptr " << iksmca->first << std::endl;
		if(iksmca->first == kptr) {
		  auto const& kseedmc = *(iksmca->second);
		  auto& i_supplementMCTI = _supplementMCTIs.at(i_supplement);
		  _infoMCStructHelper.fillTrkInfoMC(kseedmc, i_supplementMCTI);

		  auto& i_supplementMCEntTI = _supplementMCEntTIs.at(i_supplement);
		  auto& i_supplementMCMidTI = _supplementMCMidTIs.at(i_supplement);
		  auto& i_supplementMCXitTI = _supplementMCXitTIs.at(i_supplement);
		  _infoMCStructHelper.fillTrkInfoMCStep(kseedmc, i_supplementMCEntTI, _entvids);
		  _infoMCStructHelper.fillTrkInfoMCStep(kseedmc, i_supplementMCMidTI, _midvids);
		  _infoMCStructHelper.fillTrkInfoMCStep(kseedmc, i_supplementMCXitTI, _xitvids);

		  auto& i_supplementMCPriTI = _supplementMCPriTIs.at(i_supplement);
		  auto& i_supplementMCGenTI = _supplementMCGenTIs.at(i_supplement);
		  _infoMCStructHelper.fillGenAndPriInfo(kseedmc, primary, i_supplementMCPriTI, i_supplementMCGenTI);
	    
		  //		  if (_conf.diag()>1) {
		  //		    _infoMCStructHelper.fillHitInfoMCs(dekseedmc, _detshmc);
		  //		  }
		  break;
		}
	      }
	      if (supplementKS.hasCaloCluster()) {
		auto& i_supplementMCTCHI = _supplementMCTCHIs.at(i_supplement);
		// fill MC truth of the associated CaloCluster 
		for(auto iccmca= ccmcah->begin(); iccmca != ccmcah->end(); iccmca++){
		  if(iccmca->first == supplementKS.caloCluster()){
		    auto const& ccmc = *(iccmca->second);
		    _infoMCStructHelper.fillCaloClusterInfoMC(ccmc,i_supplementMCTCHI);
		    break;
		  }
		}
	      }
	    }
	  }
	}
      }

      // TODO we want MC information when we don't have a track
      // fill CRV info
      if(_conf.crv()) CRVAnalysis::FillCrvHitInfoCollections(_conf.crvCoincidenceModuleLabel(), _conf.crvCoincidenceMCModuleLabel(), event, _crvinfo, _crvinfomc);
      // fill this row in the TTree
      _trkana->Fill();
    }

    if(_conf.pempty()) { // if we want to process empty events
      _trkana->Fill();
    }
  }

  size_t TrackAnalysisReco::findSupplementTrack(KalSeedCollection const& kcol,const KalSeed& candidate, bool sameColl) {
    size_t retval = kcol.size();

    // loop over supplement tracks and find the closest
    double candidate_time = candidate.t0().t0();
    double closest_time = 999999999;
    for(auto i_kseed=kcol.begin(); i_kseed != kcol.end(); i_kseed++) {
      double supplement_time = i_kseed->t0().t0();
      if( fabs(supplement_time - candidate_time) < fabs(closest_time-candidate_time)) {
	if (sameColl && fabs(supplement_time - candidate_time)<1e-5) {
	  continue; // don't want the exact same track
	}
	closest_time = supplement_time;
	retval = i_kseed - kcol.begin();
      }
    }
    return retval;
  }

  void TrackAnalysisReco::fillEventInfo( const art::Event& event) {
    // fill basic event information
    _einfo._eventid = event.event();
    _einfo._runid = event.run();
    _einfo._subrunid = event.subRun();

    // get event weight products
    std::vector<art::Handle<EventWeight> > eventWeightHandles;
    event.getManyByType(eventWeightHandles);
    std::vector<Float_t> weights;
    for (const auto& i_weightHandle : eventWeightHandles) {
      double weight = i_weightHandle->weight();
      if (i_weightHandle.provenance()->moduleLabel() == _conf.PBIwtTag().label()) {
	if (_meanPBI > 0.0){
	  _einfo._nprotons = _meanPBI*weight;
	}
	else {
	  _einfo._nprotons = 1; // for non-background mixed jobs
	}
      }
      weights.push_back(weight);
    }
    _wtinfo.setWeights(weights);
  }

  void TrackAnalysisReco::fillTriggerBits(const art::Event& event,std::string const& process) {
    //get the TriggerResult from the process that created the KalFinalFit downstream collection
    static const std::string tname("_trigger"); // all trigger paths have this in the name
    static bool first(true);
    static std::array<bool,16> istrig = {false};
    art::InputTag const tag{Form("TriggerResults::%s", process.c_str())};
    auto trigResultsH = event.getValidHandle<art::TriggerResults>(tag);
    const art::TriggerResults* trigResults = trigResultsH.product();
    TriggerResultsNavigator tnav(trigResults);
   _trigword = 0;
   // setup the bin labels
    if(first){ // is there a better way to do this?  I think not
      for(size_t id=0;id < trigResults->size(); ++id){
	if (tnav.getTrigPath(id).find(tname) != std::string::npos) {
	  _trigbits->GetXaxis()->SetBinLabel(id+1,tnav.getTrigPath(id).c_str());
	  istrig[id] =true;
	}
      }
      first = false;
    }
    if(_conf.debug() > 0){
      cout << "Found TriggerResults for process " << process << " with " << trigResults->size() << " Lines"
      << " trigger bits word " << _trigbits << endl;
      TriggerResultsNavigator tnav(trigResults);
      tnav.print();
    }
  }

  void TrackAnalysisReco::resetBranches() {
    // reset structs
    _einfo.reset();
    _hcnt.reset();
    _tcnt.reset();
    _candidateTI.reset();
    _candidateEntTI.reset();
    _candidateMidTI.reset();
    _candidateXitTI.reset();
    _candidateTCHI.reset();
    _hinfo.reset();
    _candidateMCTI.reset();
    _candidateMCGenTI.reset();
    _candidateMCPriTI.reset();
    _candidateMCEntTI.reset();
    _candidateMCMidTI.reset();
    _candidateMCXitTI.reset();
    _candidateMCTCHI.reset();
    _wtinfo.reset();
    _trkQualInfo.reset();
    std::vector<BranchConfig> supps;
    if (_conf.supplements(supps)) {
      for (size_t i_supplement = 0; i_supplement < supps.size(); ++i_supplement) {
	_supplementTIs.at(i_supplement).reset();
	_supplementEntTIs.at(i_supplement).reset();
	_supplementMidTIs.at(i_supplement).reset();
	_supplementXitTIs.at(i_supplement).reset();

	_supplementTCHIs.at(i_supplement).reset();

	_supplementMCTIs.at(i_supplement).reset();
	_supplementMCGenTIs.at(i_supplement).reset();
	_supplementMCPriTIs.at(i_supplement).reset();

	_supplementMCEntTIs.at(i_supplement).reset();
	_supplementMCMidTIs.at(i_supplement).reset();
	_supplementMCXitTIs.at(i_supplement).reset();
	_supplementMCTCHIs.at(i_supplement).reset();
      }
    }
// clear vectors
    _detsh.clear();
    _detsm.clear();
    _detshmc.clear();
    _crvinfo.clear();
    _crvinfomc.clear();
  }

}  // end namespace mu2e

// Part of the magic that makes this class a module.
// create an instance of the module.  It also registers
using mu2e::TrackAnalysisReco;
DEFINE_ART_MODULE(TrackAnalysisReco);
