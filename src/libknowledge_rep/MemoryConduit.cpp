#include "knowledge_representation/MemoryConduit.h"

using namespace std;

bool knowledge_rep::MemoryConduit::encode(std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> &entities,
                                          pcl::PointCloud<pcl::PointXYZ>::Ptr &table) {
    vector<LongTermMemoryConduit::EntityAttribute> facings = ltmc.get_entity_attribute(robot_id, "facing");
    assert(facings.size() == 1);
    int facing_id = boost::get<int>(facings.at(0).value);
    //TODO: Make sure this is table
    //assert(boost::get<std::string>(ltmc.get_entity_attribute(facing_id, "concept")) == string("table"));
    vector<int> candidates = ltmc.get_entities_with_attribute_of_value("is_on", facing_id);
    stmc.resolve_object_correspondences(entities, candidates);
    // TODO: Take the ones that do not have correspondences. Make new entities for them, put them on the table
    // TODO: Forget in STMC the old entities that weren't corresponded to
    //TODO: Change sensed in ltmc on the entities
    return false;
}


vector<knowledge_rep::LongTermMemoryConduit::EntityAttribute>
knowledge_rep::MemoryConduit::relevant_to(std::vector<int> entities) {
    vector<knowledge_rep::LongTermMemoryConduit::EntityAttribute> obj_attrs;
    // TODO: Implement a smarter strategy for getting relevant entity attributes
    // Something that will recurse and find dependencies, but also not get
    // everything in the database as a result

    // For now, use all entities
    for (const auto obj_id: ltmc.get_all_entities()) {
        auto attrs = ltmc.get_entity_attributes(obj_id);
        obj_attrs.insert(obj_attrs.end(), attrs.begin(), attrs.end());
    }
    return obj_attrs;
}
