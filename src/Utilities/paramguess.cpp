//--------------------------------------------------------------------
// paramguess.cpp: Definitions of functions for the ParamGuess class.
//--------------------------------------------------------------------

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
#include <Arrays/cube.hh>
#include <Map/detection.hh>
#include <Tasks/galmod.hh>
#include <Utilities/paramguess.hh>
#include <Utilities/utils.hh>
#include <Utilities/lsqfit.hh>
#include <Utilities/gnuplot.hh>
#include <Tasks/moment.hh>
#include <Tasks/ellprof.hh>


template <class T>
ParamGuess<T>::ParamGuess(Cube<T> *c, Detection<T> *object) {
    
    in  = c;
    obj = object;
    obj->calcFluxes(obj->getPixelSet(in->Array(), in->AxisDim()));
    obj->calcWCSparams(in->Head());
    obj->calcIntegFlux(in->DimZ(), obj->getPixelSet(in->Array(), in->AxisDim()), in->Head());
    
    /// Extracting intensity and velocity field
    Vemap = new T[in->DimX()*in->DimY()];
    Intmap = new T[in->DimX()*in->DimY()];
    std::vector<Voxel<T> > voxelList = obj->getPixelSet(in->Array(), in->AxisDim());
    float *fluxint = new float[in->DimX()*in->DimY()];
    float *fluxsum = new float[in->DimX()*in->DimY()];
    for (int i=0; i<in->DimX()*in->DimY();i++) fluxint[i] = fluxsum[i] = Intmap[i]= 0;
    
    for(auto &vox : voxelList) {
        int x = vox.getX();
        int y = vox.getY();
        int z = vox.getZ();
        float flux = FluxtoJy(in->Array(x,y,z),in->Head());
        fluxsum[x+y*in->DimX()] += flux;
        fluxint[x+y*in->DimX()] += flux*in->getZphys(z);
        Intmap[x+y*in->DimX()] += flux;
    }
    
    totflux_obs=0;
    for (int i=0; i<in->DimX()*in->DimY();i++) {
        totflux_obs += fluxsum[i];
        Vemap[i] = AlltoVel(fluxint[i]/fluxsum[i], in->Head());
    }
    
    // Initializing radsep to the Beam size
    radsep = in->Head().Bmaj()*arcsconv(in->Head().Cunit(0));
    
    delete [] fluxint;
    delete [] fluxsum;
} 


template <class T>
void ParamGuess<T>::findAll() {
    
    // Front-end function to estimate all geometrical and kinematical
    // parameters needed by BBarolo's 3DFIT task

    // Estimating centre position
    findCentre();
    // Estimating systemic velocity
    findSystemicVelocity();
    // Estimating position angle
    findPositionAngle();
    // Estimating inclination angle
    findInclination();
    // Estimating number of rings and width
    findRings();
    // Estimating rotation velocity
    findRotationVelocity();
}


template <class T>
void ParamGuess<T>::findCentre() {
    
    // X-Y centres are estimated from the centroids of the 
    // object detected by the source-finding algorithm. 
    xcentre = (obj->getXcentre()+obj->getXaverage())/2.;
    ycentre = (obj->getYcentre()+obj->getYaverage())/2.;
}


template <class T>
void ParamGuess<T>::findSystemicVelocity() {
    
    // Systemic velocity is estimated from the total spectrum of 
    // the object detected by the source-finding algorithm. 
    vsystem = obj->getVsys();
}


template <class T>
void ParamGuess<T>::findRotationVelocity() {
    
    // Rotation velocity is estimated from the W50 of spectrum of 
    // the object detected by the source-finding algorithm. 
    vrot=fabs(obj->getW50()/2.)/sin(inclin*M_PI/180.);  
}


