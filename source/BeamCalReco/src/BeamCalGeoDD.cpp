#include "BeamCalGeoDD.hh"

#include <DD4hep/LCDD.h>
#include <DD4hep/Detector.h>
#include <DD4hep/UserExtension/BeamCalInfo.h>


class BeamCalInfo;

#include <vector>
#include <algorithm>

BeamCalGeoDD::BeamCalGeoDD(DD4hep::Geometry::LCDD const& lcdd): m_BeamCal(lcdd.detector("BeamCal")) {

}

//Wrappers around Gear Interface:
inline double                BeamCalGeoDD::getBCInnerRadius() const { 
return m_BeamCal.extension<BeamCalInfo>()->getInnerRadius(); 
}

inline double                BeamCalGeoDD::getBCOuterRadius() const { 
return m_BeamCal.extension<BeamCalInfo>()->getOuterRadius(); 
}

inline int                   BeamCalGeoDD::getBCLayers()      const { 
return m_BeamCal.extension<BeamCalInfo>()->getNumberOfLayers(); 
} 

inline int                   BeamCalGeoDD::getBCRings()       const { 
  return m_BeamCal.extension<BeamCalInfo>()->getNumberOfRings(); 
}

inline std::vector<double>   BeamCalGeoDD::getSegmentation()  const { 
  return m_BeamCal.extension<BeamCalInfo>()->getPhiSegmentationsPerRing(); 
}

inline std::vector<int>      BeamCalGeoDD::getNSegments()     const {
  return m_BeamCal.extension<BeamCalInfo>()->getNumberPhiSegmentationsPerRing(); 
}

inline double                BeamCalGeoDD::getCutout()        const {
  return m_BeamCal.extension<BeamCalInfo>()->getCutoutRadius(); 
}

inline double                BeamCalGeoDD::getBCZDistanceToIP() const { 
  return m_BeamCal.extension<BeamCalInfo>()->getZPosition(); 
}




void BeamCalGeoDD::countNumberOfPadsInRing(){
  //  m_PadsBeforeRing.clear();

  for (std::vector<int>::iterator it = m_Segments.begin(); it != m_Segments.end(); ++it) {
    m_PadsBeforeRing.push_back(m_nPadsPerLayer);//first ring has 0 entries! No it doesn't, yes it does have 0 BEFORE it
    m_nPadsPerLayer+= *it;
  }

  //  if(m_Segments.size() != m_PadsBeforeRing.size()) throw std::logic_error("this should not have happened");

  m_nPadsPerBeamCal = m_nLayers * m_nPadsPerLayer;
  //m_PadEnergies.resize(m_nPadsPerBeamCal, 0.0);
}



int BeamCalGeoDD::getPadsPerBeamCal() const{
  return m_nPadsPerBeamCal;
}

int BeamCalGeoDD::getPadsPerLayer() const {
  return m_nPadsPerLayer;
}

//layers start at 1, ring and pad start at 0
int BeamCalGeoDD::getPadIndex(int layer, int ring, int pad) const throw(std::out_of_range){
  if( layer < 1 || m_nLayers < layer) {//starting at 1 ending at nLayers
    throw std::out_of_range("Layer out of range:");
  } else if(ring < 0 || m_nRings <= ring) {//starting at 0, last entry is nRings-1
    throw std::out_of_range("Ring out of range:");
  } else if( pad < 0 || m_Segments[ring] <= pad ) {//starting at 0
    throw std::out_of_range("Pad out of range:");
  }

  return (layer-1) * (m_nPadsPerLayer) + m_PadsBeforeRing[ring] + (pad);
}


void BeamCalGeoDD::getLayerRingPad(int padIndex, int& layer, int& ring, int& pad) const{

  //how often does nPadsPerLayer fit into padIndex;
  //layer starts at 1!
  layer = getLayer(padIndex);// ( padIndex / m_nPadsPerLayer ) + 1;
  ring = getRing(padIndex);

  const int ringIndex(padIndex % m_nPadsPerLayer);
  pad = ringIndex - m_PadsBeforeRing[ring];

#ifdef DEBUG
  if (padIndex != this->getPadIndex(layer, ring, pad) ) {
    std::stringstream error;
    error << "PadIndex " 
  	  << std::setw(7) << padIndex
  	  << std::setw(7) << this->getPadIndex(layer, ring, pad);
    throw std::logic_error(error.str());
  }
#endif

  return;

}//getLayerRingPad

int BeamCalGeoDD::getRing(int padIndex) const {
  //int ringIndex = padIndex % m_nPadsPerLayer;
  std::vector<int>::const_iterator element =  std::upper_bound(m_PadsBeforeRing.begin()+1, m_PadsBeforeRing.end(),
							       padIndex % m_nPadsPerLayer);
  return element - m_PadsBeforeRing.begin() - 1 ;
}

