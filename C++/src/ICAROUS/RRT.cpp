/**
 * RRT
 *
 * RRT search
 *
 * Contact: Swee Balachandran (swee.balachandran@nianet.org)
 *
 *
 * Copyright (c) 2011-2016 United States Government as represented by
 * the National Aeronautics and Space Administration.  No copyright
 * is claimed in the United States under Title 17, U.S.Code. All Other
 * Rights Reserved.
 *
 * Notices:
 *  Copyright 2016 United States Government as represented by the Administrator of the National Aeronautics and Space Administration.
 *  All rights reserved.
 *
 * Disclaimers:
 *  No Warranty: THE SUBJECT SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY OF ANY KIND, EITHER EXPRESSED,
 *  IMPLIED, OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, ANY WARRANTY THAT THE SUBJECT SOFTWARE WILL CONFORM TO SPECIFICATIONS, ANY
 *  IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR FREEDOM FROM INFRINGEMENT,
 *  ANY WARRANTY THAT THE SUBJECT SOFTWARE WILL BE ERROR FREE, OR ANY WARRANTY THAT DOCUMENTATION, IF PROVIDED,
 *  WILL CONFORM TO THE SUBJECT SOFTWARE. THIS AGREEMENT DOES NOT, IN ANY MANNER, CONSTITUTE AN ENDORSEMENT BY GOVERNMENT
 *  AGENCY OR ANY PRIOR RECIPIENT OF ANY RESULTS, RESULTING DESIGNS, HARDWARE, SOFTWARE PRODUCTS OR ANY OTHER APPLICATIONS
 *  RESULTING FROM USE OF THE SUBJECT SOFTWARE.  FURTHER, GOVERNMENT AGENCY DISCLAIMS ALL WARRANTIES AND
 *  LIABILITIES REGARDING THIRD-PARTY SOFTWARE, IF PRESENT IN THE ORIGINAL SOFTWARE, AND DISTRIBUTES IT "AS IS."
 *
 * Waiver and Indemnity:
 *   RECIPIENT AGREES TO WAIVE ANY AND ALL CLAIMS AGAINST THE UNITED STATES GOVERNMENT,
 *   ITS CONTRACTORS AND SUBCONTRACTORS, AS WELL AS ANY PRIOR RECIPIENT.  IF RECIPIENT'S USE OF THE SUBJECT SOFTWARE
 *   RESULTS IN ANY LIABILITIES, DEMANDS, DAMAGES,s
 *   EXPENSES OR LOSSES ARISING FROM SUCH USE, INCLUDING ANY DAMAGES FROM PRODUCTS BASED ON, OR RESULTING FROM,
 *   RECIPIENT'S USE OF THE SUBJECT SOFTWARE, RECIPIENT SHALL INDEMNIFY AND HOLD HARMLESS THE UNITED STATES GOVERNMENT,
 *   ITS CONTRACTORS AND SUBCONTRACTORS, AS WELL AS ANY PRIOR RECIPIENT, TO THE EXTENT PERMITTED BY LAW.
 *   RECIPIENT'S SOLE REMEDY FOR ANY SUCH MATTER SHALL BE THE IMMEDIATE, UNILATERAL TERMINATION OF THIS AGREEMENT.
 */

#include "RRT.h"
#include <iostream>
#include <fstream>
#include "ParameterData.h"
#include "SeparatedInput.h"

RRT_t::RRT_t(){
	nodeCount = 0;
	trafficSize = 0;
	xmin = 0;
	xmax = 0;
	ymin = 0;
	ymax = 0;
	zmin = 0;
	zmax = 0;
	Tstep = 5;
	dT = 1;
	closestDist = MAXDOUBLE;
	if(!DAA.parameters.loadFromFile("params/DaidalusQuadConfig.txt")){
		printf("error:no file found\n");
	}
	daaLookAhead = DAA.parameters.getLookaheadTime("s");
}

