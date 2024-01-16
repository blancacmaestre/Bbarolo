// -----------------------------------------------------------------------
// object2D.cpp: Member functions for the Object2D class.
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
#include <vector>
#include <list>
#include <algorithm>
#include <math.h>
#include <Map/object2D.hh>
#include <Map/scan.hh>
#include <Map/voxel.hh>

namespace PixelInfo
{
   
  Object2D::Object2D(const Object2D& o) {
    operator=(o);
  }  

  
  Object2D& Object2D::operator= (const Object2D& o) {
      
    if(this == &o) return *this;
    this->scanlist = o.scanlist;
    this->numPix   = o.numPix;
    this->xSum     = o.xSum;
    this->ySum     = o.ySum;
    this->xmin     = o.xmin;
    this->ymin     = o.ymin;
    this->xmax     = o.xmax;
    this->ymax     = o.ymax;
    return *this;
  }  


  void Object2D::addPixel(long &x, long &y) {
      
  /// This function has three parts to it:
  /// First, it searches through the existing scans to see if 
  ///  a) there is a scan of the same y-value present, and 
  ///  b) whether the (x,y) pixel lies in or next to an existing 
  ///     Scan. If so, it is added and the Scan is grown if need be.
  ///  If this isn't the case, a new Scan of length 1 is added to 
  ///  the list. 
  ///  If the Scan list was altered, all are checked to see whether 
  ///  there is now a case of Scans touching. If so, they are 
  ///  combined and added to the end of the list.
  ///  If the pixel was added, the parameters are updated and the 
  ///  pixel counter incremented.
  ///  

    bool flagDone=false,flagChanged=false, isNew=false;

    for (std::vector<Scan>::iterator scan=scanlist.begin(); !flagDone && scan!=scanlist.end(); scan++){
        if(y == scan->itsY){ // if the y value is already in the list
            if(scan->isInScan(x,y)) flagDone = true; // pixel already here.
            else { // it's a new pixel!
                if((x==(scan->itsX-1)) || (x == (scan->itsX+scan->itsXLen)) ){
                    // if it is adjacent to the existing range, add to that range
                    if(x==(scan->itsX-1)) scan->growLeft();
                    else scan->growRight();
                    flagDone = true;
                    flagChanged = true;
                    isNew = true;
                }
            }     
        }
    }

    if(!flagDone){ 
        // we got to the end of the scanlist, so there is no pre-existing scan with this y value
        // add a new scan consisting of just this pixel
        Scan newOne(y,x,1);
        scanlist.push_back(newOne);
        isNew = true;
    }
    else if(flagChanged){ 
        // this is true only if one of the pre-existing scans has changed
        //
        // need to clean up, to see if there is a case of two scans when
        // there should be one. Only need to look at scans with matching
        // y-value
        //
        // because there has been only one pixel added, only 2 AT MOST
        // scans will need to be combined, so once we meet a pair that
        // overlap, we can finish.

        bool combined = false;
        std::vector<Scan>::iterator scan1=scanlist.begin(),scan2;
        for (; !combined && scan1!=scanlist.end(); scan1++){
            if(scan1->itsY==y){
                scan2=scan1;
                scan2++;
                for(; !combined && scan2!=scanlist.end(); scan2++){
                    if(scan2->itsY==y){
                        combined = scan1->addScan(*scan2);
                        if(combined){
                            scanlist.erase(scan2);
                        }
                    }   
                }
            }
        }
    }

    if(isNew){  
        // update the centres, mins, maxs and increment the pixel counter
        if(numPix==0){
            xSum = xmin = xmax = x;
            ySum = ymin = ymax = y;
        }
        else{
            xSum += x;
            ySum += y;
            if(x<xmin) xmin = x;
            if(x>xmax) xmax = x;
            if(y<ymin) ymin = y;
            if(y>ymax) ymax = y;
        }
            numPix++;
    }

  }


  void Object2D::addScan(Scan &scan) {
      
    long y=scan.getY();
    for(long x=scan.getX();x<=scan.getXmax();x++) addPixel(x,y);

  }


