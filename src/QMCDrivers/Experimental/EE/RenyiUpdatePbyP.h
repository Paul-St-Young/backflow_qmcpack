//////////////////////////////////////////////////////////////////
// (c) Copyright 2003- by Jeongnim Kim
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//   Jeongnim Kim
//   National Center for Supercomputing Applications &
//   Materials Computation Center
//   University of Illinois, Urbana-Champaign
//   Urbana, IL 61801
//   e-mail: jnkim@ncsa.uiuc.edu
//   Tel:    217-244-6319 (NCSA) 217-333-3324 (MCC)
//
// Supported by
//   National Center for Supercomputing Applications, UIUC
//   Materials Computation Center, UIUC
//////////////////////////////////////////////////////////////////
// -*- C++ -*-
#ifndef QMCPLUSPLUS_RENYI_PARTICLEBYPARTICLE_UPDATE_H
#define QMCPLUSPLUS_RENYI_PARTICLEBYPARTICLE_UPDATE_H
#include "QMCDrivers/EE/QMCRenyiUpdateBase.h"

namespace qmcplusplus
{

class RenyiUpdatePbyP: public QMCRenyiUpdateBase
{
public:
  /// Constructor.
  RenyiUpdatePbyP(MCWalkerConfiguration& w, TrialWaveFunction& psi,
                  QMCHamiltonian& h, RandomGenerator_t& rg, int order);

  ~RenyiUpdatePbyP();

  void advanceWalkers(WalkerIter_t it, WalkerIter_t it_end, bool measure);
  void plotSwapAmplitude(WalkerIter_t it, WalkerIter_t it_end,Matrix<RealType>& averageSwaps);

private:
};

//   class RenyiUpdateAll: public QMCRenyiUpdateBase {
//   public:
//     /// Constructor.
//     RenyiUpdateAll(MCWalkerConfiguration& w, TrialWaveFunction& psi,
//         QMCHamiltonian& h, RandomGenerator_t& rg, int order);
//
//     ~RenyiUpdateAll();
//
//     void advanceWalkers(WalkerIter_t it, WalkerIter_t it_end, bool measure);
//
// //     RealType advanceWalkerForEE(Walker_t& w1, vector<PosType>& dR, vector<int>& iats, vector<int>& rs, vector<RealType>& ratios);
//
//   private:
//   };
//
//   class RenyiUpdateAllWithDrift: public QMCRenyiUpdateBase {
//   public:
//     /// Constructor.
//     RenyiUpdateAllWithDrift(MCWalkerConfiguration& w, TrialWaveFunction& psi,
//         QMCHamiltonian& h, RandomGenerator_t& rg, int order);
//
//     ~RenyiUpdateAllWithDrift();
//
//     void advanceWalkers(WalkerIter_t it, WalkerIter_t it_end, bool measure);
//
// //     RealType advanceWalkerForEE(Walker_t& w1, vector<PosType>& dR, vector<int>& iats, vector<int>& rs, vector<RealType>& ratios);
//
//   private:
//     std::vector<ParticleSet::ParticlePos_t*> drifts;
//   };

//   class VMCUpdatePbyPWithDrift: public QMCUpdateBase {
//   public:
//     /// Constructor.
//     VMCUpdatePbyPWithDrift(MCWalkerConfiguration& w, TrialWaveFunction& psi,
//         QMCHamiltonian& h, RandomGenerator_t& rg);
//
//     ~VMCUpdatePbyPWithDrift();
//
//     void advanceWalkers(WalkerIter_t it, WalkerIter_t it_end, bool measure);
//
//   private:
//     vector<NewTimer*> myTimers;
//   };

//   class VMCUpdatePbyPWithDriftFast: public QMCUpdateBase {
//   public:
//     /// Constructor.
//     VMCUpdatePbyPWithDriftFast(MCWalkerConfiguration& w, TrialWaveFunction& psi,
//         QMCHamiltonian& h, RandomGenerator_t& rg);
//
//     ~VMCUpdatePbyPWithDriftFast();
//
//     void advanceWalkers(WalkerIter_t it, WalkerIter_t it_end, bool measure);
// //     for linear opt CS
//     void advanceCSWalkers(vector<TrialWaveFunction*>& pclone, vector<MCWalkerConfiguration*>& wclone, vector<QMCHamiltonian*>& hclone, vector<RandomGenerator_t*>& rng, vector<RealType>& c_i);
//     RealType advanceWalkerForEE(Walker_t& w1, vector<PosType>& dR, vector<int>& iats, vector<int>& rs, vector<RealType>& ratios);
//     RealType advanceWalkerForCSEE(Walker_t& w1, vector<PosType>& dR, vector<int>& iats, vector<int>& rs, vector<RealType>& ratios, vector<RealType>& weights, vector<RealType>& logs );
//   private:
//     vector<NewTimer*> myTimers;
//   };
//
//     /** @ingroup QMCDrivers  ParticleByParticle
//    *@brief Implements the VMC algorithm using particle-by-particle move with the drift equation.
//    */
//   class VMCUpdateRenyiWithDriftFast: public QMCUpdateBase {
//   public:
//     /// Constructor.
//     VMCUpdateRenyiWithDriftFast(MCWalkerConfiguration& w, TrialWaveFunction& psi,
//         QMCHamiltonian& h, RandomGenerator_t& rg);
//
//     ~VMCUpdateRenyiWithDriftFast();
//
//     void advanceWalkers(WalkerIter_t it, WalkerIter_t it_end, bool measure);
//
//   private:
//     vector<NewTimer*> myTimers;
//   };


//   class VMCUpdatePbyPSampleRN: public QMCUpdateBase {
//   public:
//     /// Constructor.
//     VMCUpdatePbyPSampleRN(MCWalkerConfiguration& w, TrialWaveFunction& psi, TrialWaveFunction& guide,
//         QMCHamiltonian& h, RandomGenerator_t& rg);
//
//     ~VMCUpdatePbyPSampleRN();
//
//     void advanceWalkers(WalkerIter_t it, WalkerIter_t it_end, bool measure);
//     void advanceCSWalkers(vector<TrialWaveFunction*>& pclone, vector<MCWalkerConfiguration*>& wclone, vector<QMCHamiltonian*>& hclone, vector<RandomGenerator_t*>& rng, vector<RealType>& c_i);
//     void setLogEpsilon(RealType eps)
//     {
//       logEpsilon=eps;
// //       app_log()<<eps<<endl;
//     }
//     void initWalkersForPbyP(WalkerIter_t it, WalkerIter_t it_end);
//
//     void estimateNormWalkers(vector<TrialWaveFunction*>& pclone
//     , vector<MCWalkerConfiguration*>& wclone
//     , vector<QMCHamiltonian*>& hclone
//     , vector<RandomGenerator_t*>& rng
//     , vector<RealType>& ratio_i_0);
//
//   private:
//     vector<NewTimer*> myTimers;
//     //prefactor multiplying the guiding function
//     RealType logEpsilon;
//   };
}

#endif
/***************************************************************************
 * $RCSfile: VMCUpdatePbyP.h,v $   $Author: jnkim $
 * $Revision: 1.5 $   $Date: 2006/07/17 14:29:40 $
 * $Id: VMCUpdatePbyP.h,v 1.5 2006/07/17 14:29:40 jnkim Exp $
 ***************************************************************************/