RRT_t::RRT_t(std::list<Geofence_t> &fenceList,Position initialPos,Velocity initialVel,
		std::vector<Position> trafficPos,std::vector<Velocity> trafficVel,int stepT,double dt){

	Tstep = stepT;
	dT    = dt;
	std::list<Geofence_t>::iterator it;

	proj = Projection::createProjection(initialPos.mkAlt(0));

	for(it = fenceList.begin();it != fenceList.end(); ++it){
		if(it->GetType() == KEEP_IN){
			boundingBox = it->GetPoly().poly3D(proj);
			break;
		}
	}

	xmin = -100;
	xmax = 100;
	ymin = -100;
	ymax = 100;
	zmin = -1;
	zmax = 10;

	for(it = fenceList.begin();it != fenceList.end(); ++it){
		if(it->GetType() != KEEP_IN){
			obstacleList.push_back((it->GetPoly().poly3D(proj)));
		}
	}

	Vect3 initPosR3 = proj.project(initialPos);

	nodeCount = 0;
	root.id = nodeCount;
	root.pos = initPosR3;
	root.vel = Vect3(initialVel.x,initialVel.y,initialVel.z);

	std::vector<Position>::iterator itP;
	std::vector<Velocity>::iterator itV;
	for(itP = trafficPos.begin(),itV = trafficVel.begin();
		itP != trafficPos.end(), itP != trafficPos.end();
		++itP, ++itV){
		Vect3 tPos = proj.project(*itP);
		Vect3 tVel = Vect3(itV->x,itV->y,itV->z);
		root.trafficPos.push_back(tPos);
		root.trafficVel.push_back(tVel);
	}

	trafficSize = trafficPos.size();
	nodeList.push_back(root);

	closestDist = MAXDOUBLE;
	closestNode = root;

	DAA.parameters.loadFromFile("params/DaidalusQuadConfig.txt");
	daaLookAhead = DAA.parameters.getLookaheadTime("s");
}

bool RRT_t::CheckFenceCollision(Vect3 qPos){
	std::list<Poly3D>::iterator it;;
	for(it = obstacleList.begin();it != obstacleList.end(); ++it){
		if(geoPolycarp.definitelyInside(qPos,*it)){
			return true;
		}
	}

	if(boundingBox.size() > 2){
		if(!geoPolycarp.definitelyInside(qPos,boundingBox)){
			return true;
		}
	}
	return false;
}

bool RRT_t::CheckTrafficCollision(Vect3 qPos,Vect3 qVel,
	    std::vector<Vect3> TrafficPos,std::vector<Vect3> TrafficVel,Vect3 oldVel){

	DAA.reset();
	Position so  = Position::makeXYZ(qPos.x,"m",qPos.y,"m",qPos.z,"m");
	Velocity vo  = Velocity::makeVxyz(qVel.x,qVel.y,"m/s",qVel.z,"m/s");
	Velocity vo0 = Velocity::makeVxyz(oldVel.x,oldVel.y,"m/s",oldVel.z,"m/s");
	DAA.setOwnshipState("Ownship",so,vo,0);

	std::vector<Vect3>::iterator itP;
	std::vector<Vect3>::iterator itV;
	int i=0;
	double trafficDist = MAXDOUBLE;

	for(itP = TrafficPos.begin(),itV = TrafficVel.begin();
		itP != TrafficPos.end(),itV != TrafficVel.end();
		++itP,++itV){
		Position si = Position::makeXYZ(itP->x,"m",itP->y,"m",itP->z,"m");
		Velocity vi = Velocity::makeVxyz(itV->x,itV->y,"m/s",itV->z,"m/s");
		char name[10];
		sprintf(name,"Traffic%d",i);i++;
		DAA.addTrafficState(name,si,vi);

		double distH = so.distanceH(si);

		if(distH < trafficDist){
			trafficDist = distH;
		}
	}

	//TODO: change this number to a parameter
	if(trafficDist < 8){
		printf("In cylinder\n");
		return true;
	}

	double qHeading = vo.track("degree");
	double oldHeading = vo0.track("degree");

	// Check collision with traffic based on current heading
	for(int ac = 1;ac<DAA.numberOfAircraft();ac++){
		double tlos = DAA.timeToViolation(ac);
		if(tlos >=0 && tlos <= daaLookAhead){
			DAA.kinematicMultiBands(KMB);
			for(int ib=0;ib<KMB.trackLength();++ib){
				if(KMB.trackRegion(ib) != larcfm::BandsRegion::NONE ){
					Interval ii = KMB.track(ib,"deg");
					if(qHeading > ii.low && qHeading < ii.up){
						printf("RRT:collision warning %f in [%f,%f]\n",qHeading,ii.low,ii.up);
						return true;
					}
					else{
						if(CheckTurnConflict(ii.low,ii.up,qHeading,oldHeading)){
							printf("RRT:turn conflict old:%f, new:%f [%f,%f]\n",oldHeading,qHeading,ii.low,ii.up);
							return true;
						}
					}
				}
			}
		}
	}

	// Check collision with traffic based on direct heading to
	// ensure turning to newHeading from oldHeading is conflict free
	for(itP = TrafficPos.begin(),itV = TrafficVel.begin();
		itP != TrafficPos.end(),itV != TrafficVel.end();
		++itP,++itV){
		DAA.reset();

		so          = Position::makeXYZ(qPos.x,"m",qPos.y,"m",qPos.z,"m");
		Position si = Position::makeXYZ(itP->x,"m",itP->y,"m",itP->z,"m");
		Velocity vi = Velocity::makeVxyz(itV->x,itV->y,"m/s",itV->z,"m/s");
		Vect3 AB    = Vect3(itP->x - qPos.x,itP->y - qPos.y,itP->z - qPos.z);
		AB          = AB.Scal(1/AB.norm()); //TODO: scale this vector by resolution speed
		vo          = Velocity::makeVxyz(AB.x,AB.y,"m/s",AB.z,"m/s");

		DAA.setOwnshipState("Ownship",so,vo,0);
		char name[10];
		sprintf(name,"Traffic%d",1);
		DAA.addTrafficState(name,si,vi);

		double tlos = DAA.timeToViolation(1);
		if(tlos >=0 && tlos <= daaLookAhead){
			DAA.kinematicMultiBands(KMB);
			for(int ib=0;ib<KMB.trackLength();++ib){
				if(KMB.trackRegion(ib) != larcfm::BandsRegion::NONE ){
					Interval ii = KMB.track(ib,"deg");
					if(CheckTurnConflict(ii.low,ii.up,qHeading,oldHeading)){
						printf("RRT:turn conflict old:%f, new:%f [%f,%f]\n",oldHeading,qHeading,ii.low,ii.up);
						return true;
					}
				}
			}
		}

	}
	return false;
}

bool RRT_t::CheckTurnConflict(double low,double high,double newHeading,double oldHeading){

	// Get direction of turn
	double psi   = newHeading - oldHeading;
	double psi_c = 360 - fabs(psi);
	bool leftTurn = false;
	bool rightTurn = false;
	if(psi > 0){
		if(fabs(psi) > fabs(psi_c)){
			leftTurn = true;
		}
		else{
			rightTurn = true;
		}
	}else{
		if(fabs(psi) > fabs(psi_c)){
			rightTurn = true;
		}
		else{
			leftTurn = true;
		}
	}

	// Check if turning requires crossing a conflict band
	if(psi > 0){
		if(rightTurn){
			if(oldHeading < low && newHeading > high){
				return true;
			}else{
				return false;
			}
		}else{
			if(oldHeading > high && newHeading < low){
				return true;
			}else{
				return false;
			}
		}
	}else{
		if(rightTurn){
			if(oldHeading > high && newHeading < low){
				return true;
			}else{
				return false;
			}
		}else{
			if(oldHeading < low && newHeading > high){
				return true;
			}else{
				return false;
			}
		}
	}
	return false;
}

double RRT_t::NodeDistance(node_t A, node_t B){
	return sqrt(pow((A.pos.x - B.pos.x),2) + pow((A.pos.y - B.pos.y),2));
}

