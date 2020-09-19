#include"semantic_mesh_loam/laser_odometry.h"


namespace semloam{

	LaserOdometry::LaserOdometry(void){

		scanperiod = 0.1;
		ioratio = 2;
		max_iterations = 25;
		delta_t_abort = 0.1;
		delta_r_abort = 0.1;

		
		velo_scans.reserve(scan_size);
		CloudCentroid.reserve(feature_size);
		CloudEdge.reserve(feature_size);
		FeatureCloud.reserve(feature_size);
		
		//_lastCloudCentroid.reserve(feature_size);
		//_lastCloudEdge.reserve(feature_size);
		_lastFeatureCloud.reserve(feature_size);

		//CloudCentroidInd.reserve(ind_size);
		//_lastCloudCentroidInd.reserve(ind_size);

		//CloudEdgeInd.reserve(ind_size);
		//_lastCloudEdgeInd.reserve(ind_size);

		tmp_pc_stored.reserve(feature_size);

		odom_data.header.frame_id = "map";
		odom_data.child_frame_id = "vehicle";

		_last_odom_data.header.frame_id = "map";
		_last_odom_data.child_frame_id = "vehicle";

		laserodometry.header.frame_id = "map";
		laserodometry.child_frame_id = "laserodometry";

		/*
		odometrytrans.header.frame_id = "map";
		odometrytrans.child_frame_id = "vehicle";
		*/

		system_initialized_checker = false;

	}

	bool LaserOdometry::setup(ros::NodeHandle &node, ros::NodeHandle &privateNode){

		float fparam;
		int iparam;

		if(privateNode.getParam("__systemdelay", iparam)){
			if(iparam < 1){
				ROS_ERROR("Invalid __systemdelay parameter: %d",iparam);
				return false;
			}
			else{
				__systemdelay = iparam;
			}
		}
		
		if(privateNode.getParam("MaxCorrespondDistance", fparam)){
			if(fparam < 0.0){
				ROS_ERROR("Invalid MaxCorrespondDistance parameter: %f",fparam);
				return false;
			}
			else{
				MaxCorrespondDistance = fparam;
			}
		}
		
		if(privateNode.getParam("MaximumIterations", iparam)){
			if(iparam < 0){
				ROS_ERROR("Invalid MaximumIterations parameter: %d",iparam);
				return false;
			}
			else{
				MaximumIterations = iparam;
			}
		}
		
		if(privateNode.getParam("TransformationEpsilon", fparam)){
			if(fparam < 0.0){
				ROS_ERROR("Invalid TransformationEpsilon parameter: %f",fparam);
				return false;
			}
			else{
				TransformationEpsilon = fparam;
			}
		}

		if(privateNode.getParam("EuclideanFitnessEpsilon", fparam)){
			if(fparam < 0.0){
				ROS_ERROR("Invalid EuclideanFitnessEpsilon parameter: %f",fparam);
				return false;
			}
			else{
				EuclideanFitnessEpsilon = fparam;
			}
		}

		if(privateNode.getParam("scanperiod", fparam)){
			if(fparam <= 0){
				ROS_ERROR("Invalid scanperiod parameter: %f",fparam);
				return false;
			}
			else{
				scanperiod = fparam;
			}
		}

		if(privateNode.getParam("ioratio", iparam)){
			if(iparam < 1){
				ROS_ERROR("Invalid ioratio parameter: %d", iparam);
				return false;
			}
			else{
				ioratio = iparam;
			}
		}

		if(privateNode.getParam("maxiterations", iparam)){
			if(iparam < 1){
				ROS_ERROR("Invalid maxiteration parameter: %d", iparam);
				return false;
			}
			else{
				max_iterations = iparam;
			}
		}

		if(privateNode.getParam("deltaTabort", fparam)){
			if(fparam <= 0.0){
				ROS_ERROR("Invalid deltaTabort parameter: %f", fparam);
				return false;
			}
			else{
				delta_t_abort = fparam;
			}
		}

		if(privateNode.getParam("deltaRabort", fparam)){
			if(fparam <= 0.0){
				ROS_ERROR("Invalid deltaRabort parameter: %f", fparam);
				return false;
			}
			else{
				delta_r_abort = fparam;
			}
		}

		_pubLaserOdomToInit = node.advertise<nav_msgs::Odometry>("/laser_odom_to_init", 5);
		_pubVelodynePoints3 = node.advertise<sensor_msgs::PointCloud2>("/velodyne_points3", 2);
		_pubCentroidPointLast = node.advertise<sensor_msgs::PointCloud2>("/centroid_point_last", 2);
		_pubEdgePointLast = node.advertise<sensor_msgs::PointCloud2>("/edge_point_last", 2);

		//_pubFeaturePoints3 = node.advertise<sensor_msgs::PointCloud2>("/feature_points3", 2);

		//subscribe
		_subVelodynePoints = node.subscribe<sensor_msgs::PointCloud2>
			("/velodyne_points2", 2, &LaserOdometry::velodyne_callback, this);
		_subCentroid = node.subscribe<sensor_msgs::PointCloud2>
			("/centroid_points", 2, &LaserOdometry::centroid_callback, this);
		_subEdge = node.subscribe<sensor_msgs::PointCloud2>
			("/edge_points", 2, &LaserOdometry::edge_callback, this);
		_subOdometry = node.subscribe<nav_msgs::Odometry>
			("/odometry2", 2, &LaserOdometry::odometry_callback, this);


		return true;
	}

