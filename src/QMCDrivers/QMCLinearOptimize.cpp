//////////////////////////////////////////////////////////////////
// (c) Copyright 2005- by Jeongnim Kim
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
#include "QMCDrivers/QMCLinearOptimize.h"
#include "Particle/HDFWalkerIO.h"
#include "Particle/DistanceTable.h"
#include "OhmmsData/AttributeSet.h"
#include "Message/CommOperators.h"
//#if defined(ENABLE_OPENMP)
#include "QMCDrivers/VMC/VMCSingleOMP.h"
#include "QMCDrivers/QMCCostFunctionOMP.h"
//#endif
//#include "QMCDrivers/VMC/VMCSingle.h"
//#include "QMCDrivers/QMCCostFunctionSingle.h"
#include "QMCApp/HamiltonianPool.h"
#include "Numerics/Blasf.h"
#include "Numerics/MatrixOperators.h"
#include <cassert>
#if defined(QMC_CUDA)
#include "QMCDrivers/VMC/VMC_CUDA.h"
#include "QMCDrivers/QMCCostFunctionCUDA.h"
#endif
#include "Numerics/LinearFit.h"
#include "qmc_common.h"
#include <iostream>
#include <fstream>

/*#include "Message/Communicate.h"*/

namespace qmcplusplus
{

QMCLinearOptimize::QMCLinearOptimize(MCWalkerConfiguration& w,
                                     TrialWaveFunction& psi, QMCHamiltonian& h, HamiltonianPool& hpool, WaveFunctionPool& ppool): QMCDriver(w,psi,h,ppool),
  PartID(0), NumParts(1), WarmupBlocks(10),  hamPool(hpool), optTarget(0), vmcEngine(0),  wfNode(NULL), optNode(NULL), param_tol(1e-4)
{
//     //set the optimization flag
  QMCDriverMode.set(QMC_OPTIMIZE,1);
  //read to use vmc output (just in case)
  m_param.add(param_tol,"alloweddifference","double");
  //Set parameters for line minimization:
  this->add_timers(myTimers);
}

/** Clean up the vector */
QMCLinearOptimize::~QMCLinearOptimize()
{
  if(vmcEngine) delete vmcEngine;
  if(optTarget) delete optTarget;
}

void QMCLinearOptimize::add_timers(vector<NewTimer*>& timers)
{
  timers.push_back(new NewTimer("QMCLinearOptimize::GenerateSamples"));
  timers.push_back(new NewTimer("QMCLinearOptimize::Initialize"));
  timers.push_back(new NewTimer("QMCLinearOptimize::Eigenvalue"));
  timers.push_back(new NewTimer("QMCLinearOptimize::Line_Minimization"));
  for (int i=0; i<timers.size(); ++i)
    TimerManager.addTimer(timers[i]);
}

/** Add configuration files for the optimization
* @param a root of a hdf5 configuration file
*/
void QMCLinearOptimize::addConfiguration(const string& a)
{
  if (a.size())
    ConfigFile.push_back(a);
}

void QMCLinearOptimize::start_optimize()
{
  optTarget->initCommunicator(myComm);
  //close files automatically generated by QMCDriver
//     branchEngine->finalize();
  //generate samples
  myTimers[0]->start();
  generateSamples();
  myTimers[0]->stop();

  app_log() << "<opt stage=\"setup\">" << endl;
  app_log() << "  <log>"<<endl;
  //reset the rootname
  optTarget->setRootName(RootName);
  optTarget->setWaveFunctionNode(wfNode);
  app_log() << "   Reading configurations from h5FileRoot " << h5FileRoot << endl;
  
  //get configuration from the previous run
  Timer t1;
  myTimers[1]->start();
  optTarget->getConfigurations(h5FileRoot);
  optTarget->setRng(vmcEngine->getRng());
  optTarget->checkConfigurations();
  myTimers[1]->stop();
  app_log() << "  Execution time = " << t1.elapsed() << endl;
  app_log() << "  </log>"<<endl;
  app_log() << "</opt>" << endl;
  app_log() << "<opt stage=\"main\" walkers=\""<< optTarget->getNumSamples() << "\">" << endl;
  app_log() << "  <log>" << endl;
  t1.restart();
}

void QMCLinearOptimize::finish_optimize()
{
  MyCounter++;
  app_log() << "  Execution time = " << t1.elapsed() << endl;
  TimerManager.print(myComm);
  app_log() << "  </log>" << endl;
  optTarget->resetWalkers();
  optTarget->reportParameters();
  //int nw_removed=W.getActiveWalkers()-NumOfVMCWalkers;
  //app_log() << "   Restore the number of walkers to " << NumOfVMCWalkers << ", removing " << nw_removed << " walkers." <<endl;
  //W.destroyWalkers(nw_removed);
  app_log() << "</opt>" << endl;
  app_log() << "</optimization-report>" << endl;
}

void QMCLinearOptimize::generateSamples()
{
  app_log() << "<optimization-report>" << endl;
  //reset vmc engine
  vmcEngine->QMCDriverMode.set(QMC_WARMUP,1);
  vmcEngine->QMCDriverMode.set(QMC_OPTIMIZE,1);
  vmcEngine->setValue("current",qmc_common.qmc_counter);
  vmcEngine->setValue("warmupsteps",nWarmupSteps);

  app_log() << "<vmc stage=\"main\" blocks=\"" 
    << nBlocks << "\" iteration=\""<< qmc_common.qmc_counter << "\">" << endl;
  t1.restart();
  //     W.reset();
  branchEngine->flush(0);
  branchEngine->reset();
  vmcEngine->run();
  app_log() << "  Execution time = " << t1.elapsed() << endl;
  app_log() << "</vmc>" << endl;

  //write parameter history and energies to the parameter file in the trial wave function through opttarget
  RealType e,w,var;
  vmcEngine->Estimators->getEnergyAndWeight(e,w,var);
  optTarget->recordParametersToPsi(e,var);
  h5FileRoot=RootName;
}

QMCLinearOptimize::RealType QMCLinearOptimize::getLowestEigenvector(Matrix<RealType>& A, Matrix<RealType>& B, vector<RealType>& ev)
{
  int Nl(ev.size());
  //Tested the single eigenvalue speed and It was no faster.
  //segfault issues with single eigenvalue problem for some machines
  bool singleEV(false);
  if (singleEV)
  {
    Matrix<double> TAU(Nl,Nl);
    int INFO;
    int LWORK(-1);
    vector<RealType> WORK(1);
    //optimal work size
    dgeqrf( &Nl, &Nl, B.data(), &Nl, TAU.data(), &WORK[0], &LWORK, &INFO);
    LWORK=int(WORK[0]);
    WORK.resize(LWORK);
    //QR factorization of S, or H2 matrix. to be applied to H before solve.
    dgeqrf( &Nl, &Nl, B.data(), &Nl, TAU.data(), &WORK[0], &LWORK, &INFO);
    char SIDE('L');
    char TRANS('T');
    LWORK=-1;
    //optimal work size
    dormqr(&SIDE, &TRANS, &Nl, &Nl, &Nl, B.data(), &Nl, TAU.data(), A.data(), &Nl, &WORK[0], &LWORK, &INFO);
    LWORK=int(WORK[0]);
    WORK.resize(LWORK);
    //Apply Q^T to H
    dormqr(&SIDE, &TRANS, &Nl, &Nl, &Nl, B.data(), &Nl, TAU.data(), A.data(), &Nl, &WORK[0], &LWORK, &INFO);
    //now we have a pair (A,B)=(Q^T*H,Q^T*S) where B is upper triangular and A is general matrix.
    //reduce the matrix pair to generalized upper Hesenberg form
    char COMPQ('N'), COMPZ('I');
    int ILO(1);
    int LDQ(Nl);
    Matrix<double> Z(Nl,Nl), Q(Nl,LDQ); //starts as unit matrix
    for (int zi=0; zi<Nl; zi++)
      Z(zi,zi)=1;
    dgghrd(&COMPQ, &COMPZ, &Nl, &ILO, &Nl, A.data(), &Nl, B.data(), &Nl, Q.data(), &LDQ, Z.data(), &Nl, &INFO);
    //Take the pair and reduce to shur form and get eigenvalues
    vector<RealType> alphar(Nl),alphai(Nl),beta(Nl);
    char JOB('S');
    COMPQ='N';
    COMPZ='V';
    LWORK=-1;
    //get optimal work size
    dhgeqz(&JOB, &COMPQ, &COMPZ, &Nl, &ILO, &Nl, A.data(), &Nl, B.data(), &Nl, &alphar[0], &alphai[0], &beta[0], Q.data(), &LDQ, Z.data(), &Nl, &WORK[0], &LWORK, &INFO);
    LWORK=int(WORK[0]);
    WORK.resize(LWORK);
    dhgeqz(&JOB, &COMPQ, &COMPZ, &Nl, &ILO, &Nl, A.data(), &Nl, B.data(), &Nl, &alphar[0], &alphai[0], &beta[0], Q.data(), &LDQ, Z.data(), &Nl, &WORK[0], &LWORK, &INFO);
    //find the best eigenvalue
    vector<std::pair<RealType,int> > mappedEigenvalues(Nl);
    for (int i=0; i<Nl; i++)
    {
      RealType evi(alphar[i]/beta[i]);
      if (abs(evi)<1e10)
      {
        mappedEigenvalues[i].first=evi;
        mappedEigenvalues[i].second=i;
      }
      else
      {
        mappedEigenvalues[i].first=1e100;
        mappedEigenvalues[i].second=i;
      }
    }
    std::sort(mappedEigenvalues.begin(),mappedEigenvalues.end());
    int BestEV(mappedEigenvalues[0].second);
//                   now we rearrange the  the matrices
    if (BestEV!=0)
    {
      bool WANTQ(false);
      bool WANTZ(true);
      int ILST(1);
      int IFST(BestEV+1);
      LWORK=-1;
      dtgexc(&WANTQ, &WANTZ, &Nl, A.data(), &Nl, B.data(), &Nl, Q.data(), &Nl, Z.data(), &Nl, &IFST, &ILST, &WORK[0], &LWORK, &INFO);
      LWORK=int(WORK[0]);
      WORK.resize(LWORK);
      dtgexc(&WANTQ, &WANTZ, &Nl, A.data(), &Nl, B.data(), &Nl, Q.data(), &Nl, Z.data(), &Nl, &IFST, &ILST, &WORK[0], &LWORK, &INFO);
    }
    //now we compute the eigenvector
    SIDE='R';
    char HOWMNY('S');
    int M(0);
    Matrix<double> Z_I(Nl,Nl);
    bool SELECT[Nl];
    for (int zi=0; zi<Nl; zi++)
      SELECT[zi]=false;
    SELECT[0]=true;
    WORK.resize(6*Nl);
    dtgevc(&SIDE, &HOWMNY, &SELECT[0], &Nl, A.data(), &Nl, B.data(), &Nl, Q.data(), &LDQ, Z_I.data(), &Nl, &Nl, &M, &WORK[0], &INFO);
    std::vector<RealType> evec(Nl,0);
    for (int i=0; i<Nl; i++)
      for (int j=0; j<Nl; j++)
        evec[i] += Z(j,i)*Z_I(0,j);
    for (int i=0; i<Nl; i++)
      ev[i] = evec[i]/evec[0];
//     for (int i=0; i<Nl; i++) app_log()<<ev[i]<<" ";
//     app_log()<<endl;
    return mappedEigenvalues[0].first;
  }
  else
  {
// OLD ROUTINE. CALCULATES ALL EIGENVECTORS
//   Getting the optimal worksize
    char jl('N');
    char jr('V');
    vector<RealType> alphar(Nl),alphai(Nl),beta(Nl);
    Matrix<RealType> eigenT(Nl,Nl);
    int info;
    int lwork(-1);
    vector<RealType> work(1);
    RealType tt(0);
    int t(1);
    dggev(&jl, &jr, &Nl, A.data(), &Nl, B.data(), &Nl, &alphar[0], &alphai[0], &beta[0],&tt,&t, eigenT.data(), &Nl, &work[0], &lwork, &info);
    lwork=int(work[0]);
    work.resize(lwork);
    //~ //Get an estimate of E_lin
    //~ Matrix<RealType> H_tmp(HamT);
    //~ Matrix<RealType> S_tmp(ST);
    //~ dggev(&jl, &jr, &Nl, H_tmp.data(), &Nl, S_tmp.data(), &Nl, &alphar[0], &alphai[0], &beta[0],&tt,&t, eigenT.data(), &Nl, &work[0], &lwork, &info);
    //~ RealType E_lin(alphar[0]/beta[0]);
    //~ int e_min_indx(0);
    //~ for (int i=1; i<Nl; i++)
    //~ if (E_lin>(alphar[i]/beta[i]))
    //~ {
    //~ E_lin=alphar[i]/beta[i];
    //~ e_min_indx=i;
    //~ }
    dggev(&jl, &jr, &Nl, A.data(), &Nl, B.data(), &Nl, &alphar[0], &alphai[0], &beta[0],&tt,&t, eigenT.data(), &Nl, &work[0], &lwork, &info);
    if (info!=0)
    {
      APP_ABORT("Invalid Matrix Diagonalization Function!");
    }
    vector<std::pair<RealType,int> > mappedEigenvalues(Nl);
    for (int i=0; i<Nl; i++)
    {
      RealType evi(alphar[i]/beta[i]);
      if (abs(evi)<1e10)
      {
        mappedEigenvalues[i].first=evi;
        mappedEigenvalues[i].second=i;
      }
      else
      {
        mappedEigenvalues[i].first=1e100;
        mappedEigenvalues[i].second=i;
      }
    }
    std::sort(mappedEigenvalues.begin(),mappedEigenvalues.end());
    for (int i=0; i<Nl; i++)
      ev[i] = eigenT(mappedEigenvalues[0].second,i)/eigenT(mappedEigenvalues[0].second,0);
    return mappedEigenvalues[0].first;
  }
}


QMCLinearOptimize::RealType QMCLinearOptimize::getLowestEigenvector(Matrix<RealType>& A, vector<RealType>& ev)
{
  int Nl(ev.size());
  //Tested the single eigenvalue speed and It was no faster.
  //segfault issues with single eigenvalue problem for some machines
  bool singleEV(false);
//     if (singleEV)
//     {
//         Matrix<double> TAU(Nl,Nl);
//         int INFO;
//         int LWORK(-1);
//         vector<RealType> WORK(1);
//         //optimal work size
//         dgeqrf( &Nl, &Nl, B.data(), &Nl, TAU.data(), &WORK[0], &LWORK, &INFO);
//         LWORK=int(WORK[0]);
//         WORK.resize(LWORK);
//         //QR factorization of S, or H2 matrix. to be applied to H before solve.
//         dgeqrf( &Nl, &Nl, B.data(), &Nl, TAU.data(), &WORK[0], &LWORK, &INFO);
//
//         char SIDE('L');
//         char TRANS('T');
//         LWORK=-1;
//         //optimal work size
//         dormqr(&SIDE, &TRANS, &Nl, &Nl, &Nl, B.data(), &Nl, TAU.data(), A.data(), &Nl, &WORK[0], &LWORK, &INFO);
//         LWORK=int(WORK[0]);
//         WORK.resize(LWORK);
//         //Apply Q^T to H
//         dormqr(&SIDE, &TRANS, &Nl, &Nl, &Nl, B.data(), &Nl, TAU.data(), A.data(), &Nl, &WORK[0], &LWORK, &INFO);
//
//         //now we have a pair (A,B)=(Q^T*H,Q^T*S) where B is upper triangular and A is general matrix.
//         //reduce the matrix pair to generalized upper Hesenberg form
//         char COMPQ('N'), COMPZ('I');
//         int ILO(1);
//         int LDQ(Nl);
//         Matrix<double> Z(Nl,Nl), Q(Nl,LDQ); //starts as unit matrix
//         for (int zi=0; zi<Nl; zi++) Z(zi,zi)=1;
//         dgghrd(&COMPQ, &COMPZ, &Nl, &ILO, &Nl, A.data(), &Nl, B.data(), &Nl, Q.data(), &LDQ, Z.data(), &Nl, &INFO);
//
//         //Take the pair and reduce to shur form and get eigenvalues
//         vector<RealType> alphar(Nl),alphai(Nl),beta(Nl);
//         char JOB('S');
//         COMPQ='N';
//         COMPZ='V';
//         LWORK=-1;
//         //get optimal work size
//         dhgeqz(&JOB, &COMPQ, &COMPZ, &Nl, &ILO, &Nl, A.data(), &Nl, B.data(), &Nl, &alphar[0], &alphai[0], &beta[0], Q.data(), &LDQ, Z.data(), &Nl, &WORK[0], &LWORK, &INFO);
//         LWORK=int(WORK[0]);
//         WORK.resize(LWORK);
//         dhgeqz(&JOB, &COMPQ, &COMPZ, &Nl, &ILO, &Nl, A.data(), &Nl, B.data(), &Nl, &alphar[0], &alphai[0], &beta[0], Q.data(), &LDQ, Z.data(), &Nl, &WORK[0], &LWORK, &INFO);
//         //find the best eigenvalue
//         vector<std::pair<RealType,int> > mappedEigenvalues(Nl);
//         for (int i=0; i<Nl; i++)
//         {
//             RealType evi(alphar[i]/beta[i]);
//             if (abs(evi)<1e10)
//             {
//                 mappedEigenvalues[i].first=evi;
//                 mappedEigenvalues[i].second=i;
//             }
//             else
//             {
//                 mappedEigenvalues[i].first=1e100;
//                 mappedEigenvalues[i].second=i;
//             }
//         }
//         std::sort(mappedEigenvalues.begin(),mappedEigenvalues.end());
//         int BestEV(mappedEigenvalues[0].second);
//
// //                   now we rearrange the  the matrices
//         if (BestEV!=0)
//         {
//             bool WANTQ(false);
//             bool WANTZ(true);
//             int ILST(1);
//             int IFST(BestEV+1);
//             LWORK=-1;
//
//             dtgexc(&WANTQ, &WANTZ, &Nl, A.data(), &Nl, B.data(), &Nl, Q.data(), &Nl, Z.data(), &Nl, &IFST, &ILST, &WORK[0], &LWORK, &INFO);
//             LWORK=int(WORK[0]);
//             WORK.resize(LWORK);
//             dtgexc(&WANTQ, &WANTZ, &Nl, A.data(), &Nl, B.data(), &Nl, Q.data(), &Nl, Z.data(), &Nl, &IFST, &ILST, &WORK[0], &LWORK, &INFO);
//         }
//         //now we compute the eigenvector
//         SIDE='R';
//         char HOWMNY('S');
//         int M(0);
//         Matrix<double> Z_I(Nl,Nl);
//         bool SELECT[Nl];
//         for (int zi=0; zi<Nl; zi++) SELECT[zi]=false;
//         SELECT[0]=true;
//
//         WORK.resize(6*Nl);
//         dtgevc(&SIDE, &HOWMNY, &SELECT[0], &Nl, A.data(), &Nl, B.data(), &Nl, Q.data(), &LDQ, Z_I.data(), &Nl, &Nl, &M, &WORK[0], &INFO);
//
//         std::vector<RealType> evec(Nl,0);
//         for (int i=0; i<Nl; i++) for (int j=0; j<Nl; j++) evec[i] += Z(j,i)*Z_I(0,j);
//         for (int i=0; i<Nl; i++) ev[i] = evec[i]/evec[0];
// //     for (int i=0; i<Nl; i++) app_log()<<ev[i]<<" ";
// //     app_log()<<endl;
//         return mappedEigenvalues[0].first;
//     }
//     else
//     {
// // OLD ROUTINE. CALCULATES ALL EIGENVECTORS
// //   Getting the optimal worksize
  RealType zerozero=A(0,0);
  char jl('N');
  char jr('V');
  vector<RealType> alphar(Nl),alphai(Nl),beta(Nl);
  Matrix<RealType> eigenT(Nl,Nl);
  Matrix<RealType> eigenD(Nl,Nl);
  int info;
  int lwork(-1);
  vector<RealType> work(1);
  RealType tt(0);
  int t(1);
  dgeev(&jl, &jr, &Nl, A.data(), &Nl,  &alphar[0], &alphai[0], eigenD.data(), &Nl, eigenT.data(), &Nl, &work[0], &lwork, &info);
  lwork=int(work[0]);
  work.resize(lwork);
  //~ //Get an estimate of E_lin
  //~ Matrix<RealType> H_tmp(HamT);
  //~ Matrix<RealType> S_tmp(ST);
  //~ dggev(&jl, &jr, &Nl, H_tmp.data(), &Nl, S_tmp.data(), &Nl, &alphar[0], &alphai[0], &beta[0],&tt,&t, eigenT.data(), &Nl, &work[0], &lwork, &info);
  //~ RealType E_lin(alphar[0]/beta[0]);
  //~ int e_min_indx(0);
  //~ for (int i=1; i<Nl; i++)
  //~ if (E_lin>(alphar[i]/beta[i]))
  //~ {
  //~ E_lin=alphar[i]/beta[i];
  //~ e_min_indx=i;
  //~ }
  dgeev(&jl, &jr, &Nl, A.data(), &Nl,  &alphar[0], &alphai[0], eigenD.data(), &Nl, eigenT.data(), &Nl, &work[0], &lwork, &info);
  if (info!=0)
  {
    APP_ABORT("Invalid Matrix Diagonalization Function!");
  }
  vector<std::pair<RealType,int> > mappedEigenvalues(Nl);
  for (int i=0; i<Nl; i++)
  {
    RealType evi(alphar[i]);
    if ((evi<zerozero)&&(evi>(zerozero-1e2)))
    {
      mappedEigenvalues[i].first=(evi-zerozero+2.0)*(evi-zerozero+2.0);
      mappedEigenvalues[i].second=i;
    }
    else
    {
      mappedEigenvalues[i].first=1e100;
      mappedEigenvalues[i].second=i;
    }
  }
  std::sort(mappedEigenvalues.begin(),mappedEigenvalues.end());
//         for (int i=0; i<4; i++) app_log()<<i<<": "<<alphar[mappedEigenvalues[i].second]<<endl;
  for (int i=0; i<Nl; i++)
    ev[i] = eigenT(mappedEigenvalues[0].second,i)/eigenT(mappedEigenvalues[0].second,0);
  return alphar[mappedEigenvalues[0].second];
//     }
}
bool QMCLinearOptimize::nonLinearRescale(std::vector<RealType>& dP, Matrix<RealType>& S)
{
  RealType rescale = getNonLinearRescale(dP,S);
  for (int i=1; i<dP.size(); i++)
    dP[i]*=rescale;
  return true;
}


void QMCLinearOptimize::getNonLinearRange(int& first, int& last)
{
  std::vector<int> types;
  optTarget->getParameterTypes(types);
  first=0;
  last=types.size();
  //assume all non-linear coeffs are together.
  if (types[0]==optimize::LINEAR_P)
  {
    int i(0);
    while (i < types.size())
    {
      if (types[i]==optimize::LINEAR_P)
        first=i;
      i++;
    }
    first++;
  }
  else
  {
    int i(types.size()-1);
    while (i >= 0)
    {
      if (types[i]==optimize::LINEAR_P)
        last=i;
      i--;
    }
  }
//     returns the number of non-linear parameters.
//    app_log()<<"line params: "<<first<<" "<<last<<endl;
}

QMCLinearOptimize::RealType QMCLinearOptimize::getNonLinearRescale(std::vector<RealType>& dP, Matrix<RealType>& S)
{
  int first(0),last(0);
  getNonLinearRange(first,last);
  if (first==last)
    return 1.0;
  RealType rescale(1.0);
  RealType xi(0.5);
  RealType D(0.0);
  for (int i=first; i<last; i++)
    for (int j=first; j<last; j++)
      D += S(i+1,j+1)*dP[i+1]*dP[j+1];
  rescale = (1-xi)*D/((1-xi) + xi*std::sqrt(1+D));
  rescale = 1.0/(1.0-rescale);
//     app_log()<<"rescale: "<<rescale<<endl;
  return rescale;
}

void QMCLinearOptimize::orthoScale(std::vector<RealType>& dP, Matrix<RealType>& S)
{
//     int first(0),last(0);
//     getNonLinearRange(first,last);
//     if (first==last) return;
  int x(dP.size());
  Matrix<RealType> T(S);
  std::vector<RealType> nP(dP);
  Matrix<RealType> lS(x,x);
  for (int i=0; i<x; i++)
    for (int j=0; j<x; j++)
      lS(i,j)=S(i+1,j+1);
  RealType Det= invert_matrix(lS,true);
  for (int i=0; i<x; i++)
  {
    dP[i]=0;
    for (int j=0; j<x; j++)
    {
      dP[i]+=nP[j]*lS(i,j);
    }
  }
  RealType rs = getNonLinearRescale(dP,T);
  for (int i=0; i<x; i++)
    dP[i] *=rs;
  for (int i=0; i<dP.size(); i++)
    app_log()<<dP[i]<<" ";
  app_log()<<endl;
//     RealType D(0.0);
//     for (int i=first; i<last; i++) D += 2.0*S(i+1,0)*dP[i];
//     for (int i=first; i<last; i++) for (int j=first; j<last; j++) D += S(i+1,j+1)*dP[i]*dP[j];
//     app_log()<<D<<endl;
//
//
//     RealType rescale = 0.5*D/(0.5 + 0.5*std::sqrt(1.0+D));
//     rescale = 1.0/(1.0-rescale);
//     app_log()<<rescale<<endl;
//     for (int i=0; i<dP.size(); i++) dP[i] *= rescale;
}

QMCLinearOptimize::RealType QMCLinearOptimize::getSplitEigenvectors(int first, int last, Matrix<RealType>& FullLeft, Matrix<RealType>& FullRight, vector<RealType>& FullEV, vector<RealType>& LocalEV, string CSF_Option, bool& CSF_scaled)
{
  vector<RealType> GEVSplitDirection(N,0);
  RealType returnValue;
  int N_nonlin=last-first;
  int N_lin   =N-N_nonlin-1;
//  matrices are one larger than parameter sets
  int M_nonlin=N_nonlin+1;
  int M_lin   =N_lin+1;
//  index mapping for the matrices
  int J_begin(first+1), J_end(last+1);
  int CSF_begin(1), CSF_end(first+1);
  if (first==0)
  {
    CSF_begin=last+1;
    CSF_end=N;
  }
//the Mini matrix composed of just the Nonlinear terms
  Matrix<RealType> LeftTJ(M_nonlin,M_nonlin), RightTJ(M_nonlin,M_nonlin);
//                     assume all jastrow parameters are together either first or last
  LeftTJ(0,0) =  FullLeft(0,0);
  RightTJ(0,0)= FullRight(0,0);
  for (int i=J_begin; i<J_end; i++)
  {
    LeftTJ(i-J_begin+1,0) =  FullLeft(i,0);
    RightTJ(i-J_begin+1,0)= FullRight(i,0);
    LeftTJ(0,i-J_begin+1) =  FullLeft(0,i);
    RightTJ(0,i-J_begin+1)= FullRight(0,i);
    for (int j=J_begin; j<J_end; j++)
    {
      LeftTJ(i-J_begin+1,j-J_begin+1) =  FullLeft(i,j);
      RightTJ(i-J_begin+1,j-J_begin+1)= FullRight(i,j);
    }
  }
  vector<RealType> J_parms(M_nonlin);
  myTimers[2]->start();
  RealType lowest_J_EV =getLowestEigenvector(LeftTJ,RightTJ,J_parms);
  myTimers[2]->stop();
//the Mini matrix composed of just the Linear terms
  Matrix<RealType> LeftTCSF(M_lin,M_lin), RightTCSF(M_lin,M_lin);
  LeftTCSF(0,0) =  FullLeft(0,0);
  RightTCSF(0,0)= FullRight(0,0);
  for (int i=CSF_begin; i<CSF_end; i++)
  {
    LeftTCSF(i-CSF_begin+1,0) =  FullLeft(i,0);
    RightTCSF(i-CSF_begin+1,0)= FullRight(i,0);
    LeftTCSF(0,i-CSF_begin+1) =  FullLeft(0,i);
    RightTCSF(0,i-CSF_begin+1)= FullRight(0,i);
    for (int j=CSF_begin; j<CSF_end; j++)
    {
      LeftTCSF(i-CSF_begin+1,j-CSF_begin+1) =  FullLeft(i,j);
      RightTCSF(i-CSF_begin+1,j-CSF_begin+1)= FullRight(i,j);
    }
  }
  vector<RealType> CSF_parms(M_lin);
  myTimers[2]->start();
  RealType lowest_CSF_EV =getLowestEigenvector(LeftTCSF,RightTCSF,CSF_parms);
  myTimers[2]->stop();
// //                   Now we have both eigenvalues and eigenvectors
//                   app_log()<<" Jastrow eigenvalue: "<<lowest_J_EV<<endl;
//                   app_log()<<"     CSF eigenvalue: "<<lowest_CSF_EV<<endl;
//                We can rescale the matrix and re-solve the whole thing or take the CSF parameters
//                  as solved in the matrix and opt the Jastrow instead
  if (CSF_Option=="freeze")
  {
    returnValue=min(lowest_J_EV,lowest_CSF_EV);
//                   Line minimize for the nonlinear components
    for (int i=J_begin; i<J_end; i++)
      GEVSplitDirection[i]=J_parms[i-J_begin+1];
//                   freeze the CSF components at this minimum
    for (int i=CSF_begin; i<CSF_end; i++)
      LocalEV[i-1]=CSF_parms[i-CSF_begin+1];
    FullEV[0]=1.0;
    for (int i=J_begin; i<J_end; i++)
      FullEV[i]=GEVSplitDirection[i];
  }
  else
    if (CSF_Option=="rescale")
    {
      RealType matrixRescaler=std::sqrt(std::abs(lowest_CSF_EV/lowest_J_EV));
      for (int i=0; i<N; i++)
        for (int j=0; j<N; j++)
        {
          if ((i>=J_begin)&&(i<J_end))
          {
            FullLeft(i,j) *=matrixRescaler;
            FullRight(i,j)*=matrixRescaler;
          }
          if ((j>=J_begin)&&(j<J_end))
          {
            FullLeft(i,j) *=matrixRescaler;
            FullRight(i,j)*=matrixRescaler;
          }
        }
      myTimers[2]->start();
      returnValue =getLowestEigenvector(FullLeft,FullRight,FullEV);
      myTimers[2]->stop();
    }
    else
      if (CSF_Option =="stability")
      {
//       just return the value of the CSF part
        if (lowest_J_EV>lowest_CSF_EV)
          CSF_scaled=true;
        else
          CSF_scaled=false;
        returnValue=min(lowest_J_EV,lowest_CSF_EV);
      }
  return returnValue;
}

/** Parses the xml input file for parameter definitions for the wavefunction optimization.
* @param q current xmlNode
* @return true if successful
*/
bool
QMCLinearOptimize::put(xmlNodePtr q)
{
  string useGPU("yes");
  string vmcMove("pbyp");
  OhmmsAttributeSet oAttrib;
  oAttrib.add(useGPU,"gpu");
  oAttrib.add(vmcMove,"move");
  oAttrib.put(q);
  optNode=q;
  xmlNodePtr cur=optNode->children;
  while (cur != NULL)
  {
    string cname((const char*)(cur->name));
    if (cname == "mcwalkerset")
    {
      mcwalkerNodePtr.push_back(cur);
    }
    cur=cur->next;
  }

  bool success=true;
  if (optTarget == 0)
  {
#if defined (QMC_CUDA)
    if (qmc_common.compute_device && useGPU != "no")
      optTarget = new QMCCostFunctionCUDA(W,Psi,H,hamPool);
    else
#endif
      optTarget = new QMCCostFunctionOMP(W,Psi,H,hamPool);
    optTarget->setStream(&app_log());
    success=optTarget->put(q);
  }

  //create VMC engine
  if (vmcEngine ==0)
  {
#if defined (QMC_CUDA)
    if (qmc_common.compute_device && useGPU != "no")
      vmcEngine = new VMCcuda(W,Psi,H,psiPool);
    else
#endif
      vmcEngine = new VMCSingleOMP(W,Psi,H,hamPool,psiPool);
    vmcEngine->setUpdateMode(vmcMove[0] == 'p');
    vmcEngine->initCommunicator(myComm);
  }
  vmcEngine->setStatus(RootName,h5FileRoot,AppendRun);
  vmcEngine->process(optNode);

  return success;
}

void QMCLinearOptimize::resetComponents(xmlNodePtr cur)
{
  if(qmc_common.qmc_counter==0)
  {
    app_log() << "  Resetting parameters for optimization " << endl;
    optNode=cur;
    m_param.put(cur);

    optTarget->put(cur);
    vmcEngine->resetComponents(cur);
  }
  else
    app_log() << "  Reusing parameters for the optimizations from the previous iteration " << endl;
}

bool QMCLinearOptimize::fitMappedStabilizers(vector<std::pair<RealType,RealType> >&
    mappedStabilizers, RealType& XS, RealType& val, RealType tooBig )
{
  int nms(0);
  for (int i=0; i<mappedStabilizers.size(); i++)
    if (mappedStabilizers[i].second==mappedStabilizers[i].second)
      nms++;
  bool SuccessfulFit(false);
  if (nms>=5)
  {
    //Quartic fit the stabilizers we have tried and try to choose the best we can
    vector<RealType>  Y(nms), Coefs(5);
    Matrix<RealType> X(nms,5);
    for (int i=0; i<nms; i++)
      if(mappedStabilizers[i].second==mappedStabilizers[i].second)
      {
        X(i,0)=1.0;
        X(i,1)=mappedStabilizers[i].first;
        X(i,2)=std::pow(mappedStabilizers[i].first,2);
        X(i,3)=std::pow(mappedStabilizers[i].first,3);
        X(i,4)=std::pow(mappedStabilizers[i].first,4);
        Y[i]=mappedStabilizers[i].second;
      }
    LinearFit(Y,X,Coefs);
    RealType Xmin = QuarticMinimum(Coefs);
    val=0;
    for (int i=0; i<5; i++)
      val+=std::pow(Xmin,i)*Coefs[i];
    app_log()<<"quartic Fit min: "<<Xmin<<" val: "<<val<<endl;;
//         for (int i=0; i<5; i++) app_log()<<Coefs[i]<<" ";
//         app_log()<<endl;
    SuccessfulFit=true;
    for (int i=0; i<nms; i++)
      if(mappedStabilizers[i].second==mappedStabilizers[i].second)
        if (val>mappedStabilizers[i].second)
          SuccessfulFit=false;
    if(Xmin>tooBig)
      SuccessfulFit=false;
    if (SuccessfulFit)
      XS=Xmin;
  }
  else
    if (nms>=3)
    {
      //Quadratic fit the stabilizers we have tried and try to choose the best we can
      std::sort(mappedStabilizers.begin(),mappedStabilizers.end());
      vector<RealType>  Y(nms), Coefs(3);
      Matrix<RealType> X(nms,3);
      for (int i=0; i<nms; i++)
        if(mappedStabilizers[i].second==mappedStabilizers[i].second)
        {
          X(i,0)=1.0;
          X(i,1)=mappedStabilizers[i].first;
          X(i,2)=std::pow(mappedStabilizers[i].first,2);
          Y[i]=mappedStabilizers[i].second;
        }
      LinearFit(Y,X,Coefs);
      //extremum really.
      RealType Xmin = -0.5*Coefs[1]/Coefs[2];
      val=0;
      for (int i=0; i<3; i++)
        val+=std::pow(Xmin,i)*Coefs[i];
      app_log()<<"quadratic Fit min: "<<Xmin<<" val: "<<val<<endl;
//         for (int i=0; i<3; i++) app_log()<<Coefs[i]<<" ";
//         app_log()<<endl;
      SuccessfulFit=true;
      if(Xmin>tooBig)
        SuccessfulFit=false;
      for (int i=0; i<nms; i++)
        if(mappedStabilizers[i].second==mappedStabilizers[i].second)
          if (val>mappedStabilizers[i].second)
            SuccessfulFit=false;
      if (SuccessfulFit)
        XS = Xmin;
    }
  return SuccessfulFit;
}
}
/***************************************************************************
* $RCSfile$   $Author: jnkim $
* $Revision: 6035 $   $Date: 2013-10-28 07:34:33 -0700 (Mon, 28 Oct 2013) $
* $Id: QMCLinearOptimize.cpp 6035 2013-10-28 14:34:33Z jnkim $
***************************************************************************/