void RRT_t::GetInput(node_t nn, node_t qn,double U[]){
	double dx, dy, dz;
	double norm;

	dx = qn.pos.x - nn.pos.x;
	dy = qn.pos.y - nn.pos.y;
	dz = qn.pos.z - nn.pos.z;

	norm = sqrt(pow(dx,2) + pow(dy,2) + pow(dz,2));

	if (norm > 1){
		U[0] = dx/norm;
		U[1] = dy/norm;
		U[2] = dz/norm;
	}
	else{
		U[0] = dx;
		U[1] = dy;
		U[2] = dz;
	}
}

node_t RRT_t::FindNearest(node_t query){
	double minDist = 10000;
	double dist;
	node_t nearest;

	if ( nodeList.size() == 0 ){
		nearest = query;
	}
	else{
		for(ndit = nodeList.begin();ndit != nodeList.end(); ++ndit){
			dist = NodeDistance(*ndit,query);

			if(dist < minDist){
				minDist = dist;
				nearest = *ndit;
			}
		}
	}

	return nearest;
}

void RRT_t::F(double X[], double U[],double Y[]){

	double Kc = 0.3;

	Y[0] = X[1];
	Y[1] = -Kc*(X[1] - U[0]);
	Y[2] = X[3];
	Y[3] = -Kc*(X[3] - U[1]);
	Y[4] = X[5];
	Y[5] = -Kc*(X[5] - U[2]);

	// Constant velocity for traffic
	for(int i=0;i<trafficSize;++i){
		Y[6+(6*i)+0]   = X[6+(6*i)+1];
		Y[6+(6*i)+1]   = 0;
		Y[6+(6*i)+2]   = X[6+(6*i)+3];
		Y[6+(6*i)+3]   = 0;
		Y[6+(6*i)+4]   = X[6+(6*i)+5];
		Y[6+(6*i)+5]   = 0;
	}
}

node_t RRT_t::MotionModel(Vect3 pos, Vect3 vel,
							std::vector<Vect3> trafficPos, std::vector<Vect3> trafficVel, double U[]){

	int Xsize = 6+trafficSize*6;

	double *X   = new double[Xsize];
	double *X_p = new double[Xsize];
	double *Y   = new double[Xsize];
	double *k1  = new double[Xsize];
	double *k2  = new double[Xsize];

	memset(Y,0,sizeof(double)*Xsize);
	memset(k1,0,sizeof(double)*Xsize);
	memset(k2,0,sizeof(double)*Xsize);

	X[0] = pos.x;
	X[1] = vel.x;
	X[2] = pos.y;
	X[3] = vel.y;
	X[4] = pos.z;
	X[5] = vel.z;

	std::vector<Vect3>::iterator vecItP;
	std::vector<Vect3>::iterator vecItV;
	int i=0;
	for(vecItP = trafficPos.begin(),vecItV = trafficVel.begin();
		vecItP != trafficPos.end(),vecItV != trafficVel.end();
		++vecItP,++vecItV){
		X[6+(6*i)+0] =  vecItP->x;
		X[6+(6*i)+1] =  vecItV->x;
		X[6+(6*i)+2] =  vecItP->y;
		X[6+(6*i)+3] =  vecItV->y;
		X[6+(6*i)+4] =  vecItP->z;
		X[6+(6*i)+5] =  vecItV->z;
		i++;
	}

	for(int i=0;i<Tstep;++i){
		F(X,U,Y);

		for(int j=0;j<Xsize;j++){
			k1[j] = Y[j]*dT;
			X_p[j] = X[j] + k1[j];
		}

		F(X_p,U,Y);
		for(int j=0;j<Xsize;j++){
			k2[j] = Y[j]*dT;
		}

		for(int j=0;j<Xsize;j++){
			X[j] = X[j] + 0.5*(k1[j] + k2[j]);
		}

		Vect3 newPos(X[0],X[2],X[4]);

		if(CheckFenceCollision(newPos)){
			node_t newNode;
			newNode.id = -1;
			return newNode;
		}
	}

	Vect3 newPos(X[0],X[2],X[4]);
	Vect3 newVel(X[1],X[3],X[5]);

	std::vector<Vect3> newTrafficPos;

	for(int i=0;i<trafficSize;++i){
		Vect3 newTraffic(X[6+(6*i)+0],X[6+(6*i)+2],X[6+(6*i)+4]);
		newTrafficPos.push_back(newTraffic);
	}


	if(trafficSize > 0){
		if(CheckTrafficCollision(newPos,newVel,newTrafficPos,trafficVel,vel)){
			node_t newNode;
			newNode.id = -1;
			return newNode;
		}
	}


	node_t newNode;
	nodeCount++;
	newNode.id  = nodeCount;
	newNode.pos = newPos;
	newNode.vel = newVel;
	newNode.trafficPos = newTrafficPos;
	newNode.trafficVel = trafficVel;

	delete[] X;
	delete[] X_p;
	delete[] Y;
	delete[] k1;
	delete[] k2;
	return newNode;
}

