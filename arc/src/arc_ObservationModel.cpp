/*********************************************************************************
 *  ARC-SRTK - Single Frequency RTK Pisitioning Library
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *  Created on: July 08, 2017
 *      Author: SuJingLan
 *********************************************************************************/

/**
 * @class ARC_ObservationMoel
 * @file arc_Obsservation.cpp
 * @brief SRTK Observation Model source file
 * @author SuJingLan
 */
#include <arc.h>
#include "arc_ObservationModel.h"

namespace ARC {
    ARC_ObservationModel::ARC_ObservationModel() :
            libPF::ObservationModel<ARC_States>() {
    }

    ARC_ObservationModel::~ARC_ObservationModel() {
    }
    void ARC_ObservationModel::setTrueCarState(const ARC_States& state)
    {
        m_TrueArcState = state;
    }
    double ARC_ObservationModel::measure(const ARC_States &state) const
    {
        
    }
    int ARC_ObservationModel::ComputeZd()
    {
        double r,rr_[3],pos[3],dant[NFREQ]={0},disp[3],*y,
                *e,rb[3]={0.0},*rs,*azel,*dts;
        double *yp,*rsp,*dtsp,*ep,*azelp;
        double zhd,zazel[]={0.0,90.0*D2R};
        int n=Nu+Nb,svh[MAXSAT*2],*svhp,N=0,index;
        ARC_OBSD *Obsp;

        y=mat(2,n); e=mat(3,n); rs=mat(6,n);
        azel=zeros(2,n); dts=mat(2,n);
        
        for (int i=0;i<n;i++) {
            for (int j=0;j<6;j++) {
                rs[6*i+j]=m_SatPos[SatList[i]*6+j];
            }
            for (int j=0;j<2;j++) dts[i*2+j]=m_SatClk[SatList[i]*2+j];
            svh[i]=m_SVH[SatList[i]];
        }
        for (int k=0;k<2;k++) {
            if (k==0) {
                yp=y+2*Nu;
                rsp=rs+6*Nu; dtsp=dts+2*Nu;
                svhp=svh+Nu; Obsp=m_Obs+Nu; N=Nb;
                ep=e+3*Nu; azelp=azel+2*Nu;
                rr_[0]=m_TrueArcState.getBasePosX();
                rr_[1]=m_TrueArcState.getBasePosY();
                rr_[2]=m_TrueArcState.getBasePosZ();
                index=1;
            }
            else {
                yp=y; rsp=rs; dtsp=dts;
                svhp=svh; Obsp=m_Obs; N=Nu;
                ep=e; azelp=azel;
                rr_[0]=m_TrueArcState.getRoverPosX();
                rr_[1]=m_TrueArcState.getRoverPosY();
                rr_[2]=m_TrueArcState.getRoverPosZ();
                index=0;
            }
            if (norm(rr_,3)<=0.0) continue;
            if (m_Opt.tidecorr) {
                tidedisp(gpst2utc(Obsp[0].time),rr_,m_Opt.tidecorr,&m_Nav.erp,
                         m_Opt.odisp[1],disp);
                for (int i=0;i<3;i++) rr_[i]+=disp[i];
            }
            ecef2pos(rr_,pos);
            for (int i=0;i<N;i++) {
                if ((r=geodist(rsp+i*6,rr_,e+i*3))<=0.0) continue;
                if (satazel(pos,e+i*3,azel+i*2)<m_Opt.elmin) continue;

                if (satexclude(Obsp[i].sat,svhp[i],&m_Opt)) continue;

                r+=-CLIGHT*dtsp[i*2];

                zhd=tropmodel(Obsp[0].time,pos,zazel,0.0);
                r+=tropmapf(Obsp[i].time,pos,azel+i*2,NULL)*zhd;

                antmodel(m_Opt.pcvr+index,m_Opt.antdel[index],azel+i*2,m_Opt.posopt[1],dant);
                const double *lam=m_Nav.lam[Obsp[i].sat-1];

                if (lam[0]==0.0) continue;

                if (testsnr(index,0,azel[i*2+1],Obsp[i].SNR[i]*0.25,&m_Opt.snrmask)) {
                    continue;
                }
                if (Obsp[i].L[0]!=0.0) yp[i*2  ]=Obsp[i].L[0]*lam[0]-r-dant[0];
                if (Obsp[i].P[0]!=0.0) yp[i*2+1]=Obsp[i].P[0]       -r-dant[0];
            }
        }
        if (m_Opt.intpref) {
            IntPres(m_Obs[0].time,m_Obs+Nu,Nb,&m_Nav,y+Nu*2);
        }
        for (int i=0;i<n;i++) {
            if (norm(y+2*i,2)==0.0) continue;
            for (int j=0;j<2;j++) m_ZDRes[SatList[i]*i+j]=y[i*2+j];
        }
        return 1;
    }
    void ARC_ObservationModel::ComputeSatPos()
    {
        double *SatPos=mat(6,Nu+Nb),*SatClk=mat(2,Nu+Nb),*Var=mat(1,Nu+Nb);
        int Svh[MAXSAT*2];
        satposs(m_RoverTime,m_Obs,Nu+Nb,&m_Nav,m_Opt.sateph,SatPos,SatClk,Var,Svh);
        for (int i=0;i<Nu+Nb;i++) {
            if (SatList[i]) {
                for (int j=0;j<6;i++) {
                    m_SatPos[(SatList[i]-1)*6+j]=SatPos[i*6+j];
                }
                for (int j=0;j<2;j++) {
                    m_SatClk[(SatList[i]-1)*2+j]=SatClk[i*2+j];
                }
                m_SatPosVar[SatList[i]-1]=Var[i];
                m_SVH[SatList[i]-1]=Svh[i];
            }
        }
        delete SatPos; delete SatClk; delete Var;
    }
    double ARC_ObservationModel::IntPres(ARC_Time time, const ARC_OBSD *obs, int n,
                                         const ARC_NAV *nav, double *y)
    {
        static obsd_t obsb[MAXOBS];
        static double yb[MAXOBS*NFREQ*2],rs[MAXOBS*6],dts[MAXOBS*2],var[MAXOBS];
        static double e[MAXOBS*3],azel[MAXOBS*2];
        static int nb=0,svh[MAXOBS*2];
        double tt=timediff(time,obs[0].time),ttb,*p,*q,rb[3];
        int i,j,k,nf=1;

        if (nb==0||fabs(tt)<DTTOL) {
            nb=n; for (i=0;i<n;i++) obsb[i]=obs[i];
            return tt;
        }
        ttb=timediff(time,obsb[0].time);
        if (fabs(ttb)>m_Opt.maxtdiff*2.0||ttb==tt) return tt;

        satposs(time,obsb,nb,nav,m_Opt.sateph,rs,dts,var,svh);
        rb[0]=m_TrueArcState.getBasePosX();
        rb[1]=m_TrueArcState.getBasePosY();
        rb[2]=m_TrueArcState.getBasePosZ();

        if (!zdres(1,obsb,nb,rs,dts,svh,nav,rb,&m_Opt,1,yb,e,azel)) {
            return tt;
        }
        for (i=0;i<n;i++) {
            for (j=0;j<nb;j++) if (obsb[j].sat==obs[i].sat) break;
            if (j>=nb) continue;
            for (k=0,p=y+i*nf*2,q=yb+j*nf*2;k<nf*2;k++,p++,q++) {
                if (*p==0.0||*q==0.0) *p=0.0; else *p=(ttb*(*p)-tt*(*q))/(ttb-tt);
            }
        }
        return fabs(ttb)>fabs(tt)?ttb:tt;
    }
    int ARC_ObservationModel::SelectCommonSat(const ARC_OBSD *obs, double *azel, int nu, int nr,
                                              const ARC_OPT *opt, int *sat, int *iu,int *ir)
    {
        int i,j,k=0;
        for (i=0,j=nu;i<nu&&j<nu+nr;i++,j++) {
            if      (obs[i].sat<obs[j].sat) j--;
            else if (obs[i].sat>obs[j].sat) i--;
            else if (azel[1+j*2]>=opt->elmin) { 
                sat[k]=obs[i].sat; iu[k]=i; ir[k++]=j;
            }
        }
        return k;
    }
}