	void LaserOdometry::velodyne_callback(const sensor_msgs::PointCloud2ConstPtr& clouddata){
		velo_scans_time = clouddata->header.stamp;

		std::cout << "velo_scans_time: " << velo_scans_time << std::endl;
		
		pcl::fromROSMsg(*clouddata, velo_scans);

		std::cout << "velo data catched" << std::endl;

		velo_scans_checker = true;
	}

	void LaserOdometry::centroid_callback(const sensor_msgs::PointCloud2ConstPtr& centroiddata){
		CloudCentroid_time = centroiddata->header.stamp;

		pcl::fromROSMsg(*centroiddata, CloudCentroid);

		FeatureCloud = FeatureCloud + CloudCentroid;

		std::cout << "catch centroid data" << std::endl;

		CloudCentroid_checker = true;
	}

	void LaserOdometry::edge_callback(const sensor_msgs::PointCloud2ConstPtr& edgedata){
		CloudEdge_time = edgedata->header.stamp;

		pcl::fromROSMsg(*edgedata, CloudEdge);

		FeatureCloud = FeatureCloud + CloudEdge;

		std::cout << "catch edge data" << std::endl;

		CloudEdge_checker = true;
	}

	void LaserOdometry::odometry_callback(const nav_msgs::OdometryConstPtr& odomdata){
		
		_last_odom_data_time = odom_data_time;

		odom_data_time = odomdata->header.stamp;

		std::cout << "odom_data_time: " << odom_data_time << std::endl;

		odom_data = *odomdata;

		std::cout << "catch odom data" << std::endl;

		odom_data_checker = true;
	}

	bool LaserOdometry::hasNewData(){

		bool checker_in = false;

		if(             velo_scans_checker == true
				&& CloudCentroid_checker == true
				&& CloudEdge_checker == true
				&& odom_data_checker == true)
		{	
			checker_in = true;
		}

		/*
		if(             (fabs(velo_scans_time - CloudCentroid_time) < 0.01)
				&& (fabs(velo_scans_time - CloudEdge_time) < 0.01)
				&& (fabs(velo_scans_time - odom_data_time) < 0.01))
		{
			checker_in = true;
		}
		else{
			checker_in = false;
		}*/

		return checker_in;
	}