void RRT_t::Initialize(Vect3 Pos,Vect3 Vel,
						std::vector<Vect3> TrafficPos,std::vector<Vect3> TrafficVel){
	root.pos = Pos;
	root.vel = Vel;
	root.trafficPos = TrafficPos;
	root.trafficVel = TrafficVel;
	root.id = nodeCount;

	nodeList.push_back(root);
	trafficSize = TrafficPos.size();

	xmin = 0;
	xmax = 100;
	ymin = 0;
	ymax = 100;
	zmin = 0;
	zmax = 100;
}

void RRT_t::RRTStep(int i){

	double X[2];
	// Generate random number
	int rangeX = xmax - xmin;
	int rangeY = ymax - ymin;

	X[0] = xmin + (rand() % rangeX);
	X[1] = ymin + (rand() % rangeY);


	node_t rd;
	rd.pos.x = X[0];
	rd.pos.y = X[1];
	rd.pos.z = 5;    // should  be set appropriately for 3D plans

	node_t nearest = FindNearest(rd);
	node_t newNode;

	bool directPath = false;

	if(nodeCount>0){
		directPath = CheckDirectPath2Goal(closestNode);
	}

	if(!directPath){;
		double U[3];
		GetInput(nearest,rd,U);
		newNode = MotionModel(nearest.pos,nearest.vel,
									 nearest.trafficPos,nearest.trafficVel,U);
	}else{
		nodeCount++;
		newNode = goalNode;
		newNode.id = nodeCount;
	}

	if(newNode.id < 0){
		return;
	}

	nearest.children.push_back(newNode);
	newNode.parent.push_back(nearest);

	nodeList.push_back(newNode);

}

bool RRT_t::CheckDirectPath2Goal(node_t qnode){
	//TODO: add check against fences also
	//TODO: add velocity towards goal
	//TODO: change speed to a parameter
	double speed = 1;
	Vect3 A = qnode.pos;
	Vect3 B = goalNode.pos;
	Vect3 AB = B.Sub(A);
	double norm = AB.norm();
	if(norm > 0){
		AB = AB.Scal(1/norm);
	}

	if(trafficSize > 0){
		node_t parent = qnode.parent.front();
		if(CheckTrafficCollision(qnode.pos,AB,qnode.trafficPos,qnode.trafficVel,parent.vel)){
			return false;
		}
		else{
			return true;
		}
	}
	else{
		return false;
	}

}

bool RRT_t::CheckGoal(){

	node_t lastNode = nodeList.back();

	Vect3 diff = lastNode.pos.Sub(goalNode.pos);
	double mag = diff.norm();

	if(mag < closestDist){
		closestDist = mag;
		closestNode = lastNode;
	}

	if( mag < 10 ){
		return true;
	}else{
		return false;
	}
}

void RRT_t::SetGoal(Position goal){
	goalNode.pos = proj.project(goal);
}

