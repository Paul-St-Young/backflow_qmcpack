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
//
// Supported by
//   National Center for Supercomputing Applications, UIUC
//   Materials Computation Center, UIUC
//////////////////////////////////////////////////////////////////
// -*- C++ -*-
#ifndef QMCPLUSPLUS_RMCLOCALENERGYESTIMATOR_H
#define QMCPLUSPLUS_RMCLOCALENERGYESTIMATOR_H
#include "Estimators/ScalarEstimatorBase.h"
#include "QMCHamiltonians/QMCHamiltonian.h"
#include "Particle/Reptile.h"

namespace qmcplusplus
{

/** Class to accumulate the local energy and components
 *
 * Use Walker::Properties to accumulate Hamiltonian-related quantities.
 */
class RMCLocalEnergyEstimator: public ScalarEstimatorBase
{


  int FirstHamiltonian;
  int SizeOfHamiltonians;
  int NObs;
  const QMCHamiltonian& refH;

public:

  /** constructor
   * @param h QMCHamiltonian to define the components
   */
  RMCLocalEnergyEstimator(QMCHamiltonian& h, int nobs=2);

  /** accumulation per walker
   * @param awalker current walker
   * @param wgt weight
   *
   * Weight of observables should take into account the walkers weight. For Pure DMC. In branching DMC set weights to 1.
   */
  inline void accumulate(const Walker_t& awalker, RealType wgt)
  {
  }

  /*@{*/
  inline void accumulate(const MCWalkerConfiguration& W
                         , WalkerIterator first, WalkerIterator last, RealType wgt)
  {
    //WalkerIterator tail=first+W.activeBead+W.direction;
    //WalkerIterator head=first+W.activeBead;
    //if(tail>=last)
    //  tail-=last-first;
    //else if(tail<first)
    //  tail+=last-first;
    Walker_t& head = W.reptile->getHead();
    Walker_t& tail = W.reptile->getTail();
//       mixed estimator stuff
    const RealType* restrict ePtr = head.getPropertyBase();
    //   RealType wwght=  head.Weight;
    RealType wwght=0.5;
    //app_log()<<"~~~~~For head:  Energy:"<<ePtr[LOCALENERGY]<<endl;
    scalars[0](ePtr[LOCALENERGY],wwght);
    scalars[1](ePtr[LOCALENERGY]*ePtr[LOCALENERGY],wwght);
    scalars[2](ePtr[LOCALPOTENTIAL],wwght);
    //for(int target=4, source=FirstHamiltonian; source<FirstHamiltonian+SizeOfHamiltonians;
    //   ++target, ++source)
    //scalars[target](ePtr[source],wwght);
    const RealType* restrict lPtr = tail.getPropertyBase();
    //  wwght=  tail.Weight;
    scalars[0](lPtr[LOCALENERGY],wwght);
    scalars[1](lPtr[LOCALENERGY]*lPtr[LOCALENERGY],wwght);
    scalars[2](lPtr[LOCALPOTENTIAL],wwght);
    for(int target=4, source=FirstHamiltonian; source<FirstHamiltonian+SizeOfHamiltonians; ++target, ++source)
    {
      wwght=0.5;
      scalars[target](lPtr[source],wwght);
      scalars[target](ePtr[source],wwght);
    }
    for(int target=4+SizeOfHamiltonians, source=FirstHamiltonian; source<FirstHamiltonian+SizeOfHamiltonians; ++target, ++source)
    {
      wwght=1;
      const RealType* restrict cPtr = (W.reptile->getCenter()).getPropertyBase();
      scalars[target](cPtr[source],wwght);
    }
    //     scalars[target](lPtr[source],wwght);
    //    int stride(0);
    //    int bds(last-first);
    //    if(bds%2>0)//odd
    //    {
    //      int odd=(bds+1)/2;
    //      stride=odd/NObs;
    //    }
    //   else
    //   {
    //     int odd=bds/2;
    //     stride=odd/NObs;
    //   }
    //
    //   int indx(4+SizeOfHamiltonians);
    //   for(int j=0; j < NObs; j++)
    //  {
    //    tail+= -W.direction*stride;
    //    head+=  W.direction*stride;
    //   if(tail>=last)
    //     tail-=last-first;
    //   else if(tail<first)
    //     tail+=last-first;
    //   if(head>=last)
    //     head-=last-first;
    //   else if(head<first)
    //    head+=last-first;
    //     const RealType* lPtr = tail.getPropertyBase();
    // const RealType* ePtr = head.getPropertyBase();
    // for(int target=0, source=FirstHamiltonian; target<SizeOfHamiltonians; ++target, ++source, indx++)
    //   {
    //    scalars[indx](lPtr[source]);
    // scalars[indx](ePtr[source]);
    //   }
//     }
    //   int maxage(0);
//     while(first!=last)
    //   {
    //     maxage=max(maxage,(*first)->Age);
    //      first++;
    //   }
    //  scalars[3](maxage);
//       for(; first != last; ++first) accumulate(**first,wgt);*/
  }

  void add2Record(RecordListType& record);
  void registerObservables(vector<observable_helper*>& h5dec, hid_t gid) {}
  ScalarEstimatorBase* clone();
  /*@}*/
};
}
#endif
/***************************************************************************
 * $RCSfile$   $Author: jmcminis $
 * $Revision: 4165 $   $Date: 2009-08-31 05:47:46 -0500 (Mon, 31 Aug 2009) $
 * $Id: RMCLocalEnergyEstimator.h 4165 2009-08-31 10:47:46Z jmcminis $
 ***************************************************************************/