	void LaserOdometry::get_relative_trans(){

		relative_pos_trans.dx = odom_data.pose.pose.position.x - _last_odom_data.pose.pose.position.x;
		relative_pos_trans.dy = odom_data.pose.pose.position.y - _last_odom_data.pose.pose.position.y;
		relative_pos_trans.dz = odom_data.pose.pose.position.z - _last_odom_data.pose.pose.position.z;

		relative_pos_trans.dqx = odom_data.pose.pose.orientation.x - _last_odom_data.pose.pose.orientation.x;
		relative_pos_trans.dqy = odom_data.pose.pose.orientation.y - _last_odom_data.pose.pose.orientation.y;
		relative_pos_trans.dqz = odom_data.pose.pose.orientation.z - _last_odom_data.pose.pose.orientation.z;
		relative_pos_trans.dqw = odom_data.pose.pose.orientation.w - _last_odom_data.pose.pose.orientation.w;

	}

	Eigen::Matrix4f LaserOdometry::init_pc_slide(){

		geometry_msgs::Transform trans_pose;
		
		trans_pose.translation.x = relative_pos_trans.dx;
		trans_pose.translation.y = relative_pos_trans.dy;
		trans_pose.translation.z = relative_pos_trans.dz;

		trans_pose.rotation.x = relative_pos_trans.dqx;
		trans_pose.rotation.y = relative_pos_trans.dqy;
		trans_pose.rotation.z = relative_pos_trans.dqz;
		trans_pose.rotation.w = relative_pos_trans.dqw;

		tf::Transform slide_transform;
		transformMsgToTF( trans_pose , slide_transform );

		pcl_ros::transformPointCloud( FeatureCloud, FeatureCloud, slide_transform );

		Eigen::Matrix4f init_slide_matrix;

		pcl_ros::transformAsMatrix( slide_transform , init_slide_matrix );

		return init_slide_matrix;
	}

	Eigen::Matrix4f LaserOdometry::pcl_pc_slide(){

		pcl::IterativeClosestPoint<pcl::PointXYZRGB, pcl::PointXYZRGB> icp;

		pcl::PointCloud<pcl::PointXYZRGB>::Ptr FeatureCloud_Ptr(new pcl::PointCloud<pcl::PointXYZRGB>( FeatureCloud ) );
		pcl::PointCloud<pcl::PointXYZRGB>::Ptr _lastFeatureCloud_Ptr(new pcl::PointCloud<pcl::PointXYZRGB>( _lastFeatureCloud ) );


		//Set the input source(current scan) and target(previous scan) pointcloud
		//icp.setInputCloud( FeatureCloud_Ptr); // only ::Ptr
		icp.setInputSource( FeatureCloud_Ptr );
		icp.setInputTarget( _lastFeatureCloud_Ptr );

		// Set the max correspondence distance to 15cm (e.g., correspondences with higher distances will be ignored)
		icp.setMaxCorrespondenceDistance (0.15);

		// Set the maximum number of iterations (criterion 1)
		icp.setMaximumIterations (50);
		// Set the transformation epsilon (criterion 2)
		icp.setTransformationEpsilon (1e-8);
		// Set the euclidean distance difference epsilon (criterion 3)
		icp.setEuclideanFitnessEpsilon (1);

		icp.align( tmp_pc_stored );

		Eigen::Matrix4f pcl_slide_matrix = icp.getFinalTransformation();

		return pcl_slide_matrix;
	}



	void LaserOdometry::get_tf_data(){

		while(true){

			try{
				listener.lookupTransform("map", "velodyne", _last_odom_data_time, velo_to_map);
				listener.lookupTransform("map", "laserodometry", _last_odom_data_time, laserodometry_to_map);
				ROS_INFO("GET TRANSFORM MAP FRAME AND VELODYNE FRAME");
				break;
			}
			catch(tf::TransformException ex){
				ROS_ERROR("%s", ex.what() );
				ros::Duration(1.0).sleep();
			}
		}

		std::cout << "get transform" << std::endl;

	}

	
	void LaserOdometry::convert_coordinate_of_pc(){

		//Transforming current scans
		// sitahairanai?
		//pcl_ros::transformPointCloud("map", _last_odom_data_time, velo_scans, "laserodometry", velo_scans, listener);
		//pcl_ros::transformPointCloud("map", _last_odom_data_time, CloudCentroid, "laserodometry", CloudCentroid, listener);
		//pcl_ros::transformPointCloud("map", _last_odom_data_time, CloudEdge, "laserodometry", CloudEdge, listener);

		pcl_ros::transformPointCloud("map", _last_odom_data_time, FeatureCloud, "laserodometry", FeatureCloud, listener);

	}