template <class T>
void ParamGuess<T>::findPositionAngle(int algorithm) {
    
    ////////////////////////////////////////////////////////////////////////////
    // Estimating Position angle using several different algorithms
    // algorithm = 1: velocity differences from the VSYS
    // algorithm = 2: regions of highest/lowest velocities
    //
    // NB: xcenter, ycenter, vsystem need to be set before calling this function
    // This function sets posang, pmaj, pmin
    ////////////////////////////////////////////////////////////////////////////
    
    // Getting maximum and minimum velocity in the spectral range of the cube
    T velmin = AlltoVel<T>(in->getZphys(0),in->Head());
    T velmax = AlltoVel<T>(in->getZphys(in->DimZ()-1),in->Head());
    if (velmin>velmax) std::swap(velmin,velmax);
    // Getting maximum and minimum coordinates of detection
    int Xmin=obj->getXmin(), Ymin=obj->getYmin();
    int Xmax=obj->getXmax(), Ymax=obj->getYmax();
    
    if (algorithm == 1) {
        // This algorithm calculates the median difference between a velocity 
        // on the  velocity field and the systemic velocity for any possible 
        // position angle. The PA that returns the highest median value is the 
        // best kinematical PA.
        
        double maxdev=0, bestpa=0, p=0, vl=0, vr=0;
        // Loop over PA with step 0.5 degrees
        while (p<180) {
            std::vector<T> vdev;
            double sumleft=0, sumright=0;
            if (p>45 && p<135) {
                // If 45 < PA < 135 it is better to loop over y
                for (int y=Ymin; y<=Ymax; y++) {
                    // Calculating (x,y) coordinates to sample velocity field
                    int x = lround(1/tan(p*M_PI/180.)*(y-ycentre)+xcentre);
                    if (p==90) x = lround(xcentre);
                    long npix = y*in->DimX()+x; 
                    bool isOK = x>=Xmin && x<=Xmax && !isNaN<T>(Vemap[npix]) && 
                                Vemap[npix]>=velmin && Vemap[npix]<=velmax;
                    if (!isOK) continue;
                    // Collecting the absolute difference from the VSYS
                    vdev.push_back(fabs(Vemap[npix]-vsystem));
                    // Getting info on which side we have the highest velocity
                    if (p==90) {
                        if (y>ycentre) sumleft += Vemap[npix]-vsystem;
                        else sumright += Vemap[npix]-vsystem;
                    }
                    else {
                        if (x<xcentre) sumleft += Vemap[npix]-vsystem;
                        else sumright += Vemap[npix]-vsystem;
                    }
                }
            }
            else {
                // Loop over x in any other PA case
                for (int x=Xmin; x<=Xmax; x++) {
                    // Calculating (x,y) coordinates to sample velocity field
                    int y = lround(tan(p*M_PI/180.)*(x-xcentre)+ycentre);
                    long npix = y*in->DimX()+x;
                    bool isOK = y>=Ymin && y<=Ymax && !isNaN<T>(Vemap[npix]) && 
                                Vemap[npix]>=velmin && Vemap[npix]<=velmax;
                    if (!isOK) continue;
                    // Collecting the absolute difference from the VSYS
                    vdev.push_back(fabs(Vemap[npix]-vsystem));
                    // Getting info on which side we have the highest velocity
                    if (x<xcentre) sumleft += Vemap[npix]-vsystem;
                    else sumright += Vemap[npix]-vsystem;
                }
            }
            
            // Calculating the median deviation from VSYS
            T median = findMedian<T>(vdev, vdev.size());
            // If the median is so far the highest, assign best pa
            if (median>maxdev && fabs(median)<1E16) {
                maxdev = median;
                bestpa = p;
                vl = sumleft;
                vr = sumright;
            }

            p+=0.5;
        }
    
        // Rotate the PA to conform to BBarolo's definition.
        if (vl<vr) {
            if (bestpa<90) posang = 270+bestpa;
            else posang = 90+bestpa;
        }
        else {
            if (bestpa<90) posang = 90+bestpa;
            else posang = bestpa-90;
        }
        
    }

    else if (algorithm == 2) {
        /// This algorithm samples the velocity field in rectangular regions 
        /// with size equal to the beam size. In each region it calculates the 
        /// median velocity value and finds the two spots where this median value 
        /// is the highest and the lowest. Then, a linear regression between 
        /// these two regions is performed and the position angle estimated.

        float vel_high = vsystem;
        float vel_low  = vsystem;   
        int range = ceil(in->Head().Bmaj()/in->Head().PixScale());
        int coord_high[2]={0,0}, coord_low[2]={0,0};
        int xsize = fabs(obj->getXmax()-obj->getXmin())+1;
        int ysize = fabs(obj->getYmax()-obj->getYmin())+1;
        
        for (int y=range; y<ysize-range; y++) {
            for (int x=range; x<xsize-range; x++) {
                long npix = (y+Ymin)*in->DimX()+x+Xmin;
                if (isNaN<T>(Vemap[npix])) continue;
                std::vector<T> vec;
                for (int yi=y-range; yi<=y+range; yi++) 
                    for (int xi=x-range; xi<=x+range; xi++) 
                        vec.push_back(Vemap[(yi+Ymin)*in->DimX()+xi+Xmin]);
                T median = findMedian<T>(vec, vec.size());
                if (median<vel_low && median>=velmin) {
                    vel_low = median;
                    coord_low[0] = x+Xmin;
                    coord_low[1] = y+Ymin;
                }
                if (median>vel_high && median<=velmax) {
                    vel_high = median;
                    coord_high[0] = x+Xmin;
                    coord_high[1] = y+Ymin;
                }
            }
        }

        std::vector<int> xx(3), yy(3);
        xx[0]=coord_low[0]; xx[1]=coord_high[0]; xx[2]=lround(xcentre);
        yy[0]=coord_low[1]; yy[1]=coord_high[1]; yy[2]=lround(ycentre);
        float rmaj, errmaj[2];
    
        /// Linear regression between the center and the two point found previously.
        /// For not including the center, just change the last parameter from 
        /// 2 to 1 in the function below.
        int a = linear_reg<int>(3, xx, yy, pmaj, errmaj, rmaj, 0, 2);
        
        float ang = atan(pmaj[0]);  
        if (coord_high[0]>=xcentre) {   
            if (ang<M_PI_2) posang = (3*M_PI_2+ang)*180/M_PI;
            else posang = (M_PI_2+ang)*180/M_PI;
        }
        else {
            if (ang<M_PI_2) posang = (M_PI_2+ang)*180/M_PI;
            else posang = (ang-M_PI_2)*180/M_PI;    
        }

    }
    else {
        std::cerr << "PARAMGUESS ERROR: unknown algorithm value " << algorithm << std::endl;
        std::terminate();
    }
    
    // Setting angular coefficient and zero point of major/minor axis
    setAxesLine(xcentre, ycentre, posang);
    
}


