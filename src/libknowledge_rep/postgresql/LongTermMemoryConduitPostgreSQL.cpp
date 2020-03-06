#include "LongTermMemoryConduitPostgreSQL.h"
#include <knowledge_representation/LongTermMemoryConduitInterface.h>
#include <iostream>
#include <string>
#include <knowledge_representation/LTMCConcept.h>
#include <knowledge_representation/LTMCInstance.h>
#include <knowledge_representation/LTMCMap.h>
#include <knowledge_representation/LTMCPoint.h>
#include <knowledge_representation/LTMCPose.h>
#include <knowledge_representation/LTMCRegion.h>
#include <vector>
#include <utility>

using std::string;
using std::vector;
namespace knowledge_rep
{
typedef LTMCEntity<LongTermMemoryConduitPostgreSQL> Entity;
typedef LTMCConcept<LongTermMemoryConduitPostgreSQL> Concept;
typedef LTMCInstance<LongTermMemoryConduitPostgreSQL> Instance;
typedef LTMCPoint<LongTermMemoryConduitPostgreSQL> Point;
typedef LTMCPose<LongTermMemoryConduitPostgreSQL> Pose;
typedef LTMCRegion<LongTermMemoryConduitPostgreSQL> Region;
typedef LTMCMap<LongTermMemoryConduitPostgreSQL> Map;

LongTermMemoryConduitPostgreSQL::LongTermMemoryConduitPostgreSQL(const string& db_name)
  : LongTermMemoryConduitInterface<LongTermMemoryConduitPostgreSQL>()
{
  conn = std::unique_ptr<pqxx::connection>(new pqxx::connection("postgresql://postgres@localhost/" + db_name));
}

LongTermMemoryConduitPostgreSQL::~LongTermMemoryConduitPostgreSQL() = default;

bool LongTermMemoryConduitPostgreSQL::addEntity(uint id)
{
  pqxx::work txn{ *conn };
  pqxx::result result = txn.exec("INSERT INTO entities "
                                 "VALUES (" +
                                 txn.quote(id) + ") ON CONFLICT DO NOTHING RETURNING entity_id");
  txn.commit();
  return result.size() == 1;
}

/**
 * @brief Add a new attribute
 * @param name the name of the attribute
 * @param allowed_types a bitmask representing the types allowed for the attribute
 * @return whether the attribute was added. Note that addition will fail if the attribute already exists.
 */
bool LongTermMemoryConduitPostgreSQL::addNewAttribute(const string& name, const AttributeValueType type)
{
  try
  {
    pqxx::work txn{ *conn };
    pqxx::result result = txn.exec("INSERT INTO attributes VALUES (" + txn.quote(name) + ", " +
                                   txn.quote(attribute_value_type_to_string[type]) + ") ON CONFLICT DO NOTHING");
    txn.commit();
    return result.affected_rows() == 1;
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return false;
  }
}

bool LongTermMemoryConduitPostgreSQL::entityExists(uint id) const
{
  pqxx::work txn{ *conn };
  // Remove all entities
  auto result = txn.exec("SELECT 1 FROM entities WHERE entity_id=" + txn.quote(id));
  txn.commit();
  return result.size() == 1;
}

vector<Entity> LongTermMemoryConduitPostgreSQL::getEntitiesWithAttributeOfValue(const string& attribute_name,
                                                                                const uint other_entity_id)
{
  pqxx::work txn{ *conn };
  auto result = txn.exec("SELECT entity_id FROM entity_attributes_id "
                         "WHERE attribute_value=" +
                         txn.quote(other_entity_id) + " and attribute_name = " + txn.quote(attribute_name));
  txn.commit();

  vector<Entity> return_result;
  for (const auto& row : result)
  {
    return_result.emplace_back(row["entity_id"].as<uint>(), *this);
  }
  return return_result;
}

vector<Entity> LongTermMemoryConduitPostgreSQL::getEntitiesWithAttributeOfValue(const string& attribute_name,
                                                                                const bool bool_val)
{
  pqxx::work txn{ *conn };
  auto result = txn.exec("SELECT entity_id FROM entity_attributes_bool "
                         "WHERE attribute_value=" +
                         txn.quote(bool_val) + " and attribute_name = " + txn.quote(attribute_name));
  txn.commit();

  vector<Entity> return_result;
  for (const auto& row : result)
  {
    return_result.emplace_back(row["entity_id"].as<uint>(), *this);
  }
  return return_result;
}

vector<Entity> LongTermMemoryConduitPostgreSQL::getEntitiesWithAttributeOfValue(const string& attribute_name,
                                                                                const string& string_val)
{
  pqxx::work txn{ *conn };
  auto result = txn.exec("SELECT entity_id FROM entity_attributes_id "
                         "WHERE attribute_value=" +
                         txn.quote(string_val) + " and attribute_name = " + txn.quote(attribute_name));
  txn.commit();

  vector<Entity> return_result;
  for (const auto& row : result)
  {
    return_result.emplace_back(row["entity_id"].as<uint>(), *this);
  }
  return return_result;
}

std::vector<Entity> LongTermMemoryConduitPostgreSQL::getAllEntities()
{
  pqxx::work txn{ *conn };

  auto result = txn.exec("TABLE entities");
  txn.commit();

  vector<Entity> entities;
  for (const auto& row : result)
  {
    entities.emplace_back(row["entity_id"].as<uint>(), *this);
  }
  return entities;
}

uint LongTermMemoryConduitPostgreSQL::deleteAllAttributes()
{
  pqxx::work txn{ *conn };

  // Remove all entities
  uint num_deleted = txn.exec("DELETE FROM attributes").affected_rows();
  // Use the baked in function to get the default configuration back
  txn.exec("SELECT * FROM add_default_attributes()");
  txn.commit();
  return num_deleted;
}

uint LongTermMemoryConduitPostgreSQL::deleteAllEntities()
{
  pqxx::work txn{ *conn };

  // Remove all entities
  uint num_deleted = txn.exec("DELETE FROM entities").affected_rows();
  // Use the baked in function to get the default configuration back
  txn.exec("SELECT * FROM add_default_entities()");
  txn.commit();
  assert(entityExists(1));
  return num_deleted;
}

bool LongTermMemoryConduitPostgreSQL::deleteAttribute(const string& name)
{
  pqxx::work txn{ *conn };
  uint num_deleted = txn.exec("DELETE FROM attributes WHERE attribute_name = " + txn.quote(name)).affected_rows();
  txn.commit();
  return num_deleted;
}

bool LongTermMemoryConduitPostgreSQL::attributeExists(const string& name) const
{
  pqxx::work txn{ *conn };
  auto result = txn.exec("SELECT 1 FROM attributes WHERE attribute_name=" + txn.quote(name));
  txn.commit();
  return result.size() == 1;
}

Concept LongTermMemoryConduitPostgreSQL::getConcept(const string& name)
{
  pqxx::work txn{ *conn, "getConcept" };
  auto result = txn.exec("SELECT entity_id FROM concepts WHERE concept_name =" + txn.quote(name));
  txn.commit();

  if (result.empty())
  {
    Entity new_concept = addEntity();
    pqxx::work txn{ *conn, "getConcept" };
    auto result = txn.parameterized("INSERT INTO concepts VALUES ($1, $2)")(new_concept.entity_id)(name).exec();
    txn.commit();
    return { new_concept.entity_id, name, *this };
  }
  else
  {
    return { result[0]["entity_id"].as<uint>(), name, *this };
  }
}

Instance LongTermMemoryConduitPostgreSQL::getInstanceNamed(const string& name)
{
  pqxx::work txn{ *conn, "getInstanceNamed" };
  string query = "SELECT entity_id FROM entity_attributes_str WHERE attribute_name = 'name' "
                 "AND attribute_value = " +
                 txn.quote(name) + "AND entity_id NOT IN "
                                   "(SELECT entity_id FROM entity_attributes_bool "
                                   "WHERE attribute_name = 'is_concept' AND attribute_value = true)";

  // If there's no "is_concept" marker on an entity, we assume it is not a concept
  auto q_result = txn.exec(query);
  txn.commit();

  if (q_result.empty())
  {
    Instance new_entity = Instance(addEntity().entity_id, *this);
    new_entity.addAttribute("name", name);
    new_entity.addAttribute("is_concept", false);
    return new_entity;
  }
  else
  {
    // Can only be one instance with a given name
    assert(q_result.size() == 1);
    return { q_result[0]["entity_id"].as<uint>(), *this };
  }
}

boost::optional<Entity> LongTermMemoryConduitPostgreSQL::getEntity(uint entity_id)
{
  if (entityExists(entity_id))
  {
    return Entity{ entity_id, *this };
  }
  return {};
}

Instance LongTermMemoryConduitPostgreSQL::getRobot()
{
  Instance robot = Instance(1, *this);
  assert(robot.isValid());
  return robot;
}

Entity LongTermMemoryConduitPostgreSQL::addEntity()
{
  pqxx::work txn{ *conn, "addEntity" };

  auto result = txn.exec("INSERT INTO entities VALUES (DEFAULT) RETURNING entity_id");
  txn.commit();
  return { result[0]["entity_id"].as<uint>(), *this };
}

std::vector<Concept> LongTermMemoryConduitPostgreSQL::getAllConcepts()
{
  pqxx::work txn{ *conn, "getAllConcepts" };
  auto result = txn.exec("SELECT entity_id, concept_name FROM concepts");
  txn.commit();
  vector<Concept> concepts;
  for (const auto& row : result)
  {
    concepts.emplace_back(row["entity_id"].as<uint>(), row["concept_name"].as<string>(), *this);
  }
  return concepts;
}

std::vector<Instance> LongTermMemoryConduitPostgreSQL::getAllInstances()
{
  pqxx::work txn{ *conn, "getAllInstances" };
  auto result = txn.exec("SELECT entity_id FROM entities WHERE entity_id NOT IN ("
                         "SELECT entity_id FROM concepts)");
  txn.commit();
  vector<Instance> instances;
  for (const auto& row : result)
  {
    instances.emplace_back(row["entity_id"].as<uint>(), *this);
  }
  return instances;
}

vector<std::pair<string, AttributeValueType>> LongTermMemoryConduitPostgreSQL::getAllAttributes() const
{
  vector<std::pair<string, AttributeValueType>> attribute_names;
  pqxx::work txn{ *conn, "getAllAttributes" };
  auto result = txn.exec("TABLE attributes");
  txn.commit();
  for (const auto& row : result)
  {
    attribute_names.emplace_back(row["attribute_name"].as<string>(),
                                 string_to_attribute_value_type[row["type"].as<string>()]);
  }
  return attribute_names;
}

// MAP
Map LongTermMemoryConduitPostgreSQL::getMap(const std::string& name)
{
  pqxx::work txn{ *conn, "getMap" };
  auto result = txn.parameterized("SELECT entity_id FROM maps WHERE map_name = $1")(name).exec();
  txn.commit();

  if (result.empty())
  {
    Concept map_concept = getConcept("map");
    Instance new_map = map_concept.createInstance();
    pqxx::work txn{ *conn, "getMap" };
    auto result = txn.parameterized("INSERT INTO maps VALUES ($1, $2)")(new_map.entity_id)(name).exec();
    txn.commit();
    return { new_map.entity_id, name, *this };
  }
  else
  {
    return { result[0]["entity_id"].as<uint>(), name, *this };
  }
}

vector<EntityAttribute> LongTermMemoryConduitPostgreSQL::getAllEntityAttributes()
{
  std::vector<EntityAttribute> entity_attrs;
  for (const auto entity : this->getAllEntities())
  {
    auto attrs = entity.getAttributes();
    entity_attrs.insert(entity_attrs.end(), attrs.begin(), attrs.end());
  }
  return entity_attrs;
}

// PROMOTERS

bool LongTermMemoryConduitPostgreSQL::makeConcept(uint id, std::string name)
{
  try
  {
    pqxx::work txn{ *conn, "makeConcept" };
    auto result = txn.parameterized("INSERT INTO concepts VALUES ($1, $2)")(id)(name).exec();
    txn.commit();
    return result.affected_rows() == 1;
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return false;
  }
}

// ENTITY BACKERS

bool LongTermMemoryConduitPostgreSQL::deleteEntity(Entity& entity)
{
  // TODO(nickswalker): Recursively remove entities that are members of directional relations
  if (!entity.isValid())
  {
    return false;
  }
  // Because we've all references to this entity have foreign key relationships with cascade set,
  // this should clear out any references to this entity in other tables as well
  try
  {
    pqxx::work txn{ *conn, "deleteEntity" };
    auto result = txn.exec("DELETE FROM entities WHERE entity_id = " + txn.quote(entity.entity_id));
    txn.commit();
    return result.affected_rows() == 1;
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return false;
  }
}

bool LongTermMemoryConduitPostgreSQL::addAttribute(Entity& entity, const std::string& attribute_name,
                                                   const float float_val)
{
  try
  {
    pqxx::work txn{ *conn, "addAttribute (float)" };
    auto result =
        txn.exec("INSERT INTO entity_attributes_float "
                 "VALUES (" +
                 txn.quote(entity.entity_id) + ", " + txn.quote(attribute_name) + ", " + txn.quote(float_val) + ")");
    txn.commit();
    return result.affected_rows() == 1;
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return false;
  }
}

bool LongTermMemoryConduitPostgreSQL::addAttribute(Entity& entity, const std::string& attribute_name,
                                                   const bool bool_val)
{
  try
  {
    pqxx::work txn{ *conn, "addAttribute (bool)" };
    auto result =
        txn.exec("INSERT INTO entity_attributes_bool "
                 "VALUES (" +
                 txn.quote(entity.entity_id) + ", " + txn.quote(attribute_name) + ", " + txn.quote(bool_val) + ")");
    txn.commit();
    return result.affected_rows() == 1;
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return false;
  }
}

bool LongTermMemoryConduitPostgreSQL::addAttribute(Entity& entity, const std::string& attribute_name,
                                                   const uint other_entity_id)
{
  try
  {
    pqxx::work txn{ *conn, "addAttribute (id)" };
    auto result = txn.exec("INSERT INTO entity_attributes_id VALUES (" + txn.quote(entity.entity_id) + ", " +
                           txn.quote(attribute_name) + ", " + txn.quote(other_entity_id) + ")");
    txn.commit();
    return result.affected_rows() == 1;
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return false;
  }
}

bool LongTermMemoryConduitPostgreSQL::addAttribute(Entity& entity, const std::string& attribute_name,
                                                   const std::string& string_val)
{
  try
  {
    pqxx::work txn{ *conn, "addAttribute (str)" };
    auto result =
        txn.exec("INSERT INTO entity_attributes_str "
                 "VALUES (" +
                 txn.quote(entity.entity_id) + ", " + txn.quote(attribute_name) + ", " + txn.quote(string_val) + ")");
    txn.commit();
    return result.affected_rows() == 1;
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return false;
  }
}

int LongTermMemoryConduitPostgreSQL::removeAttribute(Entity& entity, const std::string& attribute_name)
{
  string query;
  pqxx::work txn{ *conn, "removeAttribute" };
  try
  {
    auto result = txn.exec("SELECT * FROM remove_attribute"
                           "(" +
                           txn.quote(entity.entity_id) + ", " + txn.quote(attribute_name) + ") AS count");
    txn.commit();
    return result[0]["count"].as<int>();
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return 0;
  }
}

int LongTermMemoryConduitPostgreSQL::removeAttributeOfValue(Entity& entity, const std::string& attribute_name,
                                                            const Entity& other_entity)
{
  assert(false);
}

void unwrap_attribute_rows(pqxx::result rows, vector<EntityAttribute>& entity_attributes)
{
  pqxx::oid type = rows[0]["attribute_value"].type();
  if (type == 23)
  {
    for (const auto& row : rows)
    {
      entity_attributes.emplace_back(row["entity_id"].as<uint>(), row["attribute_name"].as<string>(),
                                     row["attribute_value"].as<int>());
    }
  }
  else if (type == 1043)
  {
    for (const auto& row : rows)
    {
      entity_attributes.emplace_back(row["entity_id"].as<uint>(), row["attribute_name"].as<string>(),
                                     row["attribute_value"].as<string>());
    }
  }
  else if (type == 701)
  {
    for (const auto& row : rows)
    {
      entity_attributes.emplace_back(row["entity_id"].as<uint>(), row["attribute_name"].as<string>(),
                                     row["attribute_value"].as<float>());
    }
  }
  else if (type == 16)
  {
    for (const auto& row : rows)
    {
      entity_attributes.emplace_back(row["entity_id"].as<uint>(), row["attribute_name"].as<string>(),
                                     row["attribute_value"].as<bool>());
    }
  }
  else
  {
    assert(false);
  }
}

vector<EntityAttribute> LongTermMemoryConduitPostgreSQL::getAttributes(const Entity& entity) const
{
  vector<EntityAttribute> attributes;
  for (const auto& name : TABLE_NAMES)
  {
    try
    {
      pqxx::work txn{ *conn, "getAttributes" };
      auto result =
          txn.exec("SELECT * FROM " + std::string(name) + " WHERE entity_id = " + txn.quote(entity.entity_id));
      txn.commit();
      unwrap_attribute_rows(result, attributes);
    }
    catch (const std::exception& e)
    {
      std::cerr << e.what() << std::endl;
      return {};
    }
  }
  return attributes;
}

std::vector<EntityAttribute> LongTermMemoryConduitPostgreSQL::getAttributes(const Entity& entity,
                                                                            const std::string& attribute_name) const
{
  vector<EntityAttribute> attributes;
  for (const auto& name : TABLE_NAMES)
  {
    try
    {
      pqxx::work txn{ *conn, "getAttributes" };
      auto result = txn.exec("SELECT * FROM " + std::string(name) + " WHERE entity_id = " +
                             txn.quote(entity.entity_id) + " AND attribute_name = " + txn.quote(attribute_name));
      txn.commit();
      unwrap_attribute_rows(result, attributes);
    }
    catch (const std::exception& e)
    {
      std::cerr << e.what() << std::endl;
      return {};
    }
  }
  return attributes;
}

bool LongTermMemoryConduitPostgreSQL::isValid(const Entity& entity) const
{
  return entityExists(entity.entity_id);
}

// INSTANCE BACKERS

std::vector<Concept> LongTermMemoryConduitPostgreSQL::getConcepts(const Instance& instance)
{
  try
  {
    pqxx::work txn{ *conn, "getConcepts" };
    auto result = txn.parameterized("SELECT concepts.entity_id, concepts.concept_name FROM instance_of "
                                    "INNER JOIN concepts ON concepts.concept_name = instance_of.concept_name "
                                    "WHERE instance_of.entity_id = $1")(instance.entity_id)
                      .exec();
    txn.commit();
    std::vector<Concept> concepts{};
    for (const auto& row : result)
    {
      concepts.emplace_back(row["entity_id"].as<uint>(), row["concept_name"].as<string>(), *this);
    }
    return concepts;
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return {};
  }
}

std::vector<Concept> LongTermMemoryConduitPostgreSQL::getConceptsRecursive(const Instance& instance)
{
  try
  {
    pqxx::work txn{ *conn, "getConceptsRecursive" };
    auto result = txn.parameterized("SELECT * FROM get_concepts_recursive($1)")(instance.entity_id).exec();
    txn.commit();
    std::vector<Concept> concepts{};
    for (const auto& row : result)
    {
      concepts.emplace_back(row["entity_id"].as<uint>(), row["concept_name"].as<string>(), *this);
    }
    return concepts;
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return {};
  }
}

bool LongTermMemoryConduitPostgreSQL::makeInstanceOf(Instance& instance, const Concept& concept)
{
  try
  {
    pqxx::work txn{ *conn, "makeInstanceOf" };
    auto result =
        txn.parameterized("INSERT INTO instance_of VALUES ($1,$2) ")(instance.entity_id)(concept.getName()).exec();
    txn.commit();
    return result.affected_rows() == 1;
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return false;
  }
}

// CONCEPT BACKERS

vector<Concept> LongTermMemoryConduitPostgreSQL::getChildren(const Concept& concept)
{
  try
  {
    pqxx::work txn{ *conn, "getChildren" };
    auto result = txn.parameterized("SELECT concepts.entity_id, concept_name FROM entity_attributes_id eai"
                                    " INNER JOIN concepts ON eai.entity_id = concepts.entity_id WHERE attribute_name = "
                                    "'is_a' AND attribute_value = $1")(concept.entity_id)
                      .exec();
    txn.commit();
    std::vector<Concept> concepts{};
    for (const auto& row : result)
    {
      concepts.emplace_back(row["entity_id"].as<uint>(), row["concept_name"].as<string>(), *this);
    }
    return concepts;
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return {};
  }
}

vector<Concept> LongTermMemoryConduitPostgreSQL::getChildrenRecursive(const Concept& concept)
{
  try
  {
    pqxx::work txn{ *conn, "getChildrenRecursive" };
    auto result = txn.parameterized("SELECT * FROM get_all_concept_descendants($1)")(concept.entity_id).exec();
    txn.commit();
    std::vector<Concept> concepts{};
    for (const auto& row : result)
    {
      concepts.emplace_back(row["entity_id"].as<uint>(), row["concept_name"].as<string>(), *this);
    }
    return concepts;
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return {};
  }
}

std::vector<Instance> LongTermMemoryConduitPostgreSQL::getInstances(const ConceptImpl& concept)
{
  try
  {
    pqxx::work txn{ *conn, "getInstances" };
    auto result =
        txn.parameterized("SELECT entity_id FROM instance_of WHERE concept_name = $1")(concept.getName()).exec();
    txn.commit();
    std::vector<Instance> instances{};
    for (const auto& row : result)
    {
      instances.emplace_back(row["entity_id"].as<uint>(), *this);
    }
    return instances;
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return {};
  }
}

int LongTermMemoryConduitPostgreSQL::removeInstances(const Concept& concept)
{
  pqxx::work txn{ *conn, "removeInstances" };
  auto result = txn.parameterized("DELETE FROM entities WHERE entity_id IN "
                                  "(SELECT entity_id FROM instance_of WHERE concept_name = $1)")(concept.getName())
                    .exec();
  txn.commit();
  return result.affected_rows();
}

int LongTermMemoryConduitPostgreSQL::removeInstancesRecursive(const Concept& concept)
{
  pqxx::work txn{ *conn, "removeInstancesRecursive" };
  auto result =
      txn.parameterized("DELETE FROM entities WHERE entity_id IN "
                        "(SELECT entity_id FROM get_all_instances_of_concept_recursive($1))")(concept.entity_id)
          .exec();
  txn.commit();
  return result.affected_rows();
}

// MAP BACKERS
Point LongTermMemoryConduitPostgreSQL::addPoint(Map& map, const std::string& name, double x, double y)
{
  auto point_concept = getConcept("point");
  auto point = point_concept.createInstance(name).get();
  map.addAttribute("has", point);
  pqxx::work txn{ *conn, "addPoint" };
  auto result = txn.parameterized("INSERT INTO points VALUES ($1, $2, $3, point($4 ,$5)) RETURNING entity_id")(
                       point.entity_id)(name)(map.getName())(x)(y)
                    .exec();
  txn.commit();
  // TODO(nickswalker): We have to reconstruct to promote the instance to a point. Is there a better way?
  return { point.entity_id, name, x, y, map, *this };
}

Pose LongTermMemoryConduitPostgreSQL::addPose(Map& map, const std::string& name, double x, double y, double theta)
{
  auto pose_concept = getConcept("pose");
  auto pose = pose_concept.createInstance(name).get();
  map.addAttribute("has", pose);
  pqxx::work txn{ *conn, "addPose" };
  auto result =
      txn.parameterized("INSERT INTO poses SELECT $1, $2, $3, lseg(point($4, $5),point($4+COS($6),$5+SIN($6)))")(
             pose.entity_id)(name)(map.getName())(x)(y)(theta)
          .exec();
  txn.commit();
  return { pose.entity_id, name, x, y, theta, map, *this };
}

Region LongTermMemoryConduitPostgreSQL::addRegion(Map& map, const std::string& name,
                                                  const std::vector<std::pair<double, double>>& points)
{
  assert(false);
}

boost::optional<Point> LongTermMemoryConduitPostgreSQL::getPoint(Map& map, const std::string& name)
{
  pqxx::work txn{ *conn, "getPoint" };

  auto q_result = txn.parameterized("SELECT entity_id, point[0] AS x, point[1] AS y FROM points WHERE parent_map_name "
                                    "= $1 AND point_name = $2")(map.getName())(name)
                      .exec();
  txn.commit();
  assert(q_result.size() <= 1);
  if (q_result.size() == 1)
  {
    auto point = q_result[0];
    return Point{ point["entity_id"].as<uint>(), name, point["x"].as<double>(), point["y"].as<double>(), map, *this };
  }
  return {};
}

boost::optional<Pose> LongTermMemoryConduitPostgreSQL::getPose(Map& map, const std::string& name)
{
  pqxx::work txn{ *conn, "getPose" };
  string query =
      "SELECT entity_id, start[0] as x, start[1] as y, ATAN2(end_point[1]-start[1],end_point[0]-start[0]) "
      "as theta FROM (SELECT entity_id, pose[0] as start, pose[1] as end_point FROM poses WHERE parent_map_name "
      "= $1 AND pose_name = $2) AS dummy_sub_alias";

  auto q_result = txn.parameterized(query)(map.getName())(name).exec();
  txn.commit();
  assert(q_result.size() <= 1);
  if (q_result.size() == 1)
  {
    auto pose = q_result[0];
    return Pose{ pose["entity_id"].as<uint>(),
                 name,
                 pose["x"].as<double>(),
                 pose["y"].as<double>(),
                 pose["theta"].as<double>(),
                 map,
                 *this };
  }
  return {};
}

boost::optional<Region> LongTermMemoryConduitPostgreSQL::getRegion(Map& map, const std::string& name)
{
  assert(false);
}

vector<Point> LongTermMemoryConduitPostgreSQL::getAllPoints(Map& map)
{
  pqxx::work txn{ *conn, "getAllPoints" };
  auto q_result = txn.parameterized("SELECT entity_id, point[0] AS x, point[1] AS y, point_name FROM points WHERE "
                                    "parent_map_name = $1")(map.getName())
                      .exec();
  txn.commit();
  vector<Point> points;
  for (const auto& row : q_result)
  {
    points.emplace_back(row["entity_id"].as<uint>(), row["point_name"].as<string>(), row["x"].as<double>(),
                        row["y"].as<double>(), map, *this);
  }
  return points;
}

vector<Pose> LongTermMemoryConduitPostgreSQL::getAllPoses(Map& map)
{
  pqxx::work txn{ *conn, "getAllPoses" };
  string query = "SELECT entity_id, start[0] as x, start[1] as y, ATAN2(end_point[1]-start[1],end_point[0]-start[0]) "
                 "as theta, pose_name FROM (SELECT entity_id, pose[0] as start, pose[1] as end_point, pose_name FROM "
                 "poses WHERE parent_map_name "
                 "= $1) AS dummy_sub_alias";

  auto q_result = txn.parameterized(query)(map.getName()).exec();
  txn.commit();
  vector<Pose> poses;
  for (const auto& row : q_result)
  {
    poses.emplace_back(row["entity_id"].as<uint>(), row["pose_name"].as<string>(), row["x"].as<double>(),
                       row["y"].as<double>(), row["theta"].as<double>(), map, *this);
  }
  return poses;
}

vector<Region> LongTermMemoryConduitPostgreSQL::getAllRegions(Map& map)
{
  assert(false);
}

}  // namespace knowledge_rep