void RRT_t::SetGoal(node_t goal){
	goalNode = goal;
}

Plan RRT_t::GetPlan(){

	double speed = 1;
	node_t node = closestNode;
	node_t parent;
	std::list<node_t> path;
	printf("Closest dist: %f\n",closestDist);
	printf("x,y:%f,%f\n",goalNode.pos.x,goalNode.pos.y);
	while(!node.parent.empty()){
		parent = node.parent.front();
		printf("x,y:%f,%f\n",parent.pos.x,parent.pos.y);
		CheckTrafficCollision(parent.pos,parent.vel,parent.trafficPos,parent.trafficVel,parent.parent.front().vel);
		path.push_front(parent);
		node = parent;
	}

	std::list<node_t>::iterator nodeIt;
	Plan newRoute;
	int count = 0;
	double ETA;
	for(nodeIt = path.begin(); nodeIt != path.end(); ++nodeIt){
		Position wp(proj.inverse(nodeIt->pos));
		if(count == 0){
			ETA = 0;
		}
		else{
			Position prevWP = newRoute.point(count-1).position();
			double distH    = wp.distanceH(prevWP);
			ETA             = ETA + distH/speed;
		}

		NavPoint np(wp,ETA);
		newRoute.add(np);
		count++;
	}

	std::cout<<newRoute.toString()<<std::endl;

	return newRoute;
}


/*
int main(int argc,char* argv[]){

	srand(time(NULL));

	RRT_t RRT;

	// create bounding box

	Poly2D box;
	box.addVertex(0,0);
	box.addVertex(100,0);
	box.addVertex(100,100);
	box.addVertex(0,100);

	Poly3D bbox(box,0,100);
	//RRT.boundingBox = bbox;

	// obstacles
	Poly2D obs2D;
	//obs2D.addVertex(34.4838,-5.08);
	//obs2D.addVertex(32.455,-13.56);
	//obs2D.addVertex(41.9216,-11.867);
	//obs2D.addVertex(30,60);
	//Poly3D obs1(obs2D,-100,100);

	//Poly2D obs2D_2;
	//obs2D_2.addVertex(30,0);
	//obs2D_2.addVertex(60,0);
	//obs2D_2.addVertex(60,30);
	//obs2D_2.addVertex(30,30);
	//Poly3D obs2(obs2D_2,-100,100);

	//RRT.obstacleList.push_back(obs1);
	//RRT.obstacleList.push_back(obs2);

	int Nsteps = 1000;

	Vect3 pos(50,0,0);
	Vect3 vel(1,0,0);
	Vect3 trafficPos1(90,0,0);
	Vect3 trafficVel1(-1,0,0);

	std::vector<Vect3> TrafficPos;
	std::vector<Vect3> TrafficVel;

	TrafficPos.push_back(trafficPos1);
	TrafficVel.push_back(trafficVel1);

	Vect3 gpos(90,90,0);
	node_t goal;
	goal.pos = gpos;


	RRT.Initialize(pos,vel,TrafficPos,TrafficVel);

	RRT.xmin = 0;
	RRT.xmax = 100;
	RRT.ymin = 0;
	RRT.ymax = 100;
	RRT.zmin = -10;
	RRT.zmax = 100;

	RRT.dT = 1;

	RRT.CheckTrafficCollision(pos,vel,TrafficPos,TrafficVel);


	for(int i=0;i<Nsteps;i++){
		RRT.RRTStep();

		if(RRT.CheckGoal(goal)){
			printf("%f,%f\n",0,0);
			break;
		}
	}

	node_t node = RRT.nodeList.back();
	node_t parent;
	while(!node.parent.empty()){
		//printf("%f,%f\n",node.pos.x,node.pos.y);
		printf("%f,%f,%f,%f\n",node.pos.x,node.pos.y,
				node.trafficPos.front().x,node.trafficPos.front().y);
		//printf("parent: %f,%f\n",node.parent.front().pos.x,node.parent.front().pos.y);
		parent = node.parent.front();
		node = parent;
	}



}
*/