  bool Object2D::isInObject(long x, long y) {

    std::vector<Scan>::iterator scn;
    bool returnval=false;
    for(scn=scanlist.begin();scn!=scanlist.end()&&!returnval;scn++){
        returnval = ((y == scn->itsY) && (x>= scn->itsX) && (x<=scn->getXmax()));
    }
       
    return returnval;

  }
    

  void Object2D::calcParams() {
    
    xSum = 0;
    ySum = 0;
    std::vector<Scan>::iterator s;
    for(s=scanlist.begin();s!=scanlist.end();s++){
        if(s==scanlist.begin()){
            ymin = ymax = s->itsY;
            xmin = s->itsX;
            xmax = s->getXmax();
        }
        else{
            if(ymin>s->itsY)    ymin = s->itsY;
            if(xmin>s->itsX)    xmin = s->itsX;
            if(ymax<s->itsY)    ymax = s->itsY;
            if(xmax<s->getXmax()) xmax = s->getXmax();
        }

        ySum += s->itsY*s->getXlen();
        for(int x=s->itsX;x<=s->getXmax();x++)
            xSum += x;

    }

  }


  void Object2D::cleanup() {
      
    std::vector<Scan>::iterator s1=scanlist.begin(),s2;
    for (; s1!=scanlist.end(); s1++){
        s2=s1; s2++;
        for (; s2!=scanlist.end(); s2++){
            if(overlap(*s1,*s2)){
                s1->addScan(*s2);
                scanlist.erase(s2);
            }
        } 
    }

  }


  long Object2D::getNumDistinctY() {
      
    std::vector<long> ylist;
    if(scanlist.size()==0) return 0;
    if(scanlist.size()==1) return 1;
    std::vector<Scan>::iterator scn;
    for(scn=scanlist.begin();scn!=scanlist.end();scn++){
        bool inList = false;
        unsigned int j=0;
        long y = scn->itsY;
        while(!inList && j<ylist.size()){
            if(y==ylist[j]) inList=true;
            j++;
        };
        if(!inList) ylist.push_back(y);
    }
    return ylist.size();
  }


  long Object2D::getNumDistinctX() {

    std::vector<long> xlist;
    if(scanlist.size()==0) return 0;
    if(scanlist.size()==1) return 1;
    std::vector<Scan>::iterator scn;
    for(scn=scanlist.begin();scn!=scanlist.end();scn++){
        for(int x=scn->itsX;x<scn->getXmax();x++){
            bool inList = false;
            unsigned int j=0;
            while(!inList && j<xlist.size()){
                if(x==xlist[j]) inList=true;
                j++;
            };
            if(!inList) xlist.push_back(x);
        }
    }
    return xlist.size();
  }
  

  bool Object2D::scanOverlaps(Scan &scan) {
      
    bool returnval = false;
    for(std::vector<Scan>::iterator s=scanlist.begin();!returnval&&s!=scanlist.end();s++){
        returnval = returnval || s->overlaps(scan);
    }
    return returnval;
  }


  void Object2D::addOffsets(long xoff, long yoff) {
      
    std::vector<Scan>::iterator scn;
    for(scn=scanlist.begin();scn!=scanlist.end();scn++)
        scn->addOffsets(xoff,yoff);
    xSum += xoff*numPix;
    xmin += xoff; xmax += xoff;
    ySum += yoff*numPix;
    ymin += yoff; ymax += yoff;
  }
  

