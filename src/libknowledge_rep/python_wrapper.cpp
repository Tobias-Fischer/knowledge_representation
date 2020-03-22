#include <boost/python.hpp>
#include <boost/python/enum.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>
#include <boost/noncopyable.hpp>
#include <boost/mpl/if.hpp>
#include <boost/optional.hpp>
#include <boost/type_traits/integral_constant.hpp>
#include <boost/utility/in_place_factory.hpp>
#include <knowledge_representation/LongTermMemoryConduitInterface.h>
#include <knowledge_representation/LTMCEntity.h>
#include <knowledge_representation/LTMCConcept.h>
#include <knowledge_representation/LTMCInstance.h>
#include <knowledge_representation/LTMCMap.h>
#include <knowledge_representation/LTMCPose.h>
#include <knowledge_representation/LTMCPoint.h>
#include <knowledge_representation/LTMCRegion.h>
#include <knowledge_representation/convenience.h>
#include <vector>
#include <string>
#include <utility>

namespace python = boost::python;
using knowledge_rep::AttributeValue;
using knowledge_rep::AttributeValueType;
using knowledge_rep::Concept;
using knowledge_rep::Entity;
using knowledge_rep::EntityAttribute;
using knowledge_rep::Instance;
using knowledge_rep::LongTermMemoryConduit;
using knowledge_rep::Map;
using knowledge_rep::Point;
using knowledge_rep::Pose;
using knowledge_rep::Region;
using python::bases;
using python::class_;
using python::enum_;
using python::init;
using python::vector_indexing_suite;
using std::string;
using std::vector;

namespace detail
{
/// @brief Type trait that determines if the provided type is
///        a boost::optional.
template <typename T>
struct IsOptional : boost::false_type
{
};

template <typename T>
struct IsOptional<boost::optional<T>> : boost::true_type
{
};

/// @brief Type used to provide meaningful compiler errors.
template <typename>
struct ReturnOptionalRequiresAOptionalReturnType
{
};

/// @brief ResultConverter model that converts a boost::optional object to
///        Python None if the object is empty (i.e. boost::none) or defers
///        to Boost.Python to convert object to a Python object.
template <typename T>
struct ToPythonOptional
{
  /// @brief Only supports converting Boost.Optional types.
  /// @note This is checked at runtime.
  bool convertible() const
  {
    return detail::IsOptional<T>::value;
  }

  /// @brief Convert boost::optional object to Python None or a
  ///        Boost.Python object.
  PyObject* operator()(const T& obj) const
  {
    namespace python = boost::python;
    python::object result = obj  // If boost::optional has a value, then
                                ?
                                python::object(*obj)  // defer to Boost.Python converter.
                                :
                                python::object();  // Otherwise, return Python None.

    // The python::object contains a handle which functions as
    // smart-pointer to the underlying PyObject.  As it will go
    // out of scope, explicitly increment the PyObject's reference
    // count, as the caller expects a non-borrowed (i.e. owned) reference.
    return python::incref(result.ptr());
  }

  /// @brief Used for documentation.
  const PyTypeObject* get_pytype() const  // NOLINT
  {
    return nullptr;
  }
};

}  // namespace detail

/// @brief Converts a boost::optional to Python None if the object is
///        equal to boost::none.  Otherwise, defers to the registered
///        type converter to returs a Boost.Python object.
struct ReturnOptional
{
  template <class T>
  struct apply  //  NOLINT
  {
    // The to_python_optional ResultConverter only checks if T is convertible
    // at runtime.  However, the following MPL branch cause a compile time
    // error if T is not a boost::optional by providing a type that is not a
    // ResultConverter model.
    typedef typename boost::mpl::if_<detail::IsOptional<T>, detail::ToPythonOptional<T>,
                                     detail::ReturnOptionalRequiresAOptionalReturnType<T>>::type type;
  };  // apply
};    // return_optional

BOOST_PYTHON_MODULE(_libknowledge_rep_wrapper_cpp)
{
  typedef LongTermMemoryConduit LTMC;

  class_<std::pair<string, string>>("StringPair")
      .def_readwrite("first", &std::pair<string, string>::first)
      .def_readwrite("second", &std::pair<string, string>::second);

  class_<std::pair<double, double>>("DoublePair")
      .def_readwrite("first", &std::pair<double, double>::first)
      .def_readwrite("second", &std::pair<double, double>::second);

  class_<vector<std::pair<string, string>>>("PyPairList")
      .def(vector_indexing_suite<vector<std::pair<string, string>>>());

  class_<vector<std::pair<double, double>>>("PyDoublePairList")
      .def(vector_indexing_suite<vector<std::pair<double, double>>>());

  class_<vector<Entity>>("PyEntityList").def(vector_indexing_suite<vector<Entity>>());

  class_<vector<Concept>>("PyConceptList").def(vector_indexing_suite<vector<Concept>>());

  class_<vector<Instance>>("PyInstanceList").def(vector_indexing_suite<vector<Instance>>());

  class_<vector<Point>>("PyPointList").def(vector_indexing_suite<vector<Point>>());

  class_<vector<Pose>>("PyPoseList").def(vector_indexing_suite<vector<Pose>>());

  enum_<AttributeValueType>("AttributeValueType")
      .value("int", AttributeValueType::Int)
      .value("str", AttributeValueType::Str)
      .value("bool", AttributeValueType::Bool)
      .value("float", AttributeValueType::Float);

  class_<Entity>("Entity", init<uint, LTMC&>())
      .def_readonly("entity_id", &Entity::entity_id)
      .def("add_attribute_str", static_cast<bool (Entity::*)(const string&, const string&)>(&Entity::addAttribute))
      .def("add_attribute_int", static_cast<bool (Entity::*)(const string&, uint)>(&Entity::addAttribute))
      .def("add_attribute_float", static_cast<bool (Entity::*)(const string&, float)>(&Entity::addAttribute))
      .def("add_attribute_bool", static_cast<bool (Entity::*)(const string&, bool)>(&Entity::addAttribute))
      .def("add_attribute_entity", static_cast<bool (Entity::*)(const string&, const Entity&)>(&Entity::addAttribute))
      .def("remove_attribute", &Entity::removeAttribute)
      .def("get_attributes",
           static_cast<vector<EntityAttribute> (Entity::*)(const string&) const>(&Entity::getAttributes))
      .def("get_attributes", static_cast<vector<EntityAttribute> (Entity::*)() const>(&Entity::getAttributes))
      .def("delete", &Entity::deleteEntity)
      .def("is_valid", &Entity::isValid)
      .def("__getitem__", &Entity::operator[]);

  class_<Concept, bases<Entity>>("Concept", init<uint, string, LTMC&>())
      .def("remove_instances", &Concept::removeInstances)
      .def("remove_instances_recursive", &Concept::removeInstancesRecursive)
      .def("remove_references", &Concept::removeReferences)
      .def("get_instances", &Concept::getInstances)
      .def("get_name", &Concept::getName)
      .def("get_children", &Concept::getChildren)
      .def("get_children_recursive", &Concept::getChildrenRecursive)
      .def("create_instance", static_cast<Instance (Concept::*)() const>(&Concept::createInstance))
      .def("create_instance",
           static_cast<boost::optional<Instance> (Concept::*)(const string&) const>(&Concept::createInstance),
           python::return_value_policy<ReturnOptional>());

  class_<Instance, bases<Entity>>("Instance", init<uint, LTMC&>())
      .def("make_instance_of", &Instance::makeInstanceOf)
      .def("get_name", &Instance::getName, python::return_value_policy<ReturnOptional>())
      .def("get_concepts", &Instance::getConcepts)
      .def("get_concepts_recursive", &Instance::getConceptsRecursive)
      .def("has_concept", &Instance::hasConcept)
      .def("has_concept_recursively", &Instance::hasConceptRecursively);

  class_<EntityAttribute>("EntityAttribute", init<uint, string, AttributeValue>())
      .def_readonly("entity_id", &EntityAttribute::entity_id)
      .def_readonly("attribute_name", &EntityAttribute::attribute_name)
      .def("get_int_value", &EntityAttribute::getIntValue)
      .def("get_float_value", &EntityAttribute::getFloatValue)
      .def("get_bool_value", &EntityAttribute::getBoolValue)
      .def("get_string_value", &EntityAttribute::getStringValue);

  class_<vector<EntityAttribute>>("PyAttributeList").def(vector_indexing_suite<vector<EntityAttribute>>());

  class_<Map, bases<Instance>>("Map", init<uint, string, LTMC&>())
      .def("add_point", &Map::addPoint)
      .def("add_region", &Map::addRegion)
      .def("add_pose", &Map::addPose)
      .def("get_point", &Map::getPoint, python::return_value_policy<ReturnOptional>())
      .def("get_pose", &Map::getPose, python::return_value_policy<ReturnOptional>())
      .def("get_region", &Map::getRegion, python::return_value_policy<ReturnOptional>())
      .def("get_all_points", &Map::getAllPoints)
      .def("get_all_poses", &Map::getAllPoses)
      .def("get_all_regions", &Map::getAllRegions);

  class_<Point, bases<Instance>>("Point", init<uint, string, double, double, Map, LTMC&>())
      .def_readonly("x", &Point::x)
      .def_readonly("y", &Point::y)
      .def("get_containing_regions", &Point::getContainingRegions);

  class_<Pose, bases<Instance>>("Pose", init<uint, string, double, double, double, Map, LTMC&>())
      .def_readonly("x", &Pose::x)
      .def_readonly("y", &Pose::y)
      .def_readonly("theta", &Pose::theta)
      .def("get_containing_regions", &Pose::getContainingRegions);

  class_<Region, bases<Instance>>("Region", init<uint, string, Map, LTMC&>())
      .def_readonly("points", &Region::points)
      .def("get_contained_points", &Region::getContainedPoints)
      .def("get_contained_poses", &Region::getContainedPoses);

  class_<LongTermMemoryConduit, boost::noncopyable>("LongTermMemoryConduit", init<const string&>())
      .def("add_entity", static_cast<Entity (LTMC::*)()>(&LTMC::addEntity))
      .def("add_new_attribute", &LTMC::addNewAttribute)
      .def("entity_exists", &LTMC::entityExists)
      .def("attribute_exists", &LTMC::attributeExists)
      .def("delete_all_entities", &LTMC::deleteAllEntities)
      .def("delete_all_attributes", &LTMC::deleteAllAttributes)
      .def("get_entities_with_attribute_of_value",
           static_cast<vector<Entity> (LTMC::*)(const string&, const uint)>(&LTMC::getEntitiesWithAttributeOfValue))
      .def("select_query_int",
           static_cast<bool (LTMC::*)(const string&, vector<EntityAttribute>&) const>(&LTMC::selectQueryInt))
      .def("select_query_bool",
           static_cast<bool (LTMC::*)(const string&, vector<EntityAttribute>&) const>(&LTMC::selectQueryBool))
      .def("select_query_float",
           static_cast<bool (LTMC::*)(const string&, vector<EntityAttribute>&) const>(&LTMC::selectQueryFloat))
      .def("select_query_string",
           static_cast<bool (LTMC::*)(const string&, vector<EntityAttribute>&) const>(&LTMC::selectQueryString))
      .def("get_map", &LTMC::getMap)
      .def("get_all_entities", &LTMC::getAllEntities)
      .def("get_all_concepts", &LTMC::getAllConcepts)
      .def("get_all_instances", &LTMC::getAllInstances)
      .def("get_all_attribute_names", &LTMC::getAllAttributes)
      .def("get_concept", &LTMC::getConcept)
      .def("get_instance_named", &LTMC::getInstanceNamed)
      .def("get_robot", &LTMC::getRobot);
}