template <class T>
void ParamGuess<T>::findInclination(int algorithm) {
    
    ////////////////////////////////////////////////////////////////////////////
    // Estimating Inclination angle using several different algorithms
    // algorithm = 1: Length of major/minor axis on the velocity field.
    // algorithm = 2: Ellipse with largest number of valid pixels.
    // algorithm = 3: Fit a model intensity map to the observed one.
    //
    // NB: xcenter, ycenter, vsystem and posang need to be set before calling 
    // this function.
    // This function sets inclin, Rmax, nrings, radsep
    ////////////////////////////////////////////////////////////////////////////
    
    // We always start with algorithm==1. This will provide also initial 
    // guesses for other algorithms.
    
    /// Estimating axis lengths for major and minor axis
    T axmaj = findAxisLength(pmaj, major_max, major_min);
    T axmin = findAxisLength(pmin, minor_max, minor_min);
    
    if (axmin>axmaj) {
        std::cout << "---------------> WARNING - Finding initial parameters <--------------\n"
                  << " The major axis is shorter than the minor axis. They will be swapped\n"
                  << " for estimating the inclination.\n"
                  << " The galaxy seems to be less elongated in the kinematical axis!!\n\n";
        std::swap(axmin, axmaj);
    }
    
    // Inclination angle and maximum radius
    inclin = acos(axmin/axmaj)*180./M_PI;
    Rmax = axmaj*in->Head().PixScale()*arcsconv(in->Head().Cunit(0));
    
    if (algorithm==1) {
        // We are happy with what we have done above. Just return
        return;
    }
    else if (algorithm==2 || algorithm==3) {
        /// Algorithm 2 finds the ellipses that contains the largest number of
        /// non-NaN pixels on the velocity field.
        ///
        /// Algorithm 3 minimizes the difference between a model intensity map
        /// and the observed intensity map.
        
        if (algorithm==2) func = &ParamGuess<T>::funcEllipse;
        else func = &ParamGuess<T>::funcIncfromMap;
        
        int ndim=2; 
    
        T point[ndim], dels[ndim];
        T **p = allocate_2D<T>(ndim+1,ndim);
        
        point[0] = Rmax;
        point[1] = inclin;
        //point[2] = posang;
        //point[3] = xcentre;
        //point[4] = ycentre;
    
        /// Determine the initial simplex.
        for (int i=0; i<ndim; i++) {
            dels[i]  = 0.1*point[i]; 
            point[i] = point[i]-0.05*point[i];      
        }
    
        for (int i=0; i<ndim+1; i++) {
            for (int j=0; j<ndim; j++) p[i][j]=point[j];
            if (i!=0) p[i][i-1] += dels[i-1];
        }

        bool OK = fitSimplex(ndim, p);
        
        if (OK) {
            Rmax   = p[0][0];
            inclin = p[0][1];
            //double Axmin = Rmax*cos(inclin/180.*M_PI);
            //double Axmin_corr = Axmin - in->Head().Bmaj()/in->Head().PixScale();
            //double Axmaj_corr = Rmax - in->Head().Bmaj()/in->Head().PixScale();
            //inclin = acos(Axmin_corr/Axmaj_corr)*180/M_PI;
            //posang = parmin[2];
            //xcentre= parmin[3];
            //ycentre= parmin[4];
            deallocate_2D(p,ndim+1);
        }
        else {
            std::cerr << "PARAMGUESS ERROR: Error while estimating inclination." << std::endl;
            std::terminate();
        }
    }
    else {
        std::cerr << "PARAMGUESS ERROR: unknown algorithm value " << algorithm << std::endl;
        std::terminate();
    }
    
}


