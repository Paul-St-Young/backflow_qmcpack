//////////////////////////////////////////////////////////////////
// (c) Copyright 2009- by Jeremy McMinis and Jeongnim Kim
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
#include "QMCDrivers/ForwardWalking/FRSingleOMP.h"
#include "QMCDrivers/ForwardWalkingStructure.h"

namespace qmcplusplus
{

/// Constructor.
FRSingleOMP::FRSingleOMP(MCWalkerConfiguration& w, TrialWaveFunction& psi, QMCHamiltonian& h, HamiltonianPool& hpool, ParticleSetPool& pset, WaveFunctionPool& ppool)
  : QMCDriver(w,psi,h,ppool), CloneManager(hpool), weightFreq(1), weightLength(1), fname(""), verbose(0)
  , WIDstring("WalkerID"), PIDstring("ParentID"),gensTransferred(1),startStep(0), doWeights(1), gridDivs(100), ptclPool(pset), ionroot("ion0")
{
  RootName = "FR";
  QMCType ="FRSingleOMP";
  xmlrootName="";
  m_param.add(xmlrootName,"rootname","string");
  m_param.add(ionroot,"ion","string");
  m_param.add(weightLength,"numbersteps","int");
  m_param.add(gridDivs,"bins","int");
  m_param.add(doWeights,"weights","int");
  m_param.add(weightFreq,"skipsteps","int");
  m_param.add(verbose,"verbose","int");
  m_param.add(startStep,"ignore","int");
  hdf_ID_data.setID(WIDstring);
  hdf_PID_data.setID(PIDstring);
}

bool FRSingleOMP::run()
{
  overL = 1.0/W.Lattice.Length[0];
  int nelectrons = W[0]->R.size();
  int nfloats=OHMMS_DIM*nelectrons;
  if (verbose>1)
    app_log()<<" nelectrons: "<<nelectrons<<" nfloats: "<<nfloats<<" ngrids: "<<gridDivs<<" overL: "<<overL<<endl;
  int Xind = gridDivs*gridDivs;
  int Yind = gridDivs;
  ParticleSet* ions = ptclPool.getParticleSet(ionroot);
  if(ions == 0)
  {
    APP_ABORT("Unknown source \"" + ionroot + "\" for density.");
  }
  vector<string> IONtypes(ions->mySpecies.speciesName);
  SpeciesSet& tspecies(ions->mySpecies);
  int membersize= tspecies.addAttribute("membersize");
  vector<string> IONlist;
  for(int i=0; i<IONtypes.size(); i++)
    for(int j=0; j<tspecies(membersize,i); j++)
      IONlist.push_back(IONtypes[i]);
  //     stringstream Iname("");
  //     for(int i=0;i<IONnames.size();i++) for(int j=0;j<tspecies(membersize,i);j++)
  //     {
  //       Iname<<IONnames[i];
  //       if ((i!=IONnames.size()-1)||(j!=tspecies(membersize,i)-1)) Iname<<",";
  //     }
  //     char Names[sze];
  //     for(int i=0;i<sze;i++) Names[i] = Iname.str()[i];
  //     for(int i=0;i<sze;i++) app_log()<<Names[i];
  vector<double> IONpos;
  int Nions = ions->R.size();
  for(int i=0; i<Nions; i++)
  {
    IONpos.push_back(ions->R[i][0]);
    IONpos.push_back(ions->R[i][1]);
    IONpos.push_back(ions->R[i][2]);
  }
  //     for (int i=0;i<IONnames.size();i++)
  //     app_log()<<IONnames[i]<<" ";
  //     app_log()<<endl;
  hdf_Den_data.setFileName(xmlrootName);
  hdf_Den_data.makeFile();
  hdf_Den_data.openFile();
  hdf_Den_data.writeIons(IONlist,IONpos,W.Lattice.Length[0],nelectrons,gridDivs);
  hdf_Den_data.closeFile();
  if (doWeights==1)
  {
    HDF5_FW_weights hdf_WGT_data;
    hdf_WGT_data.setFileName(xmlrootName);
    fillIDMatrix();
    hdf_WGT_data.makeFile();
    hdf_WGT_data.openFile();
    hdf_WGT_data.addFW(0);
    for (int i=0; i<Weights.size(); i++)
      hdf_WGT_data.addStep(i,Weights[i]);
    hdf_WGT_data.closeFW();
    hdf_WGT_data.closeFile();
    for(int ill=1; ill<weightLength; ill++)
    {
      transferParentsOneGeneration();
      FWOneStep();
      //       WeightHistory.push_back(Weights);
      hdf_WGT_data.openFile();
      hdf_WGT_data.addFW(ill);
      for (int i=0; i<Weights.size(); i++)
        hdf_WGT_data.addStep(i,Weights[i]);
      hdf_WGT_data.closeFW();
      hdf_WGT_data.closeFile();
    }
  }
  else
  {
    fillIDMatrix();
    //           find weight length from the weight file
    HDF5_FW_weights hdf_WGT_data;
    hdf_WGT_data.setFileName(xmlrootName);
    hid_t f_file = H5Fopen(hdf_WGT_data.getFileName().c_str(),H5F_ACC_RDONLY,H5P_DEFAULT);
    hsize_t numGrps = 0;
    H5Gget_num_objs(f_file, &numGrps);
    weightLength = static_cast<int> (numGrps)-1;
    if (H5Fclose(f_file)>-1)
      f_file=-1;
    if (verbose>0)
      app_log()<<" weightLength "<<weightLength<<endl;
  }
  if (verbose>0)
    app_log()<<" Done Computing Weights"<<endl;
  for(int Gen=0; Gen<weightLength; Gen++)
  {
    int totalWeight(0);
    vector<int> Density(gridDivs*gridDivs*gridDivs,0);
    vector<vector<float>* > ThreadsCoordinate(NumThreads);
    vector<vector<int>* > weights(NumThreads);
    vector<vector<int>* > localDensity(NumThreads);
    for (int i=0; i<NumThreads; i++)
    {
      ThreadsCoordinate[i] = new vector<float>();
      weights[i] = new vector<int>();
      localDensity[i] = new vector<int> (gridDivs*gridDivs*gridDivs,0);
    }
    vector<int> localWeight(NumThreads,0);
    for(int step=startStep; step<(numSteps-Gen); step+=NumThreads)
    {
      int nEperB(-1);
      int nthr = std::min( (numSteps-Gen) - (step), NumThreads);
      for (int ip=0; ip<nthr; ip++)
      {
        HDF5_FW_float hdf_float_data;
        HDF5_FW_weights hdf_WGT_data;
        hdf_WGT_data.setFileName(xmlrootName);
        hdf_float_data.openFile(fname.str());
        hdf_WGT_data.openFile();
        hdf_float_data.setStep(step+ip);
        (*ThreadsCoordinate[ip]).resize(walkersPerBlock[step+ip]*nfloats);
        (*weights[ip]).resize(walkersPerBlock[step+ip]);
        nEperB = hdf_float_data.getFloat(0, walkersPerBlock[step+ip]*nfloats, (*ThreadsCoordinate[ip]) )/OHMMS_DIM;
        hdf_float_data.endStep();
        hdf_float_data.closeFile();
        hdf_WGT_data.readStep(Gen,step+ip,(*weights[ip]));
        hdf_WGT_data.closeFile();
      }
      //         if (ip<nthr)
      //         app_log()<< "walkersPerBlock[step] : "<<walkersPerBlock[step]<<" step: "<<step<<" nthr: "<<nthr<<endl;
      //         app_log()<<ThreadsCoordinate[ip]->size()<< " "<<walkersPerBlock[step]*nfloats<<endl;
      #pragma omp parallel for
      for (int ip=0; ip<nthr; ip++)
        for(int iwt=0; iwt<walkersPerBlock[step+ip]; iwt++)
        {
          int ww=(*weights[ip])[iwt];
          if (ww>0)
          {
            localWeight[ip]+=ww;
            for(vector<float>::iterator TCit(ThreadsCoordinate[ip]->begin()+nfloats*iwt), TCit_end(ThreadsCoordinate[ip]->begin()+nfloats*(1+iwt)); TCit<TCit_end;)
            {
              int a= static_cast<int>(gridDivs*(overL*(*TCit) - std::floor( overL*(*TCit) ) ) );
              TCit++;
              int b= static_cast<int>(gridDivs*(overL*(*TCit) - std::floor(overL*(*TCit)) ));
              TCit++;
              int c= static_cast<int>(gridDivs*(overL*(*TCit) - std::floor(overL*(*TCit))));
              TCit++;
              (*localDensity[ip])[Xind*a+Yind*b+c] += ww;
            }
          }
        }
      if (verbose >1)
        cout<<"Done with step: "<<step<<" Gen: "<<Gen<<endl;
    }
    //       for (int ip2=0;ip2<NumThreads;ip2++)
    //       {
    //         totalWeight += localWeight[ip2];
    //         vector<int> ldh = *localDensity[ip2];
    //             for(int i=0;i<gridDivs*gridDivs*gridDivs;i++)
    //               Density[i] += ldh[i];
    //       }
    //       totalWeight=localWeight[0];
    //       Density=(*localDensity[0]);
    for (int ip=0; ip<NumThreads; ip++)
    {
      totalWeight += localWeight[ip];
      for(int i=0; i<gridDivs*gridDivs*gridDivs; i++)
      {
        Density[i] += (*localDensity[ip])[i];
      }
    }
    vector<int> info(3);
    info[0]=gridDivs;
    info[1]=Gen;
    info[2]=totalWeight;
    if (verbose >1)
      cout<<"Writing Gen: "<<Gen<<" to file"<<endl;
    hdf_Den_data.openFile();
    hdf_Den_data.writeDensity(info, Density);
    hdf_Den_data.closeFile();
  }
  return true;
}

int FRSingleOMP::getNumberOfSamples(int omittedSteps)
{
  int returnValue(0);
  for(int i=startStep; i<(numSteps-omittedSteps); i++)
    returnValue +=walkersPerBlock[i];
  return returnValue;
}

void FRSingleOMP::fillIDMatrix()
{
  if (verbose>0)
    app_log()<<" There are "<<numSteps<<" steps"<<endl;
  IDs.resize(numSteps);
  PIDs.resize(numSteps);
  Weights.resize(numSteps);
  vector<vector<long> >::iterator stepIDIterator(IDs.begin());
  vector<vector<long> >::iterator stepPIDIterator(PIDs.begin());
  int st(0);
  hdf_ID_data.openFile(fname.str());
  hdf_PID_data.openFile(fname.str());
  do
  {
    hdf_ID_data.readAll(st,*(stepIDIterator));
    hdf_PID_data.readAll(st,*(stepPIDIterator));
    walkersPerBlock.push_back( (*stepIDIterator).size() );
    stepIDIterator++;
    stepPIDIterator++;
    st++;
    if (verbose>1)
      app_log()<<"step:"<<st<<endl;
  }
  while (st<numSteps);
  hdf_ID_data.closeFile();
  hdf_PID_data.closeFile();
  //     Weights.resize( IDs.size());
  for(int i=0; i<numSteps; i++)
    Weights[i].resize(IDs[i].size(),1);
  realPIDs = PIDs;
  realIDs = IDs;
}


void FRSingleOMP::resetWeights()
{
  if (verbose>2)
    app_log()<<" Resetting Weights"<<endl;
  Weights.clear();
  Weights.resize(numSteps);
  for(int i=0; i<numSteps; i++)
    Weights[i].resize(IDs[i].size(),0);
}

void FRSingleOMP::FWOneStep()
{
  //create an ordered version of the ParentIDs to make the weight calculation faster
  vector<vector<long> > orderedPIDs=(PIDs);
  vector<vector<long> >::iterator it(orderedPIDs.begin());
  do
  {
    std::sort((*it).begin(),(*it).end());
    it++;
  }
  while(it<orderedPIDs.end());
  //     vector<vector<long> >::iterator it(orderedPIDs.begin());
  //        do
  //     {
  //       vector<long>::iterator it2((*it).begin());
  //       for(;*it2 < *(it2+1);) it2++;
  //       std::sort(it2,(*it).end());
  //       it++;
  //     } while(it<orderedPIDs.end());
  if (verbose>2)
    app_log()<<" Done Sorting IDs"<<endl;
  resetWeights();
  vector<vector<long> >::iterator stepIDIterator(IDs.begin());
  vector<vector<long> >::iterator stepPIDIterator(orderedPIDs.begin() + gensTransferred);
  vector<vector<int> >::iterator stepWeightsIterator(Weights.begin());
  //we start comparing the next generations ParentIDs with the current generations IDs
  int i=0;
  do
  {
    if (verbose>2)
      app_log()<<"  calculating weights for gen:"<<gensTransferred<<" step:"<<i<<"/"<<orderedPIDs.size()<<endl;
    //       if (verbose>2) app_log()<<"Nsamples ="<<(*stepWeightsIteratoetWeights).size()<<endl;
    vector<long>::iterator IDit( (*stepIDIterator).begin()     );
    vector<long>::iterator PIDit( (*stepPIDIterator).begin()   );
    vector<int>::iterator  Wit( (*stepWeightsIterator).begin() );
    if (verbose>2)
      app_log()<<"ID size:"<<(*stepIDIterator).size()<<" PID size:"<<(*stepPIDIterator).size()<<" Weight size:"<<(*stepWeightsIterator).size()<<endl;
    do
    {
      if ((*PIDit)==(*IDit))
      {
        (*Wit)++;
        PIDit++;
      }
      else
      {
        IDit++;
        Wit++;
        if (IDit==(*stepIDIterator).end())
        {
          IDit=(*stepIDIterator).begin();
          Wit=(*stepWeightsIterator).begin();
        }
      }
    }
    while(PIDit<(*stepPIDIterator).end());
    //       if (verbose>2) { printIDs((*stepIDIterator));printIDs((*stepPIDIterator));}
    //       if (verbose>2) printInts((*stepWeightsIterator));
    stepIDIterator++;
    stepPIDIterator++;
    stepWeightsIterator++;
    i++;
  }
  while(stepPIDIterator<orderedPIDs.end());
}

void FRSingleOMP::printIDs(vector<long> vi)
{
  for (int j=0; j<vi.size(); j++)
    app_log()<<vi[j]<<" ";
  app_log()<<endl;
}
void FRSingleOMP::printInts(vector<int> vi)
{
  for (int j=0; j<vi.size(); j++)
    app_log()<<vi[j]<<" ";
  app_log()<<endl;
}

void FRSingleOMP::transferParentsOneGeneration( )
{
  vector<vector<long> >::reverse_iterator stepIDIterator(IDs.rbegin());
  vector<vector<long> >::reverse_iterator stepPIDIterator(PIDs.rbegin()), nextStepPIDIterator(realPIDs.rbegin());
  stepIDIterator+=gensTransferred;
  nextStepPIDIterator+=gensTransferred;
  int i(0);
  do
  {
    vector<long>::iterator hereID( (*stepIDIterator).begin() ) ;
    vector<long>::iterator nextStepPID( (*nextStepPIDIterator).begin() );
    vector<long>::iterator herePID( (*stepPIDIterator).begin() );
    if (verbose>2)
      app_log()<<"  calculating Parent IDs for gen:"<<gensTransferred<<" step:"<<i<<"/"<<PIDs.size()-gensTransferred<<endl;
    if (verbose>2)
    {
      printIDs((*nextStepPIDIterator));
      printIDs((*stepIDIterator));
      printIDs((*stepPIDIterator));
    }
    do
    {
      if ((*herePID)==(*hereID))
      {
        (*herePID)=(*nextStepPID);
        herePID++;
      }
      else
      {
        hereID++;
        nextStepPID++;
        if (hereID==(*stepIDIterator).end())
        {
          hereID=(*stepIDIterator).begin();
          nextStepPID=(*nextStepPIDIterator).begin();
          //             if (verbose>2) app_log()<<"resetting to beginning of parents"<<endl;
        }
      }
    }
    while(herePID<(*stepPIDIterator).end());
    stepIDIterator++;
    nextStepPIDIterator++;
    stepPIDIterator++;
    i++;
  }
  while(stepIDIterator<IDs.rend());
  gensTransferred++; //number of gens backward to compare parents
  if (verbose>2)
    app_log()<<"  Finished generation block"<<endl;
}


bool
FRSingleOMP::put(xmlNodePtr q)
{
  fname<<xmlrootName<<".storeConfig.h5";
  c_file = H5Fopen(fname.str().c_str(),H5F_ACC_RDONLY,H5P_DEFAULT);
  hsize_t numGrps = 0;
  H5Gget_num_objs(c_file, &numGrps);
  numSteps = static_cast<int> (numGrps)-3;
  app_log()<<"  Total number of steps in input file "<<numSteps<<endl;
  if (weightFreq<1)
    weightFreq=1;
  int numberDataPoints = weightLength/weightFreq;
  //     pointsToCalculate.resize(numberDataPoints);
  //     for(int i=0;i<numberDataPoints;i++) pointsToCalculate[i]=i*weightFreq;
  app_log()<<"  "<<numberDataPoints<<" sets of observables will be calculated each "<<weightFreq<<" steps"<<endl;
  app_log()<<"  Config Generations skipped for thermalization: "<<startStep<<endl;//<<" steps. At: ";
  //     for(int i=0;i<numberDataPoints;i++) app_log()<<pointsToCalculate[i]<<" ";
  app_log()<<endl;
  if (H5Fclose(c_file)>-1)
    c_file=-1;
  return true;
}
}

/***************************************************************************
 * $RCSfile: VMCParticleByParticle.cpp,v $   $Author: jnkim $
 * $Revision: 1.25 $   $Date: 2006/10/18 17:03:05 $
 * $Id: VMCParticleByParticle.cpp,v 1.25 2006/10/18 17:03:05 jnkim Exp $
 ***************************************************************************/
