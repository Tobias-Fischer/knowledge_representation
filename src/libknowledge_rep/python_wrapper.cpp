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
using python::tuple;
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

template <typename T1, typename T2>
struct PairToPythonConverter
{
  static PyObject* convert(const std::pair<T1, T2>& pair)
  {
    return boost::python::incref(boost::python::make_tuple(pair.first, pair.second).ptr());
  }
};

template <typename T1, typename T2>
struct PythonToPairConverter
{
  using pair_type = std::pair<T1, T2>;
  PythonToPairConverter()
  {
    boost::python::converter::registry::push_back(&convertible, &construct, boost::python::type_id<pair_type>());
  }
  static void* convertible(PyObject* obj)
  {
    if (!PyTuple_CheckExact(obj))
      return 0;
    if (PyTuple_Size(obj) != 2)
      return 0;
    return obj;
  }
  static void construct(PyObject* obj, boost::python::converter::rvalue_from_python_stage1_data* data)
  {
    tuple tuple(boost::python::borrowed(obj));
    void* storage = ((boost::python::converter::rvalue_from_python_storage<pair_type>*)data)->storage.bytes;
    new (storage) pair_type(boost::python::extract<T1>(tuple[0]), boost::python::extract<T2>(tuple[1]));
    data->convertible = storage;
  }
};

template <typename T1, typename T2>
struct py_pair
{
  boost::python::to_python_converter<std::pair<T1, T2>, PairToPythonConverter<T1, T2>> toPy;
  PythonToPairConverter<T1, T2> fromPy;
};

/// @brief Type that allows for registration of conversions from
///        python iterable types.
struct iterable_converter
{
  /// @note Registers converter from a python interable type to the
  ///       provided type.
  template <typename Container>
  iterable_converter& from_python()
  {
    boost::python::converter::registry::push_back(&iterable_converter::convertible,
                                                  &iterable_converter::construct<Container>,
                                                  boost::python::type_id<Container>());

    // Support chaining.
    return *this;
  }

  /// @brief Check if PyObject is iterable.
  static void* convertible(PyObject* object)
  {
    return PyObject_GetIter(object) ? object : NULL;
  }

  /// @brief Convert iterable PyObject to C++ container type.
  ///
  /// Container Concept requirements:
  ///
  ///   * Container::value_type is CopyConstructable.
  ///   * Container can be constructed and populated with two iterators.
  ///     I.e. Container(begin, end)
  template <typename Container>
  static void construct(PyObject* object, boost::python::converter::rvalue_from_python_stage1_data* data)
  {
    namespace python = boost::python;
    // Object is a borrowed reference, so create a handle indicting it is
    // borrowed for proper reference counting.
    python::handle<> handle(python::borrowed(object));

    // Obtain a handle to the memory block that the converter has allocated
    // for the C++ type.
    typedef python::converter::rvalue_from_python_storage<Container> storage_type;
    void* storage = reinterpret_cast<storage_type*>(data)->storage.bytes;

    typedef python::stl_input_iterator<typename Container::value_type> iterator;

    // Allocate the C++ type into the converter's memory block, and assign
    // its handle to the converter's convertible variable.  The C++
    // container is populated by passing the begin and end iterators of
    // the python object to the container's constructor.
    new (storage) Container(iterator(python::object(handle)),  // begin
                            iterator());                       // end
    data->convertible = storage;
  }
};

BOOST_PYTHON_MODULE(_libknowledge_rep_wrapper_cpp)
{
  typedef LongTermMemoryConduit LTMC;

  // Register a converter to get Python tuples turned into std::pairs
  py_pair<double, double>();

  // No proxy must be set to true for the contained elements to be converted to tuples on demand
  class_<vector<Region::Point2D>>("PyDoublePairList").def(vector_indexing_suite<vector<Region::Point2D>, true>());

  // Automatically convert Python lists into vectors
  iterable_converter().from_python<std::vector<Region::Point2D>>();

  class_<vector<Entity>>("PyEntityList").def(vector_indexing_suite<vector<Entity>>());

  class_<vector<Concept>>("PyConceptList").def(vector_indexing_suite<vector<Concept>>());

  class_<vector<Instance>>("PyInstanceList").def(vector_indexing_suite<vector<Instance>>());

  class_<vector<Point>>("PyPointList").def(vector_indexing_suite<vector<Point>>());

  class_<vector<Pose>>("PyPoseList").def(vector_indexing_suite<vector<Pose>>());

  class_<vector<Region>>("PyRegionList").def(vector_indexing_suite<vector<Region>>());

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
      .def("__getitem__", &Entity::operator[])
      .def("__eq__", &Entity::operator==)
      .def("__ne__", &Entity::operator!=);

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

  class_<Map, bases<Instance>>("Map", init<uint, uint, string, LTMC&>())
      .def("add_point", &Map::addPoint)
      .def("add_region", &Map::addRegion)
      .def("add_pose", &Map::addPose)
      .def("get_point", &Map::getPoint, python::return_value_policy<ReturnOptional>())
      .def("get_pose", &Map::getPose, python::return_value_policy<ReturnOptional>())
      .def("get_region", &Map::getRegion, python::return_value_policy<ReturnOptional>())
      .def("get_all_points", &Map::getAllPoints)
      .def("get_all_poses", &Map::getAllPoses)
      .def("get_all_regions", &Map::getAllRegions)
      .def("rename", &Map::rename);

  class_<Point, bases<Instance>>("Point", init<uint, string, double, double, Map, LTMC&>())
      .def_readonly("x", &Point::x)
      .def_readonly("y", &Point::y)
      .def("get_containing_regions", &Point::getContainingRegions);

  class_<Pose, bases<Instance>>("Pose", init<uint, string, double, double, double, Map, LTMC&>())
      .def_readonly("x", &Pose::x)
      .def_readonly("y", &Pose::y)
      .def_readonly("theta", &Pose::theta)
      .def("get_containing_regions", &Pose::getContainingRegions);

  class_<Region, bases<Instance>>("Region", init<uint, string, const vector<Region::Point2D>, Map, LTMC&>())
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