template <class T>
void ParamGuess<T>::findRings() {

    nrings = lround(Rmax/radsep);
    if (nrings<5) {
        radsep /= 2.;
        nrings = lround(Rmax/radsep);
    }
}


template <class T>
T ParamGuess<T>::findAxisLength(float *lpar, int *coords_up, int *coords_low) {
    
    // Reset coordinates
    coords_up[0] = coords_up[1] = coords_low[0] = coords_low[1] = 0;
    
    double axis_r_l=0., axis_r_r=0;
    int Xmin=obj->getXmin(), Ymin=obj->getYmin();
    int Xmax=obj->getXmax(), Ymax=obj->getYmax();
    
    // Finding axis length
    double p = atan(lpar[0])*180./M_PI;
    if (p>=180) p -= 180;
    if (p>45 && p<135) {
        // If 45 < angle < 135 it is better to loop over y
        for (int y=Ymin; y<=Ymax; y++) {
            // Calculating (x,y) coordinates to sample velocity field
            int x = lround((y-lpar[1])/lpar[0]);
            if (p==90.) x = lround(xcentre);
            long npix = y*in->DimX()+x; 
            bool isOK = x>=Xmin && x<=Xmax && !isNaN<T>(Vemap[npix]);
            if (!isOK) continue;
            double r = sqrt(pow(double(x-xcentre),2.)+pow(double(y-ycentre),2.));
            if (p==90) {    // Special case for PA=90
                if (r>axis_r_l && y<=ycentre) {
                    axis_r_l = r;
                    coords_up[0] = x; coords_up[1] = y;
                }
                if (r>axis_r_r && y>ycentre) {
                    axis_r_r = r;
                    coords_low[0] = x; coords_low[1] = y;
                }
            }
            else {
                if (r>axis_r_l && x<=xcentre) {
                    axis_r_l = r;
                    coords_up[0] = x; coords_up[1] = y;
                }
                if (r>axis_r_r && x>xcentre) {
                    axis_r_r = r;
                    coords_low[0] = x; coords_low[1] = y;
                }
            }
        }
    }
    else {
        // Loop over x in any other angle case
        for (int x=Xmin; x<=Xmax; x++) {
            int y = lround(lpar[0]*x+lpar[1]);
            long npix = y*in->DimX()+x; 
            bool isOK = y>=Ymin && y<=Ymax && !isNaN<T>(Vemap[npix]);
            if (!isOK) continue;
            double r = sqrt(pow(double(x-xcentre),2.)+pow(double(y-ycentre),2.));
            if (r>axis_r_l && x<=xcentre) {
                axis_r_l = r;
                coords_up[0] = x; coords_up[1] = y;
            }
            if (r>axis_r_r && x>xcentre) {
                axis_r_r = r;
                coords_low[0] = x; coords_low[1] = y;
            }
        }
    }
        
    return (axis_r_r+axis_r_l)/2.;
    
}