	bool LaserOdometry::initialization(){
		//CloudCentroid.swap(_lastCloudCentroid);
		//CloudEdge.swap(_lastCloudEdge);
		FeatureCloud.swap( _lastFeatureCloud );

		_last_CloudCentroid_time = CloudCentroid_time;
		_last_CloudEdge_time = CloudEdge_time;

		_last_odom_data = odom_data;
		_last_odom_data_time = odom_data_time;

		// getting laser odometry's init parameter
		laserodometry.pose.pose.position = _last_odom_data.pose.pose.position;
		laserodometry.pose.pose.orientation = _last_odom_data.pose.pose.orientation;
		laserodometry.twist.twist.linear = _last_odom_data.twist.twist.linear;
		laserodometry.twist.twist.angular = _last_odom_data.twist.twist.angular;

		geometry_msgs::Pose init_pose;
		init_pose.position = laserodometry.pose.pose.position;
		init_pose.orientation = laserodometry.pose.pose.orientation;
		
		tf::Transform init_transform;
		poseMsgToTF(init_pose, init_transform);
		br.sendTransform(tf::StampedTransform(init_transform, _last_odom_data_time, "map", "laserodometry"));

		while(true){
			try{
				listener.lookupTransform("map", "laserodometry", _last_odom_data_time, laserodometry_to_map);
				ROS_INFO("GET TRANSFORM MAP FRAME AND VELODYNE FRAME");
				break;
			}
			catch(tf::TransformException ex){
				ROS_ERROR("%s", ex.what() );
				ros::Duration(1.0).sleep();
			}
		}

		//Transform previous scans
		//pcl_ros::transformPointCloud("map", _last_odom_data_time, _lastCloudCentroid, "laserodometry", _lastCloudCentroid, listener);
		//pcl_ros::transformPointCloud("map", _last_odom_data_time, _lastCloudEdge, "laserodometry", _lastCloudEdge, listener);
		pcl_ros::transformPointCloud("map", _last_odom_data_time, _lastFeatureCloud, "laserodometry", _lastFeatureCloud, listener );

		return true;

	}

	void LaserOdometry::send_tf_data(Eigen::Matrix4f Tm){
		
		geometry_msgs::Transform slide;

		slide.translation.x = Tm(0, 3);
		slide.translation.y = Tm(1, 3);
		slide.translation.z = Tm(2, 3);

		tf::Matrix3x3 tf3d;
		tf3d.setValue(static_cast<double>(Tm(0,0)), static_cast<double>(Tm(0,1)), static_cast<double>(Tm(0,2)), 
				static_cast<double>(Tm(1,0)), static_cast<double>(Tm(1,1)), static_cast<double>(Tm(1,2)), 
				static_cast<double>(Tm(2,0)), static_cast<double>(Tm(2,1)), static_cast<double>(Tm(2,2)));

		tf::Quaternion tfqt;
		tf3d.getRotation(tfqt);

		slide.rotation.x = tfqt.x();
		slide.rotation.y = tfqt.y();
		slide.rotation.z = tfqt.z();
		slide.rotation.w = tfqt.w();

		geometry_msgs::Pose calib_pose;

		calib_pose.position.x = laserodometry.pose.pose.position.x + slide.translation.x;
		calib_pose.position.y = laserodometry.pose.pose.position.y + slide.translation.y;
		calib_pose.position.z = laserodometry.pose.pose.position.z + slide.translation.z;

		calib_pose.orientation.x = laserodometry.pose.pose.orientation.x + slide.rotation.x;
		calib_pose.orientation.y = laserodometry.pose.pose.orientation.y + slide.rotation.y;
		calib_pose.orientation.z = laserodometry.pose.pose.orientation.z + slide.rotation.z;
		calib_pose.orientation.w = laserodometry.pose.pose.orientation.w + slide.rotation.w;

		tf::Transform calib_transform;
		poseMsgToTF( calib_pose , calib_transform );
		br.sendTransform(tf::StampedTransform(calib_transform, odom_data_time, "map", "laserodometry"));

		laserodometry.pose.pose.position = calib_pose.position;
		laserodometry.pose.pose.orientation = calib_pose.orientation;
		laserodometry.twist.twist.angular = odom_data.twist.twist.angular;
		laserodometry.twist.twist.linear = odom_data.twist.twist.linear;

		laserodometry.header.stamp = odom_data_time;


	}

