#include <ros/ros.h>
#include <ros/package.h>

#include <signal.h>
#include <knowledge_representation/ShortTermMemory.h>
#include <knowledge_representation/RemoveObject.h>
#include <knowledge_representation/AddObject.h>
#include <knowledge_representation/ResolveObjectCorrespondences.h>
#include <pcl/conversions.h>
#include <pcl_conversions/pcl_conversions.h>

knowledge_rep::ShortTermMemory short_term_memory;
//true if Ctrl-C is pressed
bool g_caught_sigint=false;

/* what happens when ctr-c is pressed */
void sig_handler(int sig) {
    g_caught_sigint = true;
    ROS_INFO("caught sigint, init shutdown sequence...");
    ros::shutdown();
    exit(1);
};

bool remove_object_cb(knowledge_representation::RemoveObject::Request &req,
                      knowledge_representation::RemoveObject::Response &res) {
    short_term_memory.remove(req.id);
    return true;
}

bool add_object_cb(knowledge_representation::AddObject::Request &req,
                   knowledge_representation::AddObject::Response &res) {
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
    pcl::fromROSMsg(req.cloud, *cloud);
    short_term_memory.add(req.id, cloud);
    return true;
}

bool resolve_object_correspondences_cb(knowledge_representation::ResolveObjectCorrespondences::Request &req,
                                       knowledge_representation::ResolveObjectCorrespondences::Response &res) {
    // TODO: Fancy registration of clouds
    return true;
}


int main(int argc, char **argv) {
    // Intialize ROS with this node name
    ros::init(argc, argv, "short_term_memory_node");

    ros::NodeHandle n;
    ros::NodeHandle pnh("~");
    short_term_memory = knowledge_rep::ShortTermMemory();
    ros::ServiceServer forget_service = pnh.advertiseService("remove_object", remove_object_cb);
    ros::ServiceServer remember_service = pnh.advertiseService("add_object", add_object_cb);
    ros::ServiceServer resolve_object_correspondences_service = pnh.advertiseService("resolve_object_correspondencs",
                                                                                     resolve_object_correspondences_cb);

    ros::shutdown();
}