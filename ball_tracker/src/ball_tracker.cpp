//Ball tracker

#include <ball_tracker/ball_tracker.hpp>
#include <visp/vpConfig.h>
#include <visp/vpDot2.h>
#include <visp/vpDisplayX.h>
#include <visp/vpImageConvert.h>
#include <visp_bridge/image.h>

#include <unistd.h>

#include <ur5_kendama_msgs/ball_position.h>
#include <std_msgs/Float64.h>

ball_tracker::ball_tracker(ros::NodeHandle& nh) : nh(nh) {

  this->color_received = false;
  this->depth_received = false;
  
  this->visp_init_done = false;
  blob.setGraphics(true);
  blob.setGraphicsThickness(2);

  sub_color = nh.subscribe("/camera/color/image_raw", 10, &ball_tracker::color_callback, this);
  
  sub_depth = nh.subscribe("/camera/depth/image_raw", 10, &ball_tracker::depth_callback, this);

  pub_pos = nh.advertise<ur5_kendama_msgs::ball_position>("ball_pos_camera", 10);


}

void ball_tracker::color_callback(const sensor_msgs::Image& incoming_color_img) {

  if(this->color_received == false) {
    ROS_INFO("Color image received. Waiting for depth image.");
    this->color_img = incoming_color_img;
    this->color_received = true;
  } else {
    ROS_INFO("Color image already received.");
  }

}


void ball_tracker::depth_callback(const sensor_msgs::Image& incoming_depth_img) {

  if((this->color_received == true) && (this->depth_received == false)) {
    this->depth_img = incoming_depth_img;
    this->depth_received = true;
    //Visp tracking
    ROS_INFO("Depth image receieved. Time for Visp stuff.");
    visp_tracking();
  }
  
}

void ball_tracker::visp_tracking() {

  ROS_INFO("In function ball_tracker::visp_tracking.");
  vpImage<vpRGBa> I_rgb = visp_bridge::toVispImageRGBa(this->color_img);
  vpImage<unsigned char> I;
  vpImageConvert::convert(I_rgb, I);

  try {
    
    if(!this->visp_init_done) {
      vpDisplayX d(I, 0, 0, "Camera view");
      while(!this->visp_init_done) {
	vpDisplay::display(I);
	std::cout << "Click blob to initialize tracker" << std::endl;
	vpDisplay::displayText(I, vpImagePoint(10,10), "Click blob to initialize tracker", vpColor::red);
	if(vpDisplay::getClick(I, this->germ, false)) {
	  this->blob.initTracking(I, this->germ);
	  this->visp_init_done = true;
	}
      }
      vpDisplay::flush(I);
    } else {
      this->blob.track(I);
      vpImagePoint centroid = this->blob.getCog();
      double x = centroid.get_i();
      double y = centroid.get_j();
      uint16_t z = this->depth_img.data[int(x)*this->depth_img.step + int(y)];
      std::cout << "Centroid coords: (" << x << ", " << y << ", " << z << ")" << std::endl;
      ur5_kendama_msgs::ball_position ball_pos_msg;
      ball_pos_msg.x.data = x;
      ball_pos_msg.y.data = y;
      ball_pos_msg.z.data = double(z);
      pub_pos.publish(ball_pos_msg);
    }
  } catch(...) {
    this->visp_init_done = false;
  }

  this->color_received = false;
  this->depth_received = false;
  
  std::cout << "End of ball_tracker::visp_function" << std::endl;
  
}
