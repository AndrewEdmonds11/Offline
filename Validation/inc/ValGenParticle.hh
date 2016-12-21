
#ifndef ValGenParticle_HH_
#define ValGenParticle_HH_

#include "art/Framework/Principal/Event.h"
#include "MCDataProducts/inc/GenParticleCollection.hh"
#include "Validation/inc/ValId.hh"
#include "Validation/inc/ValPosition.hh"
#include "TH1D.h"
#include <string>

namespace mu2e {

  class ValGenParticle {

  public:
    ValGenParticle(std::string name):_name(name){}
    int declare( art::TFileDirectory tfs);
    int fill(const GenParticleCollection & coll, art::Event const& event);
    std::string& name() { return _name; }

  private:
    std::string _name;
    
    TH1D* _hVer;
    TH1D* _hN;
    ValId _id;
    TH1D* _hp;
    ValPosition _pos;
  };
}


#endif