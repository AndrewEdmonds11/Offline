#ifndef TrackerConditions_StrawResponse_hh
#define TrackerConditions_StrawResponse_hh
//
// StrawResponse collects the net response features of straws used in reconstruction 
//
// Original author David Brown, LBNL
//

// C++ includes
#include <iostream>
#include <vector>
// Mu2e includes
#include "Mu2eInterfaces/inc/ConditionsEntity.hh"
#include "fhiclcpp/ParameterSet.h"

namespace mu2e {
  class StrawHit;
  class StrawResponse : virtual public ConditionsEntity {
    public:
      // construct from parameters
      explicit StrawResponse(fhicl::ParameterSet const& pset);
      virtual ~StrawResponse();
      bool wireDistance(StrawHit const& strawhit, float shlen, float& wdist, float& wderr) const;
      float driftError(float DOCA) const;  // time domain error
      bool useDriftError() const { return _usederr; } 
      void print(std::ostream& os) const;
    private:
// helper functions
      float halfPropV(float kedep) const;
      float wpRes(float kedep, float wdist) const;
      static float PieceLine(std::vector<float> const& xvals, std::vector<float> const& yvals, float xval);

      // parametric data for calibration functions
      // TD reconstruction uses 1/2 the propagation velocity and depends on the
      // Dependence on position and straw length still needed FIXME!
      // (reconstructed) energy deposit
      std::vector<float> _edep; // energy deposit boundaries
      std::vector<float> _halfvp; // effective 1/2 propagation velocity by edep
      float _central; // max wire distance for central wire region
      std::vector<float> _centres; // wire center resolution by edep
      std::vector<float> _resslope; // resolution slope vs position by edep
      bool _usederr; // flag to use the doca-dependent calibration of the drift error
      std::vector<float> _derr; // parameters describing the drift error function
      float _wbuf; // buffer at the edge of the straws, in terms of sigma
      float _slfac; // factor of straw length to set 'missing cluster' hits
      float _errfac; // error inflation for 'missing cluster' hits
  };
}
#endif
