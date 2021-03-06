/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2012-2016 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed.org for more information.

   This file is part of plumed, version 2.

   plumed is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   plumed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with plumed.  If not, see <http://www.gnu.org/licenses/>.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
#include <cmath>

#include "Function.h"
#include "ActionRegister.h"

#include <string>
#include <cstring>
#include <iostream>

using namespace std;

namespace PLMD{
namespace function{

//+PLUMEDOC FUNCTION FUNCPATHMSD
/*
This function calculates path collective variables. 

This is the Path Collective Variables implementation 
( see \cite brand07 ).
This variable computes the progress along a given set of frames that is provided  
in input ("s" component) and the distance from them ("z" component). 
It is a function of MSD that are obtained by the joint use of MSD variable and SQUARED flag 
(see below).

\par Examples

Here below is a case where you have defined three frames and you want to  
calculate the progress alng the path and the distance from it in p1

\verbatim
t1: RMSD REFERENCE=frame_1.dat TYPE=OPTIMAL SQUARED
t2: RMSD REFERENCE=frame_21.dat TYPE=OPTIMAL SQUARED
t3: RMSD REFERENCE=frame_42.dat TYPE=OPTIMAL SQUARED
p1: FUNCPATHMSD ARG=t1,t2,t3 LAMBDA=500.0 
PRINT ARG=t1,t2,t3,p1.s,p1.z STRIDE=1 FILE=colvar FMT=%8.4f
\endverbatim

In this second example is shown how to define a PATH in the \ref CONTACTMAP space:

\verbatim
CONTACTMAP ...
ATOMS1=1,2 REFERENCE1=0.1  
ATOMS2=3,4 REFERENCE2=0.5  
ATOMS3=4,5 REFERENCE3=0.25  
ATOMS4=5,6 REFERENCE4=0.0  
SWITCH=(RATIONAL R_0=1.5) 
LABEL=c1
CMDIST
... CONTACTMAP

CONTACTMAP ...
ATOMS1=1,2 REFERENCE1=0.3  
ATOMS2=3,4 REFERENCE2=0.9  
ATOMS3=4,5 REFERENCE3=0.45  
ATOMS4=5,6 REFERENCE4=0.1  
SWITCH=(RATIONAL R_0=1.5) 
LABEL=c2
CMDIST
... CONTACTMAP

CONTACTMAP ...
ATOMS1=1,2 REFERENCE1=1.0  
ATOMS2=3,4 REFERENCE2=1.0  
ATOMS3=4,5 REFERENCE3=1.0  
ATOMS4=5,6 REFERENCE4=1.0  
SWITCH=(RATIONAL R_0=1.5) 
LABEL=c3
CMDIST
... CONTACTMAP

p1: FUNCPATHMSD ARG=c1,c2,c3 LAMBDA=500.0 
PRINT ARG=c1,c2,c3,p1.s,p1.z STRIDE=1 FILE=colvar FMT=%8.4f
\endverbatim

*/
//+ENDPLUMEDOC
   
class FuncPathMSD : public Function {
  double lambda;
  int neigh_size;
  double neigh_stride;
  vector< pair<Value *,double> > neighpair;
  map<Value *,double > indexmap; // use double to allow isomaps
  vector <Value*> allArguments; 
// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
// this below is useful when one wants to sort a vector of double and have back the order 
// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
// create a custom sorter
typedef vector<double>::const_iterator myiter;
struct ordering {
       bool operator ()(pair<unsigned , myiter> const& a, pair<unsigned , myiter> const& b) {
           return *(a.second) < *(b.second);
       }
};
// sorting utility
vector<int> increasingOrder( vector<double> &v){
   // make a pair
   vector< pair<unsigned , myiter> > order(v.size());
   unsigned n = 0;
   for (myiter it = v.begin(); it != v.end(); ++it, ++n){
       order[n] = make_pair(n, it); // note: heere i do not put the values but the addresses that point to the value 
   }
   // now sort according the second value
   sort(order.begin(), order.end(), ordering());
   typedef vector< pair<unsigned , myiter> >::const_iterator pairiter;
   vector<int> vv(v.size());n=0;
   for ( pairiter it = order.begin(); it != order.end(); ++it ){
       vv[n]=(*it).first;n++; 
   }
   return vv;
}
// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

struct pairordering {
       bool operator ()(pair<Value * , double> const& a, pair<Value* , double> const& b) {
           return (a).second > (b).second;
       }
};

public:
  explicit FuncPathMSD(const ActionOptions&);
// active methods:
  virtual void calculate();
  virtual void prepare();
  static void registerKeywords(Keywords& keys);
};

PLUMED_REGISTER_ACTION(FuncPathMSD,"FUNCPATHMSD")

void FuncPathMSD::registerKeywords(Keywords& keys){
  Function::registerKeywords(keys);
  keys.use("ARG");
  keys.add("compulsory","LAMBDA","the lambda parameter is needed for smoothing, is in the units of plumed");
  keys.add("optional","NEIGH_SIZE","size of the neighbor list");
  keys.add("optional","NEIGH_STRIDE","how often the neighbor list needs to be calculated in time units");
  componentsAreNotOptional(keys);
  keys.addOutputComponent("s","default","the position on the path");
  keys.addOutputComponent("z","default","the distance from the path");
}
FuncPathMSD::FuncPathMSD(const ActionOptions&ao):
Action(ao),
Function(ao),
neigh_size(-1),
neigh_stride(-1.)
{

  parse("LAMBDA",lambda);
  parse("NEIGH_SIZE",neigh_size);
  parse("NEIGH_STRIDE",neigh_stride);
  checkRead();
  log.printf("  lambda is %f\n",lambda);
  // list the action involved and check the type 
  for(unsigned i=0;i<getNumberOfArguments();i++){
       // for each value get the name and the label of the corresponding action
       std::string myname=getPntrToArgument(i)->getPntrToAction()->getName(); 
       if(myname!="RMSD"&&myname!="CONTACTMAP")plumed_merror("This argument is not of RMSD type!!!");
  }   
  log.printf("  Consistency check completed! Your path cvs look good!\n"); 
  // do some neighbor printout
  if(neigh_stride>0. || neigh_size>0){
           if(neigh_size>getNumberOfArguments()){
           	log.printf(" List size required ( %d ) is too large: resizing to the maximum number of arg required: %d  \n",neigh_size,getNumberOfArguments());
 		neigh_size=getNumberOfArguments();
           }
           log.printf("  Neighbor list enabled: \n");
           log.printf("                size   :  %d elements\n",neigh_size);
           log.printf("                stride :  %f time \n",neigh_stride);
  }else{
           log.printf("  Neighbor list NOT enabled \n");
  }

  addComponentWithDerivatives("s"); componentIsNotPeriodic("s");
  addComponentWithDerivatives("z"); componentIsNotPeriodic("z");

  // now backup the arguments 
  for(unsigned i=0;i<getNumberOfArguments();i++)allArguments.push_back(getPntrToArgument(i)); 
  double i=1.;
  for(std::vector<Value*>:: const_iterator it=allArguments.begin();it!=allArguments.end()  ;++it){
		indexmap[(*it)]=i;i+=1.;
  }
  
}
// calculator
void FuncPathMSD::calculate(){
 // log.printf("NOW CALCULATE! \n");
  double s_path=0.;
  double partition=0.;
  if(neighpair.empty()){ // at first step, resize it
       neighpair.resize(allArguments.size());  
       for(unsigned i=0;i<allArguments.size();i++)neighpair[i].first=allArguments[i]; 
  }

  Value* val_s_path=getPntrToComponent("s");
  Value* val_z_path=getPntrToComponent("z");

  typedef  vector< pair< Value *,double> >::iterator pairiter;
  for(pairiter it=neighpair.begin();it!=neighpair.end();++it){ 
    (*it).second=exp(-lambda*((*it).first->get()));
    s_path+=(indexmap[(*it).first])*(*it).second;
    partition+=(*it).second;
  }
  s_path/=partition;
  val_s_path->set(s_path);
  val_z_path->set(-(1./lambda)*std::log(partition));
  int n=0;
  for(pairiter it=neighpair.begin();it!=neighpair.end();++it){ 
    double expval=(*it).second;
    double tmp=lambda*expval*(s_path-(indexmap[(*it).first]))/partition;
    setDerivative(val_s_path,n,tmp);
    setDerivative(val_z_path,n,expval/partition);
    n++;
  }

//  log.printf("CALCULATION DONE! \n");
}
///
/// this function updates the needed argument list
///
void FuncPathMSD::prepare(){

  // neighbor list: rank and activate the chain for the next step 

  // neighbor list: if neigh_size<0 never sort and keep the full vector
  // neighbor list: if neigh_size>0  
  //                if the size is full -> sort the vector and decide the dependencies for next step 
  //                if the size is not full -> check if next step will need the full dependency otherwise keep this dependencies 

  // here just resize the neighpair. The real resizing of reinit will be done by the prepare stage that will modify the  list of arguments
  if (neigh_size>0){
     if(neighpair.size()==allArguments.size()){ // I just did the complete round: need to sort, shorten and give it a go
                // sort the values  
		sort(neighpair.begin(),neighpair.end(),pairordering());
                // resize the effective list
                neighpair.resize(neigh_size);
		log.printf("  NEIGH LIST NOW INCLUDE INDEXES: ");
		for(int i=0;i<neigh_size;++i)log.printf(" %f ",indexmap[neighpair[i].first]);log.printf(" \n");
     }else{
        if( int(getStep())%int(neigh_stride/getTimeStep())==0 ){
                 log.printf(" Time %f : recalculating full neighlist \n",getStep()*getTimeStep());
     		 neighpair.resize(allArguments.size());  
     		 for(unsigned i=0;i<allArguments.size();i++)neighpair[i].first=allArguments[i]; 
        }
     } 
  }else{
            if( int(getStep())==0){
    		 neighpair.resize(allArguments.size());  
     		 for(unsigned i=0;i<allArguments.size();i++)neighpair[i].first=allArguments[i]; 
            }
  }
 typedef  vector< pair<Value*,double> >::iterator pairiter;
 vector<Value*> argstocall; 
 //log.printf("PREPARING \n");
 argstocall.clear(); 
 if(!neighpair.empty()){
    for(pairiter it=neighpair.begin();it!=neighpair.end();++it){
       argstocall.push_back( (*it).first );
  //     log.printf("CALLING %p %f ",(*it).first ,indexmap[(*it).first] ); 
    }
 }else{
    for(unsigned i=0;i<allArguments.size();i++){
	argstocall.push_back(allArguments[i]);  
    }
 }	  
 // now the list of argument changes
 requestArguments(argstocall);
 //now resize the derivatives as well
 //for each value in this action
 for(int i=0;i< getNumberOfComponents();i++){
 	//resize the derivative to the number   the 
	getPntrToComponent(i)->clearDerivatives();
	getPntrToComponent(i)->resizeDerivatives(getNumberOfArguments());
 }
 //log.printf("PREPARING DONE! \n");
}

}
}


