// -----------------------------------------------------------------------
// Object3D.cc: Member functions for Object3D class.
// -----------------------------------------------------------------------

/*-----------------------------------------------------------------------
 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the
 Free Software Foundation; either version 2 of the License, or (at your
 option) any later version.

 BBarolo is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 for more details.

 You should have received a copy of the GNU General Public License
 along with BBarolo; if not, write to the Free Software Foundation,
 Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA

 Correspondence concerning BBarolo may be directed to:
    Internet email: enrico.diteodoro@gmail.com
-----------------------------------------------------------------------*/

#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <Map/voxel.hh>
#include <Map/scan.hh>
#include <Map/object2D.hh>
#include <Map/object3D.hh>


namespace PixelInfo
{
  template <class T>
  Object3D<T>::Object3D() {
      
    numVox=0;
    xSum = 0;
    ySum = 0;
    zSum = 0;
    xmin = xmax = ymin = ymax = zmin = zmax = -1;
  }
  
  
  template <class T>
  Object3D<T>::Object3D(const Object3D<T>& o) {
      
    operator=(o);
  }
  
   
  template <class T> 
  Object3D<T>& Object3D<T>::operator= (const Object3D<T>& o) {
      
    if(this == &o) return *this;
    this->chanlist = o.chanlist;
    this->numVox  = o.numVox;
    this->xSum    = o.xSum;
    this->ySum    = o.ySum;
    this->zSum    = o.zSum;
    this->xmin    = o.xmin;
    this->ymin    = o.ymin;
    this->zmin    = o.zmin;
    this->xmax    = o.xmax;
    this->ymax    = o.ymax;
    this->zmax    = o.zmax;
    return *this;
  }
  
  
  template <class T>
  Object3D<T> operator+ (Object3D<T> lhs, Object3D<T> rhs) {
      
    Object3D<T> output = lhs;
    for(typename std::map<long, Object2D<T> >::iterator it = rhs.chanlist.begin(); it!=rhs.chanlist.end();it++)
        output.addChannel(it->first, it->second);
    return output;
  }
  
  
  template <class T>  
  float Object3D<T>::getXaverage() {
      
    if(numVox>0) return xSum/float(numVox); 
    else return 0.;
  }
  
 
  template <class T>
  float Object3D<T>::getYaverage() {
      
    if(numVox>0) return ySum/float(numVox);
    else return 0.;
  }
  
  
  template <class T>
  float Object3D<T>::getZaverage() {
      
    if(numVox>0) return zSum/float(numVox); 
    else return 0.;
  }

  
  template <class T> 
  bool Object3D<T>::isInObject(long x, long y, long z) {
      
    typename std::map<long,Object2D<T> >::iterator it=chanlist.begin();
    while(it!=chanlist.end() && it->first!=z) it++;
    if(it==chanlist.end()) return false;
    else return it->second.isInObject(x,y);
  }
  
    
  template <class T>
  void Object3D<T>::addPixel(long x, long y, long z) {
 
    typename std::map<long,Object2D<T> >::iterator it=chanlist.begin();
    while(it!=chanlist.end() && it->first!=z) it++;

    if(it==chanlist.end()){ //new channel
        Object2D<T> obj;
        obj.addPixel(x,y);
        chanlist.insert( std::pair<int,Object2D<T> >(z,obj) );
        // update the centres, min & max, as well as the number of voxels
        if(numVox==0){
            xSum = xmin = xmax = x;
            ySum = ymin = ymax = y;
            zSum = zmin = zmax = z;
        }
        else {
            xSum += x;
            ySum += y;
            zSum += z;
            if(x<xmin) xmin = x;
            if(x>xmax) xmax = x;
            if(y<ymin) ymin = y;
            if(y>ymax) ymax = y;
            zmin = chanlist.begin()->first;
            zmax = chanlist.rbegin()->first;
        }
        numVox++;   
    }
    else{ // existing channel
      // This method deals with the case of a new pixel being added AND
      // with the new pixel already existing in the Object2D
 
      // Remove that channel's information from the Object's information
      xSum -= it->second.xSum;
      ySum -= it->second.ySum;
      zSum -= z*it->second.numPix;
      numVox -= it->second.numPix;

      // Add the pixel
      it->second.addPixel(x,y);
    
      numVox += it->second.numPix;
      xSum += it->second.xSum;
      ySum += it->second.ySum;
      zSum += z*it->second.numPix;
      if(x<xmin) xmin = x;
      if(x>xmax) xmax = x;
      if(y<ymin) ymin = y;
      if(y>ymax) ymax = y;
      // don't need to do anything to zmin/zmax -- the z-value is
      // already in the list
    }

  }
  
   
  template <class T>
  void Object3D<T>::addScan(Scan<T> s, long z) {
      
    long y=s.getY();
    for(int x=s.getX(); x<=s.getXmax(); x++) 
        addPixel(x,y,z);
  }


