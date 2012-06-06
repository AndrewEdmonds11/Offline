#ifndef ProductionSolenoidGeom_PSVacuum_hh
#define ProductionSolenoidGeom_PSVacuum_hh

//
// $Id: PSVacuum.hh,v 1.1 2012/06/06 19:29:30 gandr Exp $
// $Author: gandr $
// $Date: 2012/06/06 19:29:30 $
//
// Original author Andrei Gaponenko
//

#include <vector>
#include <ostream>

#include "GeomPrimitives/inc/Tube.hh"
#include "Mu2eInterfaces/inc/Detector.hh"

#include "art/Persistency/Common/Wrapper.h"

namespace mu2e {

  class PSVacuumMaker;

  class PSVacuum : virtual public Detector {

  public:

    const Tube& vacuum() const { return vacuum_; }

  private:

    friend class PSVacuumMaker;

    // Private ctr: the class should only be constructed via PSVacuum::PSVacuumMaker.
    explicit PSVacuum(const Tube& vac)
      : vacuum_(vac)
    {}

    // Or read back from persistent storage
    PSVacuum() {}
    template<class T> friend class art::Wrapper;

    // The real enclosure shape is shown in docdb-2066 It is
    // approximated here by a cylinder closed with a flat end plate.
    Tube vacuum_;
  };

}

#endif/*ProductionSolenoidGeom_PSVacuum_hh*/
