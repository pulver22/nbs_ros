#include <iostream>
#include <iterator>
#include "map.h"
#include "ray.h"
#include "newray.h"
#include "mcdmfunction.h"
#include "graphpose.h"
#include "Criteria/traveldistancecriterion.h"
# define PI           3.14159265358979323846  /* pi */
#include <unistd.h>
#include <ros/ros.h>
#include "movebasegoal.h"
#include <geometry_msgs/PointStamped.h>
#include <geometry_msgs/PoseStamped.h>
#include <tf/transform_listener.h>
#include <nav_msgs/GetMap.h>
#include <costmap_2d/costmap_2d_ros.h>



using namespace std;
using namespace dummy;

typedef actionlib::SimpleActionClient<move_base_msgs::MoveBaseAction> MoveBaseClient;

vector<int> occdata;
int costmapReceived = 0;
float costresolution;
int costwidth;
int costheight;
geometry_msgs::Pose costorigin;
nav_msgs::OccupancyGrid costmap_grid;

geometry_msgs::PoseStamped getCurrentPose();
void move(int x, int y, double orW, double orZ);
void update_callback(const map_msgs::OccupancyGridUpdateConstPtr& msg);
void grid_callback(const nav_msgs::OccupancyGridConstPtr& msg);
int getIndex(int x, int y);