  template <class T>
  void Object3D<T>::addChannel(const long &z, Object2D<T> &obj) {

    typename std::map<long,Object2D<T> >::iterator it=chanlist.begin();
    while(it!=chanlist.end() && it->first!=z) it++;

    if(it == chanlist.end()) { // channel z is not already in object, so add it.
        chanlist.insert(std::pair<long,Object2D<T> >(z,obj));
        if(numVox == 0) { // if there are no other pixels, so initialise mins,maxs,sums
            xmin = obj.xmin;
            xmax = obj.xmax;
            ymin = obj.ymin;
            ymax = obj.ymax;
            zmin = zmax = z;
            xSum = obj.xSum;
            ySum = obj.ySum;
            zSum = z * obj.getSize();
        }
        else { // there are other pixels in other channels, so update mins, maxs, sums
            if(obj.xmin<xmin) xmin = obj.xmin;
            if(obj.xmax>xmax) xmax = obj.xmax;
            if(obj.ymin<ymin) ymin = obj.ymin;
            if(obj.ymax>ymax) ymax = obj.ymax;
            if(z<zmin) zmin = z;
            if(z>zmax) zmax = z;
            xSum += obj.xSum;
            ySum += obj.ySum;
            zSum += z * obj.getSize();
        }
        numVox += obj.getSize();
    }
    else { // channel is already present, so need to combine objects.
        xSum -= it->second.xSum;
        ySum -= it->second.ySum;
        zSum -= z*it->second.getSize();
        numVox -= it->second.getSize();
        it->second = it->second + obj;
        xSum += it->second.xSum;
        ySum += it->second.ySum;
        zSum += z*it->second.getSize();
        numVox += it->second.getSize();
        if(obj.xmin<xmin) xmin = obj.xmin;
        if(obj.xmax>xmax) xmax = obj.xmax;
        if(obj.ymin<ymin) ymin = obj.ymin;
        if(obj.ymax>ymax) ymax = obj.ymax;
    }
  }
  
  
  template <class T>
  unsigned long Object3D<T>::getSpatialSize() {
      
    Object2D<T> spatialMap = getSpatialMap();
    return spatialMap.getSize();
  }
  
  
  template <class T>
  Object2D<T> Object3D<T>::getSpatialMap() {
    
    Object2D<T> spatialMap;
    for(typename std::map<long, Object2D<T> >::iterator it = chanlist.begin(); 
        it!=chanlist.end();it++){
        spatialMap = spatialMap + it->second;
    }
    return spatialMap;
  }
  
  
  template <class T> 
  void Object3D<T>::calcParams() {
      
    xSum = 0;
    ySum = 0;
    zSum = 0;
    numVox = 0;

    zmin = chanlist.begin()->first;
    zmax = chanlist.rbegin()->first;
    for(typename std::map<long, Object2D<T> >::iterator it = chanlist.begin(); it!=chanlist.end();it++) {

        it->second.calcParams();
        if(it==chanlist.begin()) {
            xmin = it->second.getXmin();
            xmax = it->second.getXmax();
            ymin = it->second.getYmin();
            ymax = it->second.getYmax();
        }
        else{
            if(it->second.xmin<xmin) xmin = it->second.xmin;
            if(it->second.xmax>xmax) xmax = it->second.xmax;
            if(it->second.ymin<ymin) ymin = it->second.ymin;
            if(it->second.ymax>ymax) ymax = it->second.ymax;
        }
        xSum += it->second.xSum;
        ySum += it->second.ySum;
        zSum += it->first * it->second.getSize();
        numVox += it->second.getSize();
    }

  }
  
  
  template <class T>
  void Object3D<T>::print(std::ostream& theStream) {
      
    for(typename std::map<long, Object2D<T> >::iterator it = chanlist.begin(); it!=chanlist.end();it++){
        for(typename std::vector<Scan<T> >::iterator s=it->second.scanlist.begin();s!=it->second.scanlist.end();s++){
            theStream << *s << "," << it->first << "\n";
        }
    }  
    theStream << "\n";
  }