	/*
	void LaserOdometry::final_transform_pc(Eigen::Matrix4f laserodometry_trans_matrix){

		pcl::transformPointCloud( velo_scans , velo_scans , laserodometrty_trans_matrix );
		pcl::transformPointCloud( FeatureCloud , FeatureCloud , laserodometry_trans_matrix );

	}
	*/

	void LaserOdometry::publish_result(){

		sensor_msgs::PointCloud2 velo, cent, edge;

		pcl::toROSMsg(velo_scans, velo);
		velo.header.frame_id = "laserodometry";
		velo.header.stamp = odom_data_time;

		pcl::toROSMsg(CloudCentroid, cent);
		cent.header.frame_id = "laserodometry";
		cent.header.stamp = odom_data_time;

		pcl::toROSMsg(CloudEdge, edge);
		edge.header.frame_id = "laserodometry";
		edge.header.stamp = odom_data_time;

		_pubVelodynePoints3.publish(velo);
		_pubCentroidPointLast.publish(cent);
		_pubEdgePointLast.publish(edge);
		_pubLaserOdomToInit.publish(laserodometry);

	}

	void LaserOdometry::reset(){
		tmp_pc_stored.clear();

		velo_scans.clear();
		CloudCentroid.clear();
		CloudEdge.clear();

		_lastFeatureCloud = FeatureCloud;
		FeatureCloud.clear();

		_last_odom_data = odom_data;

		velo_scans_checker = false;
		CloudCentroid_checker = false;
		CloudEdge_checker = false;
		odom_data_checker = false;

	}

	void LaserOdometry::process(){

		if( __systemdelay > 0 ){

			--__systemdelay;
			return;
		}

		if(!system_initialized_checker){
			bool init_bool = initialization();
			if(init_bool){
				system_initialized_checker = true;
				return;
			}
		}

		bool status = hasNewData();

		if( status == false )
			return;

		get_relative_trans();

		get_tf_data();

		// Convert point cloud's coordinate velodyne frame to map frame
		convert_coordinate_of_pc();

		//Calculate init slide as homogenerous transformation matrix by odometry
		Eigen::Matrix4f init_slide_matrix = init_pc_slide();

		//Calculate ICP slide by pcl's Iterative Closest Point module
		Eigen::Matrix4f pcl_slide_matrix = pcl_pc_slide();

		//Calculate calibrated laserodometry as homogenerous transformation matrix
		Eigen::Matrix4f laserodometry_trans_matrix = init_slide_matrix * pcl_slide_matrix;

		// Calculate calibrated odometry data and publish tf data
		send_tf_data(laserodometry_trans_matrix);

		// transform to calibrated position
		//final_transform_pc(laserodometry_trans_matrix);

		publish_result();

		reset();

	}

	void LaserOdometry::spin(){

		ros::Rate rate(100);

		rate.sleep();

		while( ros::ok() ){

			ros::spinOnce(); //get published data

			process();

			rate.sleep();
		}

	}

}