int main(int argc, char **argv) {
    ros::init(argc, argv, "mcdm_exploration_framework_node");
    ros::NodeHandle nh;
    ros::ServiceClient map_service_client_ = nh.serviceClient<nav_msgs::GetMap>("static_map");
    nav_msgs::GetMap srv_map;
    ros::Publisher grid_pub = nh.advertise<nav_msgs::OccupancyGrid>("mcdm_grid", 1000);
    MoveBaseGoal goal;
    ros::Publisher moveBasePub = nh.advertise<geometry_msgs::PoseStamped>("move_base_simple/goal",1000);
    MoveBaseClient ac("move_base", true);
    ros::Subscriber costmap_sub;
    ros::Subscriber costmap_update_sub;
    
    while(!ac.waitForServer(ros::Duration(5.0))){
    ROS_INFO("Waiting for the move_base action server to come up");
    }
    // Input :  15 180 0.95 0.12
    // range centralAngle precision threshold
    
    ros::Rate r(10);
    
    while(ros::ok()){
    
    //if (map_service_client_.call(srv_map)){
	
	costmap_sub = nh.subscribe<nav_msgs::OccupancyGrid>("move_base/global_costmap/costmap", 100, grid_callback);
	costmap_update_sub = nh.subscribe<map_msgs::OccupancyGridUpdate>("move_base/global_costmap/costmap_updates", 10, update_callback);

	if(costmapReceived == 0) {
	    //ROS_INFO_STREAM( "waiting for costmap" << std::endl);
	    cout << "Waiting for costmap" << endl;
	}

	if(costmapReceived == 1)
	{
	/*
	ROS_INFO("Map service called successfully");
	const nav_msgs::OccupancyGrid& map (srv_map.response.map);
	float resolution = map.info.resolution;
	int width = map.info.width;
	int height = map.info.height;
	geometry_msgs::Pose origin = map.info.origin;
	tf::Transform transform;
	transform.setOrigin( tf::Vector3(origin.position.x, origin.position.y, origin.position.z) );
	cout << origin.position.x << "," << origin.position.y << endl;
	transform.setRotation(tf::Quaternion(origin.orientation.x,origin.orientation.y,origin.orientation.z,origin.orientation.w));
	
	vector<int> data;
	for(int i=0; i < map.data.size(); i++){
	    data.push_back(map.data.at(i));
	}
	*/
	Map newMap = Map(costresolution,costwidth,costheight,occdata,costorigin);
	ros::Publisher marker_pub = nh.advertise<geometry_msgs::PointStamped>("goal_pt", 10);
	
    
	/*NOTE: Transform between map and image, to be enabled if not present in the launch file
	//tf::Transform tranMapToImage;
	//tranMapToImage.setOrigin(tf::Vector3(0, 30, 0.0));
	//tf::Vector3 vecImageToMap = tf::Vector3(0, 30,0.0);
	*/	
	
	// Get the initial pose in map frame
	geometry_msgs::PoseStamped start_pose;
	start_pose = getCurrentPose();
	double initX = start_pose.pose.position.x;
	double initY = start_pose.pose.position.y;
	tf::Quaternion quat = tf::Quaternion(start_pose.pose.orientation.x,start_pose.pose.orientation.y,start_pose.pose.orientation.z,start_pose.pose.orientation.w);
	
	cout <<start_pose.pose.orientation.x <<","<< start_pose.pose.orientation.y <<","<< start_pose.pose.orientation.z<< ","<< start_pose.pose.orientation.w <<endl; 
	
	tfScalar angle = 2 * acos(quat[3]);
	cout << "Initial position in the map frame:" << initX << "," << initY <<" with orientation :" << angle << endl;
	
	int initOrientation = (int)(angle * 180/PI);
	cout << "Orientation after casting: " << initOrientation << endl;
	
	
	/*
	// NOTE: TO ADJUST ORIENTATION ACCORDING TO A MAP
	if(initOrientation <= 45 || initOrientation > 315) initOrientation = 0;
	if(initOrientation > 45 && initOrientation <= 135) initOrientation = 90;
	if(initOrientation >135 && initOrientation <= 225) initOrientation = 180;
	if(initOrientation > 225 && initOrientation <= 315) initOrientation = 270;
	*/
	
	double initFov = atoi(argv[1] );
	initFov = initFov * PI /180;
	int initRange = atoi(argv[2]);
	double precision = atof(argv[3]);
	double threshold = atof(argv[4]);
	
	
	//convert from map frame to image
	tf::Vector3 pose = tf::Vector3(initX,initY,0.0);
	//pose = tranMapToImage.operator*(pose);
	//pose = pose /resolution;
	//pose = transform.operator*(pose);
	
	//NOTE: Y in map are X in image
	Pose initialPose = Pose(newMap.getNumGridRows() - (long)pose.getY(), (long)pose.getX(),initOrientation,initRange,initFov);
	Pose target = initialPose;
	Pose previous = initialPose;
	
	cout << "Initial position in the image frame: " << target.getY() << "," << target.getX() << endl;
	
	
	
	//----------------- PRINT INITIAL POSITION
	//ATTENTION: doesn't work! ...anyway the initial position is represented by the robot
	geometry_msgs::PointStamped p;
	p.header.frame_id = "map";
	p.header.stamp = ros::Time::now();
	//NOTE: as before, Y in map are X in image
	p.point.x = ( newMap.getNumGridRows() -  target.getX() )* costresolution;
	p.point.y = (target.getY() )* costresolution;
	
	//cout << p.point.x << ","<< p.point.y << endl;
	
	tf::Vector3 vec =  tf::Vector3(p.point.x,p.point.y,0.0);
	//vec = transform.operator*(vec);
	
	p.point.x = vec.getY() ;
	p.point.y = vec.getX() ;
	
	//cout << p.point.x << ","<< p.point.y << endl;
	marker_pub.publish(p);
	//----------------------------------------

	
	long numConfiguration =0;
	//testing
	vector<pair<string,list<Pose>>> graph2;
	NewRay ray;
	MCDMFunction function;
	long sensedCells = 0;
	long newSensedCells =0;
	long totalFreeCells = newMap.getTotalFreeCells() ;
	int count = 0;
	int countBT;
	double travelledDistance = 0;
	unordered_map<string,int> visitedCell;
	vector<string>history;
	history.push_back(function.getEncodedKey(target,1));
	//amount of time the robot should do nothing for scanning the environment ( final value expressed in second)
	unsigned int microseconds = 5 * 1000 * 1000 ;
	//cout << "total free cells in the main: " << totalFreeCells << endl;
	list<Pose> unexploredFrontiers;
	while(sensedCells < precision * totalFreeCells ){
	long x = target.getX();
	long y = target.getY();
	int orientation = target.getOrientation();
	int range = target.getRange();
	double FOV = target.getFOV();
	string actualPose = function.getEncodedKey(target,0);
	newMap.setCurrentPose(target);
	travelledDistance = travelledDistance + target.getDistance(previous);
	string encoding = to_string(target.getX()) + to_string(target.getY());
	visitedCell.emplace(encoding,0);
	
	
	cout << "-----------------------------------------------------------------"<<endl;
	cout << "Round : " << count<< endl;
	newSensedCells = sensedCells + ray.getInformationGain(newMap,x,y,orientation,FOV,range);
	cout << "Area sensed: " << newSensedCells << " / " << totalFreeCells<< endl;

	ray.performSensingOperation(newMap,x,y,orientation,FOV,range);
	ray.findCandidatePositions(newMap,x,y,orientation,FOV,range);
	vector<pair<long,long> >candidatePosition = ray.getCandidatePositions();
	ray.emptyCandidatePositions();
	
	/*
	//NOTE: PRINT THE MAP NEAR THE ROBOT-------------
	int curX = previous.getX();
	int curY = previous.getY();
	int minX = curX - 30;
	if(minX < 0) minX = 0;
	int maxX = curX + 30;
	if(maxX > newMap.getNumGridRows()-1) maxX= newMap.getNumGridRows()-1;
	int minY = curY - 30;
	if(minY < 0) minY = 0;
	int maxY = curY + 30;
	if(maxY > newMap.getNumGridCols()-1) maxY = newMap.getNumGridCols()-1;
	//print portion of the map
	    for(int i = minX; i < maxX; ++i)
	    {
		for(int j = minY; j < maxY; ++j)
		{
		    if(i == curX && j == curY)
		    {
			std::cout << "X ";
		    }
		    else if (i == target.getX() && j == target.getY())
		    {
			std::cout << "Y ";
		    }
		    else std::cout << newMap.getGridValue(i, j) << " ";
		}
		std::cout << std::endl;
	    }
	    std::cout << std::endl;
	//---------------------------------------
	*/
	
	//-------------------
	//STAMP ACTUAL ORIENTATION IN MAP FRAME
	geometry_msgs::PoseStamped current_pose;
	current_pose = getCurrentPose();
	tfScalar angle2 = 2 * acos(current_pose.pose.orientation.w);
	
	int currentOrientation = (int)(angle2 * 180 / PI);
	cout << "Orientation after casting of current pose: " << currentOrientation << endl;
	//-----------------------
	
	
	if(candidatePosition.size() == 0) {
	    
	    cout << "No other candidate position" << endl;
	    cout << "----- BACKTRACKING -----" << endl;
	    
	    
	    
	    if (graph2.size() >0){
		
		
		// OLD METHOD
		graph2.pop_back();
		string targetString = graph2.at(graph2.size()-1).first;
		graph2.pop_back();
		EvaluationRecords record;
		target = record.getPoseFromEncoding(targetString);
		history.push_back(function.getEncodedKey(target,2));
		cout << "[BT]No significative position reachable. Come back to previous position" << endl;
		cout << "New target: x = " << target.getY() << ",y = " << target.getX() <<", orientation = " << target.getOrientation() << endl;
		count = count + 1;
		cout << "Graph dimension : " << graph2.size() << endl;
		
		
	    } else {
		cout << "-----------------------------------------------------------------"<<endl;
		cout << "I came back to the original position since i don't have any other candidate position"<< endl;
		cout << "Total cell visited :" << numConfiguration <<endl;
		cout << "FINAL: Map not completely explored!" << endl;
		cout << "-----------------------------------------------------------------"<<endl;
		exit(0);
	    }
	    
	    sensedCells = newSensedCells;
	}else{
	    
	    
	    // need to convert from a <int,int pair> to a Pose with also orientation,laser range and angle
	    list<Pose> frontiers;
	    vector<pair<long,long> >::iterator it =candidatePosition.begin();
	    for(it; it != candidatePosition.end(); it++){
		Pose p1 = Pose((*it).first,(*it).second,0 ,range,FOV);
		Pose p2 = Pose((*it).first,(*it).second,180,range,FOV);
		Pose p3 = Pose((*it).first,(*it).second,90,range,FOV);
		Pose p4 = Pose((*it).first,(*it).second,270,range,FOV);
		frontiers.push_back(p1);
		frontiers.push_back(p2);
		frontiers.push_back(p3);
		frontiers.push_back(p4);
	    }
	    
	    unexploredFrontiers = frontiers;
	    
	    //cout << "Graph dimension : " << graph2.size() << endl;
	    //cout << "Candidate position: " << candidatePosition.size() << endl;
	    cout <<"Frontiers: "<<  frontiers.size() << endl;
	    EvaluationRecords *record = function.evaluateFrontiers(frontiers,newMap,threshold);
	    //cout << "Record: " << record->size() << endl;
	    //cout << "Evaluation Record obtained" << endl;
	    
	    
	    if(record->size() != 0){
		//set the previous pose equal to the actual one(actually represented by target)
		previous = target;
		std::pair<string,list<Pose>> pair = make_pair(actualPose,frontiers);
		graph2.push_back(pair);
		std::pair<Pose,double> result = function.selectNewPose(record);
		target = result.first;
		if (!target.isEqual(previous)){
		    count = count + 1;
		    countBT = graph2.size();
		    numConfiguration++;
		    history.push_back(function.getEncodedKey(target,1));
		    cout << "Graph dimension : " << graph2.size() << endl;
		    //cout << record->size() << endl;
		}else{
		    cout << "[BT]Cell already explored!Come back to previous position";
		    countBT = countBT -2;
		    string targetString = graph2.at(countBT).first;
		    target = record->getPoseFromEncoding(targetString);
		    graph2.pop_back();
		    history.push_back(function.getEncodedKey(target,2));
		    cout << "New target: x = " << target.getY() << ",y = " << target.getX() <<", orientation = " << target.getOrientation() << endl;
		    count = count + 1;
		}
	    }else {  
		    
		    //OLD METHOD
		    
		    if(graph2.size() == 0) break;
		    
		   
		    string targetString = graph2.at(graph2.size()-1).first;
		    target = record->getPoseFromEncoding(targetString);
		    graph2.pop_back();
		    if(!target.isEqual(previous)){
			previous = target;
			cout << "[BT]No significative position reachable. Come back to previous position" << endl;
			history.push_back(function.getEncodedKey(target,2));
			cout << "New target: x = " << target.getY() << ",y = " << target.getX() <<", orientation = " << target.getOrientation() << endl;
			count = count + 1;
		    }else {
			if(graph2.size() == 0 ) {
			    cout << "No other possibilities to do backtracking on previous positions" << endl;
			    break;
			}
			string targetString = graph2.at(graph2.size()-1).first;
			target = record->getPoseFromEncoding(targetString);
			graph2.pop_back();
			previous = target;
			cout << "[BT]No significative position reachable. Come back to previous position" << endl;
			history.push_back(function.getEncodedKey(target,2));
			cout << "New target: x = " << target.getY() << ",y = " << target.getX() <<", orientation = " << target.getOrientation() << endl;
			count = count + 1;
		    }
		    
	    }
    
	    sensedCells = newSensedCells;
	    
	    
	    //---------------------------PRINT GOAL POSITION
	    geometry_msgs::PointStamped p;
	    p.header.frame_id = "map";
	    p.header.stamp = ros::Time::now();
	    //NOTE: as before, Y in map are X in image
	    p.point.x = (newMap.getNumGridRows() - target.getX() ); //* resolution;
	    p.point.y = (target.getY() ) ;//* resolution;
	    
	    //cout << p.point.x << ","<< p.point.y << endl;
	    
	    tf::Vector3 vec =  tf::Vector3(p.point.x,p.point.y,0.0);

	    //vec = transform.operator*(vec);
	    
	    p.point.x = vec.getY() ;
	    p.point.y = vec.getX() ;
	    
	    //cout << p.point.x << ","<< p.point.y << endl;
	    
	    //NOTE: not requested for testing purpose
	    //usleep(microseconds);
	    marker_pub.publish(p);
	    
	    //--------------------------------------
	    
	    
	    cout << "Goal in image: " << target.getY() << "," << target.getX() << endl;
	    tf::Vector3 goal_pose = tf::Vector3(target.getY(),target.getX(),0.0);
	    //cout << "1)" <<goal_pose.getX() << "," << goal_pose.getY() << endl;
	    
	    goal_pose = goal_pose ;//* resolution;
	    //cout << "2)" <<goal_pose.getX() << "," << goal_pose.getY() << endl;
	    
	    //goal_pose = goal_pose.operator-=(vecImageToMap);
	    //cout << "3)" <<goal_pose.getX() << "," << goal_pose.getY() << endl;
	    
	    cout << "Goal in map: " << goal_pose.getX() <<","<< goal_pose.getY() << endl;
	    
	    
	    move_base_msgs::MoveBaseGoal goal;
	    
	    /* 
	    //NOTE: DEBUG IF THE MOVE METHOD DOESN'T MOVE
	    goal.target_pose.header.frame_id = "map";
	    goal.target_pose.header.stamp = ros::Time::now();
	    goal.target_pose.pose.position.x = p.point.x;
	    goal.target_pose.pose.position.y = p.point.y;
	    //goal.target_pose.pose.position.x = goal_pose.getX();
	    //goal.target_pose.pose.position.y = goal_pose.getY();
	    goal.target_pose.pose.orientation.w = cos(PI - target.getOrientation()/2);
	    goal.target_pose.pose.orientation.z = sin(PI - target.getOrientation()/2);
	    
	    ROS_INFO("Sending goal");
	    ac.sendGoal(goal);
	    ac.waitForResult();

	    if(ac.getState() == actionlib::SimpleClientGoalState::SUCCEEDED)
		ROS_INFO("I'm moving...");
	    else
		ROS_INFO("The base failed to move");
	    */
	    double orientZ = (double)(target.getOrientation()* PI/(2*180));
	    double orientW =  (double)(target.getOrientation()* PI/(2 * 180));
	    move(p.point.x,p.point.y, sin(orientZ), cos(orientW));
	  
	    /* NOTE: DEBUG WITH RED ARROW
	    geometry_msgs::PoseStamped goal;
	    goal.header.frame_id = "map";
	    goal.header.stamp = ros::Time::now();
	    goal.pose.position.x = goal_pose.getX();
	    goal.pose.position.y = goal_pose.getY();
	    goal.pose.orientation.w = cos(target.getOrientation()/2);
	    goal.pose.orientation.z = sin(target.getOrientation()/2);
	    moveBasePub.publish<geometry_msgs::PoseStamped>(goal);
	    */
	    sensedCells = newSensedCells;
	    sleep(2);
	    frontiers.clear();
	    candidatePosition.clear();
	    delete record;
	    ros::spinOnce();
	    r.sleep();
	    }
	}
    
	newMap.drawVisitedCells(visitedCell,costresolution);
	newMap.printVisitedCells(history);


	 if (graph2.size() != 0 && sensedCells >= precision * totalFreeCells ){
	    cout << "-----------------------------------------------------------------"<<endl;
	    cout << "Total cell visited :" << numConfiguration <<endl;
	    cout << "Total travelled distance (cells): " << travelledDistance << endl;
	    cout << "FINAL: MAP EXPLORED!" << endl;
	    cout << "-----------------------------------------------------------------"<<endl;
	}else{
	    cout << "-----------------------------------------------------------------"<<endl;
	    cout << "I came back to the original position since i don't have any other candidate position"<< endl;
	    cout << "Total cell visited :" << numConfiguration <<endl;
	    cout << "-----------------------------------------------------------------"<<endl;    
	}
	return 1;
    }
	
	/*
    }else{
	ROS_ERROR("Failed to call map service");
	return 0;
    }*/
	
    cout << "Spinning at the end" << endl;
    ros::spinOnce();
    r.sleep();
    }
    
    
}



