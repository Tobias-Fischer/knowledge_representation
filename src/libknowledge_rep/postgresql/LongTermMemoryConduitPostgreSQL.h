#pragma once

#include <knowledge_representation/LongTermMemoryConduitInterface.h>
#include <pqxx/pqxx>
#include <string>
#include <vector>
#include <utility>

namespace knowledge_rep
{
static const char* TABLE_NAMES[] = { "entity_attributes_id", "entity_attributes_str", "entity_attributes_bool",
                                     "entity_attributes_float" };

/// A concrete implementation of the LongTermMemoryConduitInterface, leveraging libpqxx and PostgreSQL.
class LongTermMemoryConduitPostgreSQL : public LongTermMemoryConduitInterface<LongTermMemoryConduitPostgreSQL>
{
  using EntityImpl = LTMCEntity<LongTermMemoryConduitPostgreSQL>;
  using InstanceImpl = LTMCInstance<LongTermMemoryConduitPostgreSQL>;
  using ConceptImpl = LTMCConcept<LongTermMemoryConduitPostgreSQL>;
  using MapImpl = LTMCMap<LongTermMemoryConduitPostgreSQL>;
  using PointImpl = LTMCPoint<LongTermMemoryConduitPostgreSQL>;
  using PoseImpl = LTMCPose<LongTermMemoryConduitPostgreSQL>;
  using RegionImpl = LTMCRegion<LongTermMemoryConduitPostgreSQL>;

  friend EntityImpl;
  friend InstanceImpl;
  friend ConceptImpl;
  friend MapImpl;
  friend PointImpl;
  friend PoseImpl;
  friend RegionImpl;

  friend class LongTermMemoryConduitInterface;

public:
  std::unique_ptr<pqxx::connection> conn;

  explicit LongTermMemoryConduitPostgreSQL(const std::string& db_name);

  // Move constructor
  LongTermMemoryConduitPostgreSQL(LongTermMemoryConduitPostgreSQL&& that) = default;

  ~LongTermMemoryConduitPostgreSQL();

  // Move assignment
  LongTermMemoryConduitPostgreSQL& operator=(LongTermMemoryConduitPostgreSQL&& that) noexcept = default;

  // TODO(nickswalker): Expose this in the interface once we know what run-time attribute
  // operations are useful.
  bool addNewAttribute(const std::string& name, const AttributeValueType type);

  std::vector<EntityImpl> getEntitiesWithAttributeOfValue(const std::string& attribute_name,
                                                          const uint other_entity_id);

  std::vector<EntityImpl> getEntitiesWithAttributeOfValue(const std::string& attribute_name, const bool bool_val);

  std::vector<EntityImpl> getEntitiesWithAttributeOfValue(const std::string& attribute_name,
                                                          const std::string& string_val);

  bool entityExists(uint id) const;

  bool deleteAttribute(const std::string& name);

  bool attributeExists(const std::string& name) const;

  uint deleteAllEntities();

  uint deleteAllAttributes();

  std::vector<EntityImpl> getAllEntities();

  std::vector<ConceptImpl> getAllConcepts();

  std::vector<InstanceImpl> getAllInstances();

  std::vector<std::pair<std::string, AttributeValueType>> getAllAttributes() const;

  std::vector<EntityAttribute> getAllEntityAttributes();

  template <typename T>
  bool selectQuery(const std::string& sql_query, std::vector<EntityAttribute>& result) const
  {
    try
    {
      pqxx::work txn{ *conn };
      auto query_result = txn.exec(sql_query);
      for (const auto& row : query_result)
      {
        result.emplace_back(row["entity_id"].as<uint>(), row["attribute_name"].as<std::string>(),
                            row["attribute_name"].as<T>());
      }
    }
    catch (const std::exception& e)
    {
      std::cerr << e.what() << std::endl;
      return false;
    }
    return true;
  }
  // RAW QUERIES

  bool selectQueryInt(const std::string& sql_query, std::vector<EntityAttribute>& result) const
  {
    return selectQuery<int>(sql_query, result);
  }

  bool selectQueryFloat(const std::string& sql_query, std::vector<EntityAttribute>& result) const
  {
    return selectQuery<float>(sql_query, result);
  }

  bool selectQueryString(const std::string& sql_query, std::vector<EntityAttribute>& result) const
  {
    return selectQuery<std::string>(sql_query, result);
  }

  bool selectQueryBool(const std::string& sql_query, std::vector<EntityAttribute>& result) const
  {
    return selectQuery<bool>(sql_query, result);
  }

  // MAP
  MapImpl getMap(const std::string& name);

  // CONVENIENCE
  ConceptImpl getConcept(const std::string& name);

  InstanceImpl getInstanceNamed(const std::string& name);

  InstanceImpl getRobot();

  EntityImpl addEntity();

  bool addEntity(uint id);

  boost::optional<EntityImpl> getEntity(uint entity_id);

  // PROMOTERS

  bool makeConcept(uint id, std::string name);

protected:
  // ENTITY BACKERS
  bool deleteEntity(EntityImpl& entity);

  bool addAttribute(EntityImpl& entity, const std::string& attribute_name, const float float_val);

  bool addAttribute(EntityImpl& entity, const std::string& attribute_name, const bool bool_val);

  bool addAttribute(EntityImpl& entity, const std::string& attribute_name, const uint other_entity_id);

  bool addAttribute(EntityImpl& entity, const std::string& attribute_name, const std::string& string_val);

  int removeAttribute(EntityImpl& entity, const std::string& attribute_name);

  int removeAttributeOfValue(EntityImpl& entity, const std::string& attribute_name, const EntityImpl& other_entity);

  std::vector<EntityAttribute> getAttributes(const EntityImpl& entity) const;

  std::vector<EntityAttribute> getAttributes(const EntityImpl& entity, const std::string& attribute_name) const;

  bool isValid(const EntityImpl& entity) const;

  // INSTANCE BACKERS
  std::vector<ConceptImpl> getConcepts(const InstanceImpl& instance);

  std::vector<ConceptImpl> getConceptsRecursive(const InstanceImpl& instance);

  bool makeInstanceOf(InstanceImpl& instance, const ConceptImpl& concept);

  // CONCEPT BACKERS

  std::vector<ConceptImpl> getChildren(const ConceptImpl& concept);

  std::vector<ConceptImpl> getChildrenRecursive(const ConceptImpl& concept);

  std::vector<InstanceImpl> getInstances(const ConceptImpl& concept);

  int removeInstances(const ConceptImpl& concept);

  int removeInstancesRecursive(const ConceptImpl& concept);

  // MAP BACKERS
  PointImpl addPoint(MapImpl& map, const std::string& name, double x, double y);

  PoseImpl addPose(MapImpl& map, const std::string& name, double x, double y, double theta);

  RegionImpl addRegion(MapImpl& map, const std::string& name, const std::vector<std::pair<double, double>>& points);

  boost::optional<PointImpl> getPoint(MapImpl& map, const std::string& name);

  boost::optional<PoseImpl> getPose(MapImpl& map, const std::string& name);

  boost::optional<RegionImpl> getRegion(MapImpl& map, const std::string& name);

  std::vector<PointImpl> getAllPoints(MapImpl& map);

  std::vector<PoseImpl> getAllPoses(MapImpl& map);

  std::vector<RegionImpl> getAllRegions(MapImpl& map);

  bool renameMap(MapImpl& map, const std::string& new_name);
};

typedef LTMCEntity<LongTermMemoryConduitPostgreSQL> Entity;
typedef LTMCConcept<LongTermMemoryConduitPostgreSQL> Concept;
typedef LTMCInstance<LongTermMemoryConduitPostgreSQL> Instance;
typedef LTMCPoint<LongTermMemoryConduitPostgreSQL> Point;
typedef LTMCPose<LongTermMemoryConduitPostgreSQL> Pose;
typedef LTMCRegion<LongTermMemoryConduitPostgreSQL> Region;
typedef LTMCMap<LongTermMemoryConduitPostgreSQL> Map;
typedef LongTermMemoryConduitPostgreSQL LongTermMemoryConduit;
}  // namespace knowledge_rep