  template <class T>
  std::ostream& operator<< ( std::ostream& theStream, Object3D<T>& obj) {
    
    obj.print(theStream);
    return theStream;
  }
  
    
  template <class T>
  std::vector<Voxel<T> > Object3D<T>::getPixelSet() {
    
  /// Returns a vector of the Voxels in the object. All
  /// flux values are set to 0.

    std::vector<Voxel<T> > voxList(numVox);
    long count = 0;
    for(typename std::map<long, Object2D<T> >::iterator it = chanlist.begin(); it!=chanlist.end();it++) {
        long z = it->first;
        for(typename std::vector<Scan<T> >::iterator s=it->second.scanlist.begin();s!=it->second.scanlist.end();s++) {
            long y = s->getY();
            for(long x=s->getX(); x<=s->getXmax(); x++) {
                voxList[count].setXYZF(x,y,z,0);
                count++;
            }
        }
    }
    return voxList;

  }
  
  
  template <class T>
  std::vector<Voxel<T> > Object3D<T>::getPixelSet(T *array, int *dim) {
      
    /// Returns a vector of Voxels with the flux values for each voxel
    /// taken from the array provided. No check is made as to whether
    /// the pixels fall in the array boundaries
    ///
    /// \param array    Array of pixel values
    /// \param dim      Array of axis dimensions

    std::vector<Voxel<T> > voxList(numVox);
    long count = 0;
    for(typename std::map<long, Object2D<T> >::iterator it = chanlist.begin(); it!=chanlist.end();it++) {
        long z = it->first;
        for(typename std::vector<Scan<T> >::iterator s=it->second.scanlist.begin();s!=it->second.scanlist.end();s++){
            long y = s->getY();
            for(long x=s->getX(); x<=s->getXmax(); x++){
                voxList[count].setXYZF(x,y,z,array[x+dim[0]*y+dim[0]*dim[1]*z]);
                count++;
            }
        }
    }
    return voxList;

  }
  
  
  template <class T>
  std::vector<long> Object3D<T>::getChannelList() {

    std::vector<long> chanlist;
    typename std::map<long, Object2D<T> >::iterator it;
    for(it = this->chanlist.begin(); it != this->chanlist.end(); it++) {
        chanlist.push_back(it->first);
    }
    return chanlist;
  }
  
  
  template <class T>
  Object2D<T> Object3D<T>::getChanMap(long z) {
    
    Object2D<T> obj;
    typename std::map<long,Object2D<T> >::iterator it=chanlist.begin();
    while(it!=chanlist.end() && it->first!=z) it++;
    if(it==chanlist.end()) obj = Object2D<T>();
    else obj = it->second;
    return obj;
  }
  
  
  template <class T>
  int Object3D<T>::getMaxAdjacentChannels() {
      
  /// Find the maximum number of contiguous channels in the
  /// object. Since there can be gaps in the channels included in an
  /// object, we run through the list of channels and keep track of
  /// sizes of contiguous segments. Then return the largest size.

    int maxnumchan=0;
    int zcurrent=0, zprevious,zcount=0;
    typename std::map<long, Object2D<T> >::iterator it;
    for(it = chanlist.begin(); it!=chanlist.end();it++) {
        if(it==chanlist.begin()){
            zcount++;
            zcurrent = it->first;
        }
        else { 
            zprevious = zcurrent;
            zcurrent = it->first;
            if (zcurrent-zprevious>1) {
                maxnumchan = std::max(maxnumchan, zcount);
                zcount=1;
            }
            else zcount++;
        }
    }
    maxnumchan = std::max(maxnumchan,zcount);
    return maxnumchan;
  }
  
  
  template <class T> 
  void Object3D<T>::addOffsets(long xoff, long yoff, long zoff) {
      
    std::map<long,Object2D<T> > newmap;
    for(typename std::map<long, Object2D<T> >::iterator it = chanlist.begin(); it!=chanlist.end();it++){
        std::pair<long, Object2D<T> > newOne(it->first+zoff, it->second);
        newOne.second.addOffsets(xoff,yoff);
        newmap.insert(newOne);
    }
    chanlist.clear();
    chanlist = newmap;
    if(numVox>0){
        xSum += xoff*numVox;
        xmin += xoff; xmax += xoff;
        ySum += yoff*numVox;
        ymin += yoff; ymax += yoff;
        zSum += zoff*numVox;
        zmin += zoff; zmax += zoff;
    }
  }
  
  
  // Explicit instantiation of the class
  template class Object3D<short>;
  template class Object3D<int>;
  template class Object3D<long>;
  template class Object3D<float>;
  template class Object3D<double>;

}