  double Object2D::getPositionAngle() {

    int sumxx=0;
    int sumyy=0;
    int sumxy=0;
    std::vector<Scan>::iterator scn=scanlist.begin();
    for(;scn!=scanlist.end();scn++){
        sumyy += (scn->itsY*scn->itsY)*scn->itsXLen;
        for(int x=scn->itsX;x<=scn->getXmax();x++){
            sumxx += x*x;
            sumxy += x*scn->itsY;
        }
    }

    // Calculate net moments
    double mxx = sumxx - xSum*xSum / double(numPix);
    double myy = sumyy - ySum*ySum / double(numPix);
    double mxy = sumxy - xSum*ySum / double(numPix);

    if(mxy==0){
        if(mxx>myy) return M_PI/2.;
        else return 0.;
    }

    // Angle of the minimum moment
    double tantheta = (mxx - myy + sqrt( (mxx-myy)*(mxx-myy) + 4.*mxy*mxy))/(2.*mxy);

    return atan(tantheta);
  }
  
  
  std::pair<double,double> Object2D::getPrincipleAxes() {

    double theta = getPositionAngle();
    double x0 = getXaverage();
    double y0 = getYaverage();
    std::vector<double> majorAxes, minorAxes;
    std::vector<Scan>::iterator scn=scanlist.begin();
    for(;scn!=scanlist.end();scn++){
        for(int x=scn->itsX;x<=scn->getXmax();x++){
            majorAxes.push_back((x-x0+0.5)*cos(theta) + (scn->itsY-y0+0.5)*sin(theta));
            minorAxes.push_back((x-x0+0.5)*sin(theta) + (scn->itsY-y0+0.5)*cos(theta));
        }
    }

    std::sort(majorAxes.begin(),majorAxes.end());
    std::sort(minorAxes.begin(),minorAxes.end());
    int size = majorAxes.size();
    std::pair<double,double> axes;
    axes.first = fabs(majorAxes[0]-majorAxes[size-1]);
    axes.second = fabs(minorAxes[0]-minorAxes[size-1]);
    if(axes.first<0.5) axes.first=0.5;
    if(axes.second<0.5) axes.second=0.5;
    return axes;

  }
  
  
  bool Object2D::canMerge(Object2D &other, float threshS, bool flagAdj) {
    
    long gap;
    if(flagAdj) gap = 1;
    else gap = long( ceil(threshS) );
    bool near = isNear(other,gap);
    if(near) return isClose(other,threshS,flagAdj);
    else return near;
  }
  
 
  bool Object2D::isNear(Object2D &other, long gap) {
      
    bool areNear;
    // Test X ranges
    if((xmin-gap)<other.xmin) areNear=((xmax+gap)>=other.xmin);
    else areNear=(other.xmax>=(xmin-gap));
    // Test Y ranges
    if(areNear){
        if((ymin-gap)<other.ymin) areNear=areNear&&((ymax+gap)>=other.ymin);
        else areNear=areNear&&(other.ymax>=(ymin-gap));
    }
    return areNear;
  }


  bool Object2D::isClose(Object2D &other, float threshS, bool flagAdj) {

    long gap = long(ceil(threshS));
    if(flagAdj) gap=1;

    bool close = false;

    long ycommonMin=std::max(ymin-gap,other.ymin)-gap;
    long ycommonMax=std::min(ymax+gap,other.ymax)+gap;

    std::vector<Scan>::iterator iter1,iter2;
    for(iter1=scanlist.begin();!close && iter1!=scanlist.end();iter1++){
        if(iter1->itsY >= ycommonMin && iter1->itsY<=ycommonMax){
            for(iter2=other.scanlist.begin();!close && iter2!=other.scanlist.end();iter2++){
                if(iter2->itsY >= ycommonMin && iter2->itsY<=ycommonMax){         
                    if(labs(iter1->itsY-iter2->itsY)<=gap){
                        if(flagAdj) {
                            if((iter1->itsX-gap)>iter2->itsX) 
                                close=((iter2->itsX+iter2->itsXLen-1)>=(iter1->itsX-gap));
                            else close = ( (iter1->itsX+iter1->itsXLen+gap-1)>=iter2->itsX);
                        }
                        else close = (minSep(*iter1,*iter2) < threshS);
                    }
                }
            }
        }
    }
        
    return close;
    
  }


  Object2D operator+ (Object2D lhs, Object2D rhs) {
      
    Object2D output = lhs;
    for(std::vector<Scan>::iterator s=rhs.scanlist.begin();s!=rhs.scanlist.end();s++)
        output.addScan(*s);
    return output;
  } 
  
  
  std::ostream& operator<< (std::ostream& theStream, Object2D& obj) {
      
    if(obj.scanlist.size()>1) obj.order();
    for(std::vector<Scan>::iterator s=obj.scanlist.begin();s!=obj.scanlist.end();s++)
        theStream << *s << "\n";
    theStream<<"---\n";
    return theStream;
  }

}