template <class T>
void ParamGuess<T>::setAxesLine(T xcen, T ycen, T pa) {

    float m = pa - 90;
    while (m>180) m-=180;
    while (m<0) m+=180;
    pmaj[0] = tan(m*M_PI/180.);
    pmaj[1] = ycen-pmaj[0]*xcen;
    pmin[0] = - 1/pmaj[0];
    pmin[1] = ycen-pmin[0]*xcen;
    
}


template <class T>
bool ParamGuess<T>::fitSimplex(int ndim, T **p) {
    
    const int NMAX=5000;            
    const double TINY=1.0e-10;
    const double tol =1.E-03;
    
    int mpts=ndim+1;    
    T minimum;
    
    T psum[ndim], x[ndim];
    T *y = new T[mpts];
    
    for (int i=0; i<mpts; i++) {
        for (int j=0; j<ndim; j++) x[j]=p[i][j];
        y[i]=(this->*func)(x); 
    }
    
    int nfunc=0;
    for (int j=0; j<ndim; j++) {
        T sum=0.0;
        for (int i=0; i<mpts; i++) sum += p[i][j];
        psum[j]=sum;
    }
    
    bool Ok = false;
    for (;;) {
        int ihi, inhi;
        int ilo=0;
        ihi = y[0]>y[1] ? (inhi=1,0) : (inhi=0,1);
        for (int i=0; i<mpts; i++) {
            if (y[i]<=y[ilo]) ilo=i;
            if (y[i]>y[ihi]) {
                inhi=ihi;
                ihi=i;
            } 
            else if (y[i]>y[inhi] && i!=ihi) inhi=i;
        }
        
        double rtol=2.0*fabs(y[ihi]-y[ilo])/(fabs(y[ihi])+fabs(y[ilo])+TINY);
        
        if (rtol<tol) {     
            std::swap(y[0],y[ilo]);
            for (int i=0; i<ndim; i++) {
                std::swap(p[0][i],p[ilo][i]);
            }
            minimum=y[0];
            delete [] y;
            Ok = true;
            break;
        }
        
        if (nfunc>=NMAX) {
            delete [] y;
            Ok = false;
            break;
        }
        nfunc += 2;
        
        double fac=-1.0;
        double fac1=(1.0-fac)/ndim;
        double fac2=fac1-fac;
        T ptry[ndim];
        for (int j=0; j<ndim; j++) ptry[j]=psum[j]*fac1-p[ihi][j]*fac2;
        T ytry=(this->*func)(ptry);         
        if (ytry<y[ihi]) {  
            y[ihi]=ytry;
            for (int j=0; j<ndim; j++) {
                psum[j] += ptry[j]-p[ihi][j];
                p[ihi][j]=ptry[j];
            }
        }
        
        
        if (ytry<=y[ilo]) {
            fac=2.0;
            fac1=(1.0-fac)/ndim;
            fac2=fac1-fac;
            for (int j=0; j<ndim; j++) ptry[j]=psum[j]*fac1-p[ihi][j]*fac2;
            ytry=(this->*func)(ptry);           
            if (ytry<y[ihi]) {  
                y[ihi]=ytry;
                for (int j=0; j<ndim; j++) {
                    psum[j] += ptry[j]-p[ihi][j];
                    p[ihi][j]=ptry[j];
                }
            }
        }   
        else if (ytry>=y[inhi]) {
            T ysave=y[ihi];
            fac=0.5;
            fac1=(1.0-fac)/ndim;
            fac2=fac1-fac;
            for (int j=0; j<ndim; j++) ptry[j]=psum[j]*fac1-p[ihi][j]*fac2;
            ytry=(this->*func)(ptry);           
            if (ytry<y[ihi]) {  
                y[ihi]=ytry;
                for (int j=0; j<ndim; j++) {
                    psum[j] += ptry[j]-p[ihi][j];
                    p[ihi][j]=ptry[j];
                }
            }
            if (ytry>=ysave) {      
                for (int i=0; i<mpts; i++) {
                    if (i!=ilo) {
                        for (int j=0; j<ndim; j++)
                            p[i][j]=psum[j]=0.5*(p[i][j]+p[ilo][j]);
                        y[i]=(this->*func)(psum);
                    }
                }
                
                nfunc += ndim;  
                
                for (int j=0;j<ndim;j++) {
                    T sum=0.0;
                    for (int i=0; i<mpts; i++) sum += p[i][j];
                    psum[j]=sum;
                }
            }
        } 
        else --nfunc; 
    }
    
    return Ok;
}


