#include <ur5_kendama_controller_rtt/ball_launch_controller.hpp>

#include <std_msgs/Float64.h>

#include <kdl/chainiksolverpos_nr.hpp>
#include <kdl/chainiksolvervel_pinv.hpp>
#include <kdl/chainfksolverpos_recursive.hpp>

#include <kdl/frames.hpp>

#include <rtt/Component.hpp> // This should come last in the include list

ball_launch_controller::ball_launch_controller( const std::string& name ) 
    : TaskContext(name),
    port_msr_jnt_state("Measured joint state"),
    controller_state(idle)
{
    std::cout << "ball_launch_controller::ball_launch_controller" << std::endl;

    std::string robot_description_string;
    nh.param("/robot_description", robot_description_string, std::string());

    //sub_js = nh.subscribe("/robot/joint_states", 10, &ball_launch_controller::jointStateCallback, this);

    // Obtain the IK solver
    if ( kdl_parser::treeFromString(robot_description_string, tree) )
    {
        if ( tree.getChain("base_link", "ee_link", chain) )
        {
            fk_pos = new KDL::ChainFkSolverPos_recursive(chain);
            ik_vel = new KDL::ChainIkSolverVel_pinv(chain);
            ik_pos = new KDL::ChainIkSolverPos_NR(chain, *fk_pos, *ik_vel);
        }

        else 
        {
            ROS_ERROR("Cannot get a chain");
        }
    }

    else
    {
        ROS_ERROR("Cannot get a tree");
    }

    addPort("MsrJntState", port_msr_jnt_state);

    addOperation("GetJointPos", &ball_launch_controller::getJointPos, this, RTT::OwnThread); 
    addOperation("SetJointPos", &ball_launch_controller::setJointPos, this, RTT::OwnThread);
    addOperation("ZeroPose", &ball_launch_controller::commandZeroPose, this, RTT::OwnThread);
    addOperation("TPose", &ball_launch_controller::commandTPose, this, RTT::OwnThread);
    addOperation("LaunchBall", &ball_launch_controller::launchBall, this, RTT::OwnThread);
}


bool ball_launch_controller::configureHook() 
{
    std::cout << "ball_launch_controller::configureHook" << std::endl;

    // initialize trajectory generation objects
    rml = new ReflexxesAPI(6, getPeriod()); // gets period from task context object
    ip = new RMLPositionInputParameters(6);
    op = new RMLPositionOutputParameters(6);

    // initialize trajectory generation parameters
    for ( int i = 0; i < 6; i++ )
    {
        ip->CurrentPositionVector->VecData[i] = 0.0;
        ip->CurrentVelocityVector->VecData[i] = 0.0;
        ip->CurrentAccelerationVector->VecData[i] = 0.0;

        ip->MaxVelocityVector->VecData[i] = 1.0;
        ip->MaxAccelerationVector->VecData[i] = 1.0;
        ip->MaxJerkVector->VecData[i] = 10.0;

        ip->SelectionVector->VecData[i] = true;

        ip->TargetPositionVector->VecData[i] = 0.0;
        ip->TargetVelocityVector->VecData[i] = 0.0;
    }

    std::cout << ip->TargetPositionVector->VecData[1] << std::endl;

    commandTPose();

    std::cout << ip->TargetPositionVector->VecData[1] << std::endl;
}


bool ball_launch_controller::startHook() 
{
    std::cout << "ball_launch_controller::startHook" << std::endl;
}


void ball_launch_controller::updateHook() 
{
    /*switch(controller_state)
    {
        case idle :
            
            std::cout << "Idle" << std::endl;
            break;

        case launch :
            
            std::cout << "Launching" << std::endl;
            break;

        case follow :
            
            std::cout << "Following" << std::endl;
            
            break;

        case craddle :
            
            std::cout << "Craddling" << std::endl;
            break;
    }*/


    // compute an iteration of the trajectory
    int result = rml->RMLPosition(*ip, op, flags);

    // setup for next iteration
    *ip->CurrentPositionVector = *op->NewPositionVector;
    *ip->CurrentVelocityVector = *op->NewVelocityVector;
    *ip->CurrentAccelerationVector = *op->NewAccelerationVector;

    // get the joint values and write them to the port
    std_msgs::Float64MultiArray js;
    js.data.resize(6);
    
    for ( int i = 0; i < 6; i++ )
    {
        op->GetNewPositionVectorElement(&js.data[i], i);
    }

    port_msr_jnt_state.write(js);
}