int BeamCalGeoDD::getLocalPad(int padIndex) const {
  int layer, ring, pad;
  getLayerRingPad(padIndex, layer, ring, pad);
  return pad;
}


int BeamCalGeoDD::getLayer(int padIndex) const {
  //layer starts at 1
  return   ( padIndex / m_nPadsPerLayer ) + 1;
}

//ARG
double BeamCalGeoDD::getPadPhi(int ring, int pad) const {
  double phi = 0;
  if( ring < m_firstFullRing) {
    phi += m_fullKeyholeCutoutAngle/2.0;
    double deltaPhi = (360.0 - m_fullKeyholeCutoutAngle/2.0)/double(m_Segments[ring]);
    phi +=  deltaPhi * double(pad) + deltaPhi/2.0;
  } else {
    double deltaPhi = 360.0 / double(m_Segments[ring]);
    //Pads still start at the top of the cutout! which is negative -180 + half cutoutangle
    phi +=  m_fullKeyholeCutoutAngle/2.0 + deltaPhi * double(pad) + deltaPhi/2.0;
  }

  phi  += 180.0;
  if( phi > 360.0 ) {
    phi -= 360.0;
  }

  //  std::cout << std::setw(10) << ring << std::setw(10) << pad << std::setw(10) << phi  << std::endl;
  return phi;
}


double BeamCalGeoDD::getPadPhi(int globalPandIndex) const {
  int layer, ring, pad;
  getLayerRingPad(globalPandIndex, layer, ring, pad);
  return getPadPhi(ring, pad);
}

double BeamCalGeoDD::getThetaFromRing(double averageRing) const  {
  const double radiusStep = getBCOuterRadius() - getBCInnerRadius();
  const double radius = getBCInnerRadius() + ( averageRing + 0.5 )  * ( ( radiusStep  ) / double(m_nRings) );
  return atan( radius / getBCZDistanceToIP() );
}



/**
 * Returns true if the pads are in adjacent cells in the same ring
 * or if they are in adjacent rings, then it will return true if the overlap in phi is smaller than
 * some amount of degrees...
 * do the layers have to be the same? --> use boolean flag
 * What about adjacent layers and the same index? we are going to write a second function for those, if we have to
 * Pads are Neighbours if they are in the same Ring, but have Pads with a difference of one
 * What about the keyhole cutout? They will not be considered to be neighbours
 */

bool BeamCalGeoDD::arePadsNeighbours(int globalPadIndex1, int globalPadIndex2, bool mustBeInSameLayer) const {
  int layerIndex1, layerIndex2, ringIndex1, ringIndex2, padIndex1, padIndex2;
  getLayerRingPad(globalPadIndex1, layerIndex1, ringIndex1, padIndex1);
  getLayerRingPad(globalPadIndex2, layerIndex2, ringIndex2, padIndex2);

  if( mustBeInSameLayer and ( not ( layerIndex1 == layerIndex2 ) ) ) {
    //    std::cout << "Not Neighbours: Layers differ"  << std::endl;
    return false;
  }
  //If they are in the same ring
  if( ringIndex1 == ringIndex2 ) {

    if( 
       //The index of neighbouring pads differs by 1
       ( abs( padIndex1 - padIndex2 ) <= 1 ) or
       // or one is zero and the other is m_segments[ringIndex1] - 1
       (  ringIndex1 >= m_firstFullRing && ( ( abs( padIndex1 - padIndex2 ) + 1 )  == m_Segments[ringIndex1] )  )
	) {
      // std::cout << "Neighbours: Adjacent pads in same ring"  
      // 		<< std::setw(5) << padIndex1
      // 		<< std::setw(5) << padIndex2
      // 		<< std::endl;
      return true;
    } else { //if they are in the same ring, but not neighbours, we can bail out here
      // std::cout << "Not Neighbours: Same ring but not adjacent pads" 
      // 		<< std::setw(5) << padIndex1
      // 		<< std::setw(5) << padIndex2
      // 		<< std::endl;
      return false;
    }
  }//if in the same ring

  //adjacent rings' indices differ by 1
  if( abs( ringIndex1 - ringIndex2 ) == 1 ) {
    //Angles are in Degrees!
    double phi1 = getPadPhi(ringIndex1, padIndex1);
    double phi2 = getPadPhi(ringIndex2, padIndex2);

    if( fabs( phi1 - phi2 ) < 18.0 ) {
      // std::cout << "Neighbours: Adjacent pads in different ring"  
      // 		<< std::setw(10) << phi1
      // 		<< std::setw(10) << phi2
      // 		<< std::endl;
      return true;
    } else {
      // std::cout << "Not Neighbours: Non adjacent pads in different ring"  
      // 		<< std::setw(10) << phi1
      // 		<< std::setw(10) << phi2
      // 		<< std::endl;
      return false;
    }
  }

  return false;

}