template <class T>
T ParamGuess<T>::funcEllipse(T *mypar) {
    
    double F = M_PI/180.;
    
    T R   = mypar[0]/(in->Head().PixScale()*arcsconv(in->Head().Cunit(0)));
    T inc = mypar[1];
    T phi = posang;//mypar[2];
    T x0  = xcentre;//mypar[3];
    T y0  = ycentre;//mypar[4];

    double func = 0;
    for (int x=0; x<in->DimX(); x++) {
        for (int y=0; y<in->DimY(); y++) {
            T xr =  -(x-x0)*sin(F*phi)+(y-y0)*cos(F*phi);
            T yr = (-(x-x0)*cos(F*phi)-(y-y0)*sin(F*phi))/cos(F*inc);
            T r = sqrt(xr*xr+yr*yr);
            bool isIn = r<=R;
            if (!isIn) continue;
            if (isNaN(Vemap[x+y*in->DimX()])) func++;
            else func--;
        }
    }
    
    return func;

}


template <class T> 
T ParamGuess<T>::funcIncfromMap(T *mypar) {
        
    /*
        if (mypar[0]<0) mypar[0]=1;
        if (mypar[0]>90) mypar[0]=89;
        
        T inc = mypar[0];
                
        Rings<T> *rings = new Rings<T>;     
        rings->radsep=radsep;
        rings->nr = int(Rmax/radsep);
        
        T *prof = new T[rings->nr];
        int *counter = new int[rings->nr];

        cout << mypar[0] << " " << radsep << " " << rings->nr << " " << xcentre << " "<< ycentre << " " << posang <<  endl;
        
        for (int i=0; i<rings->nr; i++) {
            rings->radii.push_back(i*rings->radsep);
            float r1 = rings->radii[i]/(in->Head().PixScale()*arcsconv(in->Head().Cunit(0)));
            float r2 = (i+1)*rings->radsep/(in->Head().PixScale()*arcsconv(in->Head().Cunit(0)));
            prof[i]=counter[i]=0.;
        }

        for (int x=0; x<in->DimX();x++) {
            for (int y=0; y<in->DimY(); y++) {
                float xr = -(x-xcentre)*sin(posang*M_PI/360.)+(y-ycentre)*cos(posang*M_PI/360.);
                float yr = (-(x-xcentre)*cos(posang*M_PI/360.)-(y-ycentre)*sin(posang*M_PI/360.))/cos(inc*M_PI/360.);
                float rad = sqrt(xr*xr+yr*yr);
                for (int i=0; i<rings->nr; i++) {
                    float r1 = rings->radii[i]/(in->Head().PixScale()*arcsconv(in->Head().Cunit(0)));
                    float r2 = (i+1)*rings->radsep/(in->Head().PixScale()*arcsconv(in->Head().Cunit(0)));
                    if (rad>=r1 && rad<r2) {
                        //cout << Intmap[x+y*in->DimX()] << endl;
                        prof[i]+=fabs(Intmap[x+y*in->DimX()]);
                        counter[i]++;
                    }
                }
            }
        }       

        for (int i=0; i<rings->nr; i++) prof[i]/=float(counter[i]);
        
        float minval = *min_element(&prof[0],&prof[0]+rings->nr);
        float mulfac = pow(10,-int(log10(minval)));

        for (int i=0; i<rings->nr; i++) {
            if (counter[i]!=0) {
                cout << setprecision(10);
                cout << prof[i] << " " << std::flush;
                prof[i]*= mulfac;
                cout << prof[i] << " " << counter[i] <<endl;
            }
            rings->vrot.push_back(AlltoVel(10*in->Head().Cdelt(2),in->Head()));
            rings->vdisp.push_back(8);
            rings->z0.push_back(0);
            rings->dens.push_back(prof[i]*1E20);
            rings->inc.push_back(inc);
            rings->phi.push_back(posang);
            rings->xpos.push_back(xcentre);
            rings->ypos.push_back(ycentre);
            rings->vsys.push_back(vsystem);
        }

        
        Model::Galmod<T> *mod = new Model::Galmod<T>;

        mod->input(in,rings,1);
        mod->calculate();
        mod->smooth();

        delete rings;
        delete [] prof;
        
        T *map_mod = new T[in->DimX()*in->DimY()];
        
        float totflux_mod=0;
        for (int x=0; x<in->DimX(); x++){
            for (int y=0; y<in->DimY(); y++){
                map_mod[x+y*in->DimX()]=0;
                for (int z=0; z<in->DimZ(); z++)
                    map_mod[x+y*in->DimX()]+=mod->Out()->Array(x,y,z);
                totflux_mod += map_mod[x+y*in->DimX()];
            }           
        }
        
                
        float factor = totflux_obs/totflux_mod;

        float res_sum=0;
        for (int i=0; i<in->DimX()*in->DimY();i++) {
            if (map_mod[i]!=0) res_sum += fabs(Intmap[i]-map_mod[i]*factor);    
        }

        return res_sum;
        */

    bool verbosity = in->pars().isVerbose();
    in->pars().setVerbosity(false);


    if (mypar[0]<0) mypar[0]=2*radsep;
    if (mypar[0]>1.5*Rmax) mypar[0]=Rmax;
    if (mypar[1]<0) mypar[1]=1;
    if (mypar[1]>90) mypar[1]=89;
    
    T RMAX = mypar[0];
    T inc  = mypar[1];

    Rings<T> *rings = new Rings<T>;
    rings->radsep=radsep/2.;
    rings->nr = int(RMAX/rings->radsep);

    cout << mypar[0] << " " << mypar[1]<<  "  " << radsep << " " << rings->nr << " " << xcentre << " "<< ycentre << " " << posang <<  endl;

    for (int i=0; i<rings->nr; i++) {
        rings->radii.push_back(i*rings->radsep+rings->radsep/2.);
        rings->vrot.push_back(AlltoVel(10*in->Head().Cdelt(2),in->Head()));
        rings->vdisp.push_back(5);
        rings->z0.push_back(0);
        rings->inc.push_back(inc);
        rings->phi.push_back(posang);
        rings->xpos.push_back(xcentre);
        rings->ypos.push_back(ycentre);
        rings->vsys.push_back(vsystem);
        rings->dens.push_back(1E20);
    }

    MomentMap<T> *totalmap = new MomentMap<T>;
    totalmap->input(in);
    totalmap->SumMap(true);
    for (int i=0; i<totalmap->NumPix();i++) totalmap->Array()[i] = Intmap[i];
    Tasks::Ellprof<T> ell(totalmap,rings);
    ell.RadialProfile();
    delete totalmap;

    double profmin=FLT_MAX;
    for (int i=0; i<rings->nr; i++) {
        double mean = ell.getMean(i);
        if (!isNaN(mean) && profmin>mean && mean>0) profmin = mean;
    }
    float factor = 1;
    while(profmin<0.1) {
        profmin*=10;
        factor *=10;
    }
    while (profmin>10) {
        profmin /= 10;
        factor /= 10;
    }
    for (int i=0; i<rings->nr; i++) {
        rings->dens[i]=factor*fabs(ell.getMean(i))*1E20;
        if (rings->dens[i]==0) rings->dens[i]=profmin*1E20;
    }

    Model::Galmod<T> *mod = new Model::Galmod<T>;
    mod->input(in,rings);
    mod->calculate();
    mod->smooth();

    delete rings;

    T *map_mod = new T[in->DimX()*in->DimY()];

    float totflux_mod=0;
    for (int x=0; x<in->DimX(); x++){
        for (int y=0; y<in->DimY(); y++){
            map_mod[x+y*in->DimX()]=0;
            for (int z=0; z<in->DimZ(); z++)
                map_mod[x+y*in->DimX()]+=mod->Out()->Array(x,y,z);
            totflux_mod += map_mod[x+y*in->DimX()];
        }
    }

    factor = totflux_obs/totflux_mod;

    float res_sum=0;
    for (int i=0; i<in->DimX()*in->DimY();i++) {
        res_sum += fabs(Intmap[i]-map_mod[i]*factor);
    }

    delete [] map_mod;

    in->pars().setVerbosity(verbosity);

    return res_sum;
}