void ball_launch_controller::stopHook() 
{
    std::cout << "ball_launch_controller::stopHoop" << std::endl;
}


void ball_launch_controller::cleanupHook() 
{
    std::cout << "ball_launch_controller::cleanupHook" << std::endl;
}


KDL::JntArray ball_launch_controller::getJointPos()
{
    // return the current joint position
    KDL::JntArray js(6);
    for ( int i = 0; i < 6; i++ )
    {
        js.data[i] = ip->CurrentPositionVector->VecData[i];
    }

    return js;
}


void ball_launch_controller::setJointPos( const KDL::JntArray& q , const KDL::JntArray& q_dot )
{
    // extract out components of the input joint array to set as target positions
    for ( int i = 0; i < 6; i++ )
    {
        ip->TargetPositionVector->VecData[i] = q.data[i];
        ip->TargetVelocityVector->VecData[i] = q_dot.data[i];
    }
}

void ball_launch_controller::commandTPose()
{
    KDL::JntArray t_pose_coords(6);
    t_pose_coords.data[0] = 0.0;
    t_pose_coords.data[1] = -1.5708;
    t_pose_coords.data[2] = 1.8708;
    t_pose_coords.data[3] = -0.3;
    t_pose_coords.data[4] = 1.5708;
    t_pose_coords.data[5] = 0.0;
    
    KDL::JntArray t_pose_velo(6);
    t_pose_velo.data[0] = 0.0;
    t_pose_velo.data[1] = 0.0;
    t_pose_velo.data[2] = 0.0;
    t_pose_velo.data[3] = 0.0;
    t_pose_velo.data[4] = 0.0;
    t_pose_velo.data[5] = 0.0;

    setJointPos(t_pose_coords, t_pose_velo);
}

void ball_launch_controller::commandZeroPose()
{
    KDL::JntArray t_pose_coords(6);
    t_pose_coords.data[0] = 0.0;
    t_pose_coords.data[1] = 0.0;
    t_pose_coords.data[2] = 0.0;
    t_pose_coords.data[3] = 0.0;
    t_pose_coords.data[4] = 0.0;
    t_pose_coords.data[5] = 0.0;
    
    KDL::JntArray t_pose_velo(6);
    t_pose_velo.data[0] = 0.0;
    t_pose_velo.data[1] = 0.0;
    t_pose_velo.data[2] = 0.0;
    t_pose_velo.data[3] = 0.0;
    t_pose_velo.data[4] = 0.0;
    t_pose_velo.data[5] = 0.0;

    setJointPos(t_pose_coords, t_pose_velo);
}

void ball_launch_controller::jointStateCallback()
{

}

void ball_launch_controller::launchBall()
{
    // Compute the IK
    KDL::JntArray q_cur = getJointPos();
    KDL::Frame p_cur;
    fk_pos->JntToCart(q_cur, p_cur, -1);

    KDL::Frame p_desired = p_cur;
    KDL::Vector up(0.0, 0.0, 0.1);
    p_desired.p += up;

    KDL::JntArray q_desired(6);
    ik_pos->CartToJnt(q_cur, p_desired, q_desired);

    KDL::JntArray t_pose_velo(6);
    t_pose_velo.data[0] = 0.0;
    t_pose_velo.data[1] = 0.0;
    t_pose_velo.data[2] = 0.0;
    t_pose_velo.data[3] = 0.0;
    t_pose_velo.data[4] = 0.0;
    t_pose_velo.data[5] = 0.0;

    setJointPos(q_desired, t_pose_velo);
}


ORO_CREATE_COMPONENT(ball_launch_controller) // register the RTT component