geometry_msgs::PoseStamped getCurrentPose()
{
    ros::Time _now_stamp_ = ros::Time(0);
   
    tf::StampedTransform start_pose_in_tf;
    tf::TransformListener _tf_listener;
   
    _tf_listener.waitForTransform("map", "base_link", _now_stamp_, ros::Duration(2.0));
    try
    {
        _tf_listener.lookupTransform("map", "base_link", _now_stamp_, start_pose_in_tf);
    }
    catch(tf::TransformException& ex)
    {
        ROS_INFO("TRANSFORMS ARE COCKED-UP PAL! Why is that :=> %s", ex.what());
    }
   
    tf::Vector3 start_position = start_pose_in_tf.getOrigin();
    tf::Quaternion start_orientation = start_pose_in_tf.getRotation();

    geometry_msgs::PoseStamped start_pose;
    start_pose.header.stamp = start_pose_in_tf.stamp_;
    start_pose.header.frame_id = start_pose_in_tf.frame_id_;

    tf::pointTFToMsg(start_position, start_pose.pose.position);
    tf::quaternionTFToMsg(start_orientation, start_pose.pose.orientation);
    
    return start_pose;
}

void grid_callback(const nav_msgs::OccupancyGridConstPtr& msg)
{
    //cout << "alive" << endl;
  if(costmapReceived == 0)
  {
    std::cout << "CALLBACK FIRST" << std::endl;
    //costmap_grid = msg.get();
    costresolution = msg->info.resolution;
    costwidth = msg->info.width;
    costheight = msg->info.height;
    costorigin = msg->info.origin;
    for(int i = 0; i < msg.get()->data.size(); ++i)
    {
      occdata.push_back(msg->data.at(i));
    }
    std::cout << "size of occdata " << occdata.size() << " size of message data " << msg->data.size() << std::endl;
    std::cout << "height" << msg->info.height << " width " << msg->info.width << " resolution " << msg->info.resolution << std::endl;
    costmapReceived = 1;
  }
  
}

void update_callback(const map_msgs::OccupancyGridUpdateConstPtr& msg)
{
	//NOTE: everything is commented because we don't want update the costmap since the environment is 
	//assumed static

	//std::cout << "CALLBACK SECOND" << std::endl;
        
	/*int index = 0;
        for(int y=msg->y; y< msg->y+msg->height; y++) {
                for(int x=msg->x; x< msg->x+msg->width; x++) {
                        costmap_grid.data[ getIndex(x,y) ] = msg->data[ index++ ];
                }
        }*/
}

int getIndex(int x, int y){
        int sx = costmap_grid.info.width;
        return y * sx + x;
}


void move(int x, int y, double orZ, double orW){
    move_base_msgs::MoveBaseGoal goal;

    MoveBaseClient ac ("move_base", true);
    //we'll send a goal to the robot to move 1 meter forward
    goal.target_pose.header.frame_id = "map";
    goal.target_pose.header.stamp = ros::Time::now();

    goal.target_pose.pose.position.x = x;
    goal.target_pose.pose.position.y = y;
    goal.target_pose.pose.orientation.z = orZ;
    goal.target_pose.pose.orientation.w = orW;
  
    ROS_INFO("Sending goal");
    ac.sendGoal(goal);

    ac.waitForResult();

    if(ac.getState() == actionlib::SimpleClientGoalState::SUCCEEDED)
	ROS_INFO("I'm moving...");
    else
	ROS_INFO("The base failed to move");
}