template <class T>
void ParamGuess<T>::plotGuess() {
    

    /// Plotting axes in a .eps file.
    std::ofstream velf;
    std::string outfolder = in->pars().getOutfolder();
    velf.open((outfolder+"vfield.dat").c_str());

    std::vector<T> vec;
    float vrange = fabs(DeltaVel<float>(in->Head()));
    for (int x=0; x<in->DimX(); x++) {
        for (int y=0; y<in->DimY(); y++) {
            long npix = x+y*in->DimX(); 
            if (!isNaN(Vemap[npix])) vec.push_back(Vemap[npix]); 
            velf << x << " " << y << " " << Vemap[npix]<<std::endl;
        }
        velf << std::endl;
    }
    velf.close();
    
    float maxvel = *max_element(&vec[0], &vec[0]+vec.size());
    float minvel = *min_element(&vec[0], &vec[0]+vec.size());
    vec.clear();
    
    std::string outfile = outfolder+"initial_geometry.eps";
    std::ofstream gnu((outfolder+"gnuscript.gnu").c_str());
    T Rmaxpix = Rmax/(in->Head().PixScale()*arcsconv(in->Head().Cunit(0)));
    std::string amaj = to_string(Rmaxpix);
    std::string amin = to_string(Rmaxpix*cos(inclin/180*M_PI));
    std::string posa = to_string(posang/180*M_PI-M_PI_2);
    std::string xcenter = to_string(xcentre);
    std::string ycenter = to_string(ycentre); 
    gnu << "unset key\n"
    //  << "set grid\n"
        << "set title 'Axis fitting'\n"
        << "set cbtics scale 0\n"
        << "set palette defined (0 '#000090',1 '#000fff',2 '#0090ff',3 '#0fffee',4 '#90ff70',"
        << " 5 '#ffee00', 6 '#ff7000',7 '#ee0000',8 '#7f0000')\n"
        << "f(x)="<<to_string(pmaj[0])<<"*x+"<<to_string(pmaj[1])<<std::endl
        << "g(x)="<<to_string(pmin[0])<<"*x+"<<to_string(pmin[1])<<std::endl
        << "set xrange [0:"<<to_string<long>(in->DimX())<<"]\n"
        << "set yrange [0:"<<to_string<long>(in->DimY())<<"]\n"
        << "set cbrange ["<<to_string(minvel)<<":"<<to_string(maxvel)<<"]\n"
        << "set xlabel 'X (pixels)'\n"
        << "set ylabel 'Y (pixels)'\n"
        << "set size square\n"
        << "set parametric\n"
        << "x(t)="+xcenter+"+"+amaj+"*cos("+posa+")*cos(t)-"+amin+"*sin("+posa+")*sin(t)\n"
        << "y(t)="+ycenter+"+"+amaj+"*sin("+posa+")*cos(t)+"+amin+"*cos("+posa+")*sin(t)\n"
        << "set table '"+outfolder+"ellipse.tab'\n"
        << "plot x(t), y(t)\n"
        << "unset table\n"
        << "unset parametric\n"
        << "set terminal postscript eps enhanced color font 'Helvetica,14'\n"
        << "set output '"<<outfile<<"'\n"
        << "plot '"+outfolder+"vfield.dat' w image pixels,"
        << " '"+outfolder+"ellipse.tab' w l ls -1 lw 2, f(x) ls 1 lw 2, g(x) ls 3 lw 2,'-' ls 5, '-' ls 7 \n"
        << to_string(xcentre)+" "+to_string(ycentre) << std::endl
        << "e" << std::endl
        << to_string(major_max[0])+" "+to_string(major_max[1]) << std::endl
        << to_string(major_min[0])+" "+to_string(major_min[1]) << std::endl
        << to_string(minor_max[0])+" "+to_string(minor_max[1]) << std::endl
        << to_string(minor_min[0])+" "+to_string(minor_min[1]) << std::endl
        << "e" << std::endl;
    
#ifdef HAVE_GNUPLOT
    Gnuplot gp;
    gp.begin(); 
    gp.commandln(("load '"+outfolder+"gnuscript.gnu'").c_str());
    gp.end();
    remove((outfolder+"ellipse.tab").c_str());
#endif  
    remove((outfolder+"vfield.dat").c_str());
    remove((outfolder+"gnuscript.gnu").c_str());

}


// Explicit instantiation of the class
template class ParamGuess<float>;
template class ParamGuess<double>;
