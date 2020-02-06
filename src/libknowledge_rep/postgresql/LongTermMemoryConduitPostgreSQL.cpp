#include "LongTermMemoryConduitPostgreSQL.h"
#include <knowledge_representation/LongTermMemoryConduitInterface.h>
#include <iostream>
#include <string>
#include <knowledge_representation/LTMCConcept.h>
#include <knowledge_representation/LTMCInstance.h>

using std::string;
using std::vector;
namespace knowledge_rep {

typedef LTMCConcept<LongTermMemoryConduitPostgreSQL> Concept;
typedef LTMCInstance<LongTermMemoryConduitPostgreSQL> Instance;
typedef LTMCEntity<LongTermMemoryConduitPostgreSQL> Entity;

LongTermMemoryConduitPostgreSQL::LongTermMemoryConduitPostgreSQL(const string &db_name)
  : LongTermMemoryConduitInterface<LongTermMemoryConduitPostgreSQL>() {

  conn = std::unique_ptr<pqxx::connection>(
    new pqxx::connection{"postgresql://postgres:nopass@localhost/knowledge_base"});


}

LongTermMemoryConduitPostgreSQL::~LongTermMemoryConduitPostgreSQL() = default;

bool LongTermMemoryConduitPostgreSQL::add_entity(int id) {
  pqxx::work txn{*conn};
  pqxx::result result = txn.exec(
    "INSERT INTO entities VALUES (" + txn.quote(id) + ") ON CONFLICT DO NOTHING RETURNING entity_id");
  txn.commit();
  return result.size() == 1;
}

/**
 * @brief Add a new attribute
 * @param name the name of the attribute
 * @param allowed_types a bitmask representing the types allowed for the attribute
 * @return whether the attribute was added. Note that addition will fail if the attribute already exists.
 */
bool LongTermMemoryConduitPostgreSQL::add_new_attribute(const string &name, const AttributeValueType type) {
  try {
    pqxx::work txn{*conn};
    pqxx::result result = txn.exec(
      "INSERT INTO attributes VALUES (" + txn.quote(name) + ", " + txn.quote(attribute_value_type_to_string[type]) +
      ") ON CONFLICT DO NOTHING");
    txn.commit();
    return result.affected_rows() == 1;
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return false;
  }
}

/// \return true if an entity exists with the given ID
bool LongTermMemoryConduitPostgreSQL::entity_exists(uint id) const {
  pqxx::work txn{*conn};
  // Remove all entities
  auto result = txn.exec("SELECT 1 FROM entities WHERE entity_id=" + txn.quote(id));
  txn.commit();
  return result.size() == 1;
}


vector<Entity> LongTermMemoryConduitPostgreSQL::get_entities_with_attribute_of_value(const string &attribute_name,
                                                                                     const uint other_entity_id) {
  pqxx::work txn{*conn};
  auto result = txn.exec(
    "SELECT entity_id FROM entity_attributes_id WHERE attribute_value=" + txn.quote(other_entity_id) +
    " and attribute_name = " + txn.quote(attribute_name));
  txn.commit();

  vector<Entity> return_result;
  for (const auto &row: result) {
    return_result.emplace_back(row["entity_id"].as<uint>(), *this);
  }
  return return_result;
}

vector<Entity> LongTermMemoryConduitPostgreSQL::get_entities_with_attribute_of_value(const string &attribute_name,
                                                                                     const bool bool_val) {
  pqxx::work txn{*conn};
  auto result = txn.exec(
    "SELECT entity_id FROM entity_attributes_bool WHERE attribute_value=" + txn.quote(bool_val) +
    " and attribute_name = " + txn.quote(attribute_name));
  txn.commit();

  vector<Entity> return_result;
  for (const auto &row: result) {
    return_result.emplace_back(row["entity_id"].as<uint>(), *this);
  }
  return return_result;

}


vector<Entity> LongTermMemoryConduitPostgreSQL::get_entities_with_attribute_of_value(const string &attribute_name,
                                                                                     const string &string_val) {
  pqxx::work txn{*conn};
  auto result = txn.exec(
    "SELECT entity_id FROM entity_attributes_id WHERE attribute_value=" + txn.quote(string_val) +
    " and attribute_name = " + txn.quote(attribute_name));
  txn.commit();

  vector<Entity> return_result;
  for (const auto &row: result) {
    return_result.emplace_back(row["entity_id"].as<uint>(), *this);
  }
  return return_result;

}

std::vector<Entity> LongTermMemoryConduitPostgreSQL::get_all_entities() {
  pqxx::work txn{*conn};

  auto result = txn.exec("TABLE entities");
  txn.commit();

  vector<Entity> entities;
  for (const auto &row: result) {
    entities.emplace_back(row["entity_id"].as<uint>(), *this);
  }
  return entities;
}

/**
 * @brief Remove all attributes except those defined in the schema as defaults
 */
uint LongTermMemoryConduitPostgreSQL::delete_all_attributes() {
  pqxx::work txn{*conn};

  // Remove all entities
  uint num_deleted = txn.exec("DELETE FROM attributes").affected_rows();
  // Use the baked in function to get the default configuration back
  txn.exec("SELECT * FROM add_default_attributes()");
  txn.commit();
  return num_deleted;
}

/**
 * @brief Remove all entities and all entity attributes except for the robot
 */
uint LongTermMemoryConduitPostgreSQL::delete_all_entities() {
  pqxx::work txn{*conn};

  // Remove all entities
  uint num_deleted = txn.exec("DELETE FROM entities").affected_rows();
  // Use the baked in function to get the default configuration back
  txn.exec("SELECT * FROM add_default_entities()");
  txn.commit();
  assert(entity_exists(1));
  return num_deleted;
}

bool LongTermMemoryConduitPostgreSQL::delete_attribute(string &name) {
  pqxx::work txn{*conn};

  // Remove all entities
  uint num_deleted = txn.exec("DELETE FROM attributes WHERE attribute_name = " + txn.quote(name)).affected_rows();
  txn.commit();
  return num_deleted;
}

bool LongTermMemoryConduitPostgreSQL::attribute_exists(const string &name) const {
  pqxx::work txn{*conn};
  auto result = txn.exec("SELECT 1 FROM attributes WHERE attribute_name=" + txn.quote(name));
  txn.commit();
  return result.size() == 1;
}

/**
 * @brief Retrieves a concept of the given name, or creates one with the name if no such instance exists
 * @param name
 * @return the existing concept, or the newly created one. In either case, the instance will at least have the name
 *         passed as a parameter.
 */
Concept LongTermMemoryConduitPostgreSQL::get_concept(const string &name) {
  pqxx::work txn{*conn, "get_concept"};
  string query = "SELECT entity_id FROM entity_attributes_str WHERE attribute_name = 'name' "
                 "AND attribute_value = " + txn.quote(name) + " "
                                                              "AND entity_id IN "
                                                              "(SELECT entity_id FROM entity_attributes_bool "
                                                              "WHERE attribute_name = 'is_concept' AND attribute_value = true)";
  auto result = txn.exec(query);
  txn.commit();

  if (result.empty()) {
    Entity new_concept = add_entity();
    new_concept.add_attribute("name", name);
    new_concept.add_attribute("is_concept", true);
    return {new_concept.entity_id, name, *this};
  } else {
    assert(result.size() == 1);
    return {result[0]["entity_id"].as<uint>(), name, *this};
  }
}


/**
 * @brief Retrieves an instance of the given name, or creates one with the name if no such instance exists
 * @param name
 * @return the existing instance, or the newly created one. In either case, the instance will at least have the name
 *         passed as a parameter.
 */
Instance LongTermMemoryConduitPostgreSQL::get_instance_named(const string &name) {
  pqxx::work txn{*conn, "get_instance_named"};
  string query =
    "SELECT entity_id FROM entity_attributes_str WHERE attribute_name = 'name' "
    "AND attribute_value = " + txn.quote(name) + " "
                                                 "AND entity_id NOT IN "
                                                 "(SELECT entity_id FROM entity_attributes_bool "
                                                 "WHERE attribute_name = 'is_concept' AND attribute_value = true)";

  // If there's no "is_concept" marker on an entity, we assume it is not a concept
  auto q_result = txn.exec(query);
  txn.commit();

  if (q_result.empty()) {
    Instance new_entity = Instance(add_entity().entity_id, *this);
    new_entity.add_attribute("name", name);
    new_entity.add_attribute("is_concept", false);
    return new_entity;
  } else {
    // Can only be one instance with a given name
    assert(q_result.size() == 1);
    return {q_result[0]["entity_id"].as<uint>(), *this};
  }
}

/**
 * @brief Returns an entity with the given ID, if it exists
 * @param entity_id the ID of the entity to fetch
 * @return the entity requested, or an empty optional if no such entity exists
 */
boost::optional<Entity> LongTermMemoryConduitPostgreSQL::get_entity(uint entity_id) {
  if (entity_exists(entity_id)) {
    return Entity{entity_id, *this};
  }
  return {};
}

/**
 * @brief Gets the instance representing the robot
 * @return an instance representing the robot the LTMC is running on
 */
Instance LongTermMemoryConduitPostgreSQL::get_robot() {
  Instance robot = Instance(1, *this);
  assert(robot.is_valid());
  return robot;
}

/**
 * @brief Inserts a new entity into the database. Returns the entity's ID so it can be manipulated with other methods.
 * @return the new entity
 */
Entity LongTermMemoryConduitPostgreSQL::add_entity() {
  pqxx::work txn{*conn, "add_entity"};

  auto result = txn.exec("INSERT INTO entities VALUES (DEFAULT) RETURNING entity_id");
  txn.commit();
  return {result[0]["entity_id"].as<uint>(), *this};
}

/**
 * @brief Queries for all entities that are marked as concepts
 * @return all concepts in the LTMC
 */
std::vector<Concept> LongTermMemoryConduitPostgreSQL::get_all_concepts() {
  pqxx::work txn{*conn, "get_all_concepts"};
  string query =
    "SELECT entity_id FROM entity_attributes_bool "
    "WHERE attribute_name = 'is_concept' "
    "AND attribute_value = true ";
  auto result = txn.exec(query);
  txn.commit();
  vector<Concept> concepts;
  for (const auto &row: result) {
    concepts.emplace_back(row["entity_id"].as<uint>(), *this);
  }
  return concepts;
}

/**
 * @brief Queries for all entities that are identified as instances
 * @return all instances in the LTMC
 */
std::vector<Instance> LongTermMemoryConduitPostgreSQL::get_all_instances() {
  pqxx::work txn{*conn, "get_all_instances"};
  auto result = txn.exec("SELECT entity_id FROM entities WHERE entity_id NOT IN ("
                         "SELECT entity_id FROM entity_attributes_bool "
                         "WHERE attribute_name = 'is_concept' AND attribute_value = true)");
  txn.commit();
  vector<Instance> instances;
  for (const auto &row: result) {
    instances.emplace_back(row["entity_id"].as<uint>(), *this);
  }
  return instances;
}

/**
 * @brief Retrieves all attributes
 * @return a list of tuples. First element of each is the attribute name, the second is the allowed type for the attribute
 */
vector<std::pair<string, AttributeValueType> > LongTermMemoryConduitPostgreSQL::get_all_attributes() const {
  vector<std::pair<string, AttributeValueType> > attribute_names;
  pqxx::work txn{*conn, "get_all_attributes"};
  auto result = txn.exec("TABLE attributes");
  txn.commit();
  for (const auto &row: result) {
    attribute_names.emplace_back(row["attribute_name"].as<string>(),
                                 string_to_attribute_value_type[row["type"].as<string>()]);
  }
  return attribute_names;
}


/**
 * @brief Deletes an entity and any other entities and relations that rely on it.
 * @return true if the entity was deleted. False if it could not be, or already was
 */
bool LongTermMemoryConduitPostgreSQL::delete_entity(Entity &entity) {
  // TODO: Recursively remove entities that are members of directional relations
  if (!entity.is_valid()) {
    return false;
  }
  // Because we've all references to this entity have foreign key relationships with cascade set,
  // this should clear out any references to this entity in other tables as well
  try {
    pqxx::work txn{*conn, "delete_entity"};
    auto result = txn.exec("DELETE FROM entities WHERE entity_id = " + txn.quote(entity.entity_id));
    txn.commit();
    return result.affected_rows() == 1;
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return false;
  }
}

bool LongTermMemoryConduitPostgreSQL::add_attribute(Entity &entity, const std::string &attribute_name,
                                                    const float float_val) {
  try {
    pqxx::work txn{*conn, "add_attribute (float)"};
    auto result = txn.exec(
      "INSERT INTO entity_attributes_float VALUES (" + txn.quote(entity.entity_id) + ", " + txn.quote(attribute_name) +
      ", " + txn.quote(float_val) + ")");
    txn.commit();
    return result.affected_rows() == 1;
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return false;
  }

}


bool
LongTermMemoryConduitPostgreSQL::add_attribute(Entity &entity, const std::string &attribute_name, const bool bool_val) {
  try {
    pqxx::work txn{*conn, "add_attribute (bool)"};
    auto result = txn.exec(
      "INSERT INTO entity_attributes_bool VALUES (" + txn.quote(entity.entity_id) + ", " + txn.quote(attribute_name) +
      ", " + txn.quote(bool_val) + ")");
    txn.commit();
    return result.affected_rows() == 1;
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return false;
  }

}


bool LongTermMemoryConduitPostgreSQL::add_attribute(Entity &entity, const std::string &attribute_name,
                                                    const uint other_entity_id) {
  try {
    pqxx::work txn{*conn, "add_attribute (id)"};
    auto result = txn.exec(
      "INSERT INTO entity_attributes_id VALUES (" + txn.quote(entity.entity_id) + ", " + txn.quote(attribute_name) +
      ", " + txn.quote(other_entity_id) + ")");
    txn.commit();
    return result.affected_rows() == 1;
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return false;
  }

}

bool LongTermMemoryConduitPostgreSQL::add_attribute(Entity &entity, const std::string &attribute_name,
                                                    const std::string &string_val) {
  try {
    pqxx::work txn{*conn, "add_attribute (str)"};
    auto result = txn.exec(
      "INSERT INTO entity_attributes_str VALUES (" + txn.quote(entity.entity_id) + ", " + txn.quote(attribute_name) +
      ", " + txn.quote(string_val) + ")");
    txn.commit();
    return result.affected_rows() == 1;
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return false;
  }

}

int LongTermMemoryConduitPostgreSQL::remove_attribute(Entity &entity, const std::string &attribute_name) {
  int removed_count = 0;
  string query;
  pqxx::work txn{*conn, "remove_attribute"};
  try {
    auto result = txn.exec(
      "SELECT * FROM remove_attribute(" + txn.quote(entity.entity_id) + ", " + txn.quote(attribute_name) +
      ") AS count");
    txn.commit();
    return result[0]["count"].as<int>();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 0;
  }
}

int LongTermMemoryConduitPostgreSQL::remove_attribute_of_value(Entity &entity, const std::string &attribute_name,
                                                               const Entity &other_entity) {
  assert(false);
}

void unwrap_attribute_rows(pqxx::result rows, vector<EntityAttribute> &entity_attributes) {
  pqxx::oid type = rows[0]["attribute_value"].type();
  if (type == 23) {
    for (const auto &row: rows) {
      entity_attributes.emplace_back(row["entity_id"].as<uint>(), row["attribute_name"].as<string>(),
                                     row["attribute_value"].as<int>());
    }
  } else if (type == 1043) {
    for (const auto &row: rows) {
      entity_attributes.emplace_back(row["entity_id"].as<uint>(), row["attribute_name"].as<string>(),
                                     row["attribute_value"].as<string>());
    }
  } else if (type == 701) {
    for (const auto &row: rows) {
      entity_attributes.emplace_back(row["entity_id"].as<uint>(), row["attribute_name"].as<string>(),
                                     row["attribute_value"].as<float>());
    }
  } else if (type == 16) {
    for (const auto &row: rows) {
      entity_attributes.emplace_back(row["entity_id"].as<uint>(), row["attribute_name"].as<string>(),
                                     row["attribute_value"].as<bool>());
    }
  } else {
    assert(false);
  }
}

vector<EntityAttribute>
LongTermMemoryConduitPostgreSQL::get_attributes(const Entity &entity) const {
  vector<EntityAttribute> attributes;
  for (const auto &name: table_names) {
    try {
      pqxx::work txn{*conn, "get_attributes"};
      auto result = txn.exec(
        "SELECT * FROM " + std::string(name) + " WHERE entity_id = " + txn.quote(entity.entity_id));
      txn.commit();
      unwrap_attribute_rows(result, attributes);
    } catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
      return {};
    }
  }
  return attributes;
}

std::vector<EntityAttribute>
LongTermMemoryConduitPostgreSQL::get_attributes(const Entity &entity, const std::string &attribute_name) const {
  vector<EntityAttribute> attributes;
  for (const auto &name: table_names) {
    try {
      pqxx::work txn{*conn, "get_attributes"};
      auto result = txn.exec(
        "SELECT * FROM " + std::string(name) + " WHERE entity_id = " + txn.quote(entity.entity_id) +
        " AND attribute_name = " +
        txn.quote(attribute_name));
      txn.commit();
      unwrap_attribute_rows(result, attributes);
    } catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
      return {};
    }
  }
  return attributes;
}

/**
 * @brief Verifies that the entity still exists in the LTMC
 * An entity can be invalidated if it is explicitly deleted from the LTMC, or if
 * it is removed as a result of other helpers that delete entities.
 * @return whether the entity is valid
 */
bool LongTermMemoryConduitPostgreSQL::is_valid(const Entity &entity) const {
  return entity_exists(entity.entity_id);
}


/**
 * @brief Get all concepts that this instance is transitively an instance of
 * For example, if an entity A "instance_of" concept named apple, and apple "is_a" concept of fruit,
 * then get_concepts will return the concepts of both apple and fruit.
 * @return
 */
std::vector<Concept> LongTermMemoryConduitPostgreSQL::get_concepts(const Instance &instance) {
  try {
    pqxx::work txn{*conn, "get_concepts"};
    auto result = txn.exec("SELECT get_concepts(" + txn.quote(instance.entity_id) + ") AS concept_id");
    txn.commit();
    std::vector<Concept> concepts{};
    for (const auto &row: result) {
      concepts.emplace_back(row["concept_id"].as<uint>(), *this);
    }
    return concepts;
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return {};
  }

}

}

