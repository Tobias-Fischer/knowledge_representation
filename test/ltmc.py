#!/usr/bin/env python
import sys
import unittest
from knowledge_representation import PyAttributeList, AttributeValueType
import knowledge_representation

ltmc = knowledge_representation.get_default_ltmc()


class TestLTMC(unittest.TestCase):

    def setUp(self):
        ltmc.delete_all_attributes()
        ltmc.delete_all_entities()

    def tearDown(self):
        ltmc.delete_all_attributes()
        ltmc.delete_all_entities()

    def test_add_entity_works(self):
        coke = ltmc.add_entity()
        self.assertTrue(coke.is_valid())
        self.assertTrue(ltmc.entity_exists(coke.entity_id))

    def test_get_concept(self):
        nsb_concept = ltmc.get_concept("never seen before")
        nsb_concept.remove_instances()
        self.assertEqual(len(nsb_concept.get_instances()), 0)

    def test_create_instance(self):
        nsb_concept = ltmc.get_concept("never seen before")
        instance = nsb_concept.create_instance()
        self.assertTrue(instance)

    def test_get_entities_with_attribute_of_value(self):
        nsb_concept = ltmc.get_concept("never seen before")
        instance = nsb_concept.create_instance()
        instance_list = ltmc.get_entities_with_attribute_of_value("is_open", nsb_concept.entity_id)
        self.assertEqual(len(instance_list),  0)

    def test_remove_instances(self):
        nsb_concept = ltmc.get_concept("never seen before")
        nsb_concept.remove_instances()
        self.assertEqual(len(nsb_concept.get_instances()), 0)

    def test_select_query(self):
        result_list = PyAttributeList()
        ltmc.select_query_string("SELECT * FROM entity_attributes_str", result_list)

    def test_add_attribute(self):
        nsb_concept = ltmc.get_concept("never seen before")
        instance = nsb_concept.create_instance("nsb")
        self.assertTrue(instance)

        result = ltmc.add_new_attribute("new attribute", AttributeValueType.str)
        self.assertTrue(result)
        self.assertTrue(ltmc.attribute_exists("new attribute"))
        added = instance.add_attribute_str("new attribute", "value of attribute")
        self.assertTrue(added)
        self.assertEqual("value of attribute", instance["new attribute"][0].get_string_value())

    def test_map_types(self):
        map = ltmc.get_map("test map")
        self.assertEqual("test map", map.get_name())
        point = map.add_point("test", 0, 1)
        self.assertTrue(point)
        self.assertEqual("test", point.get_name())

        pose = map.add_pose("test pose", 0, 1, 2)
        self.assertTrue(pose)
        self.assertEqual(0, pose.x)
        self.assertEqual(1, pose.y)
        self.assertEqual(2, pose.theta)
        self.assertEqual("test pose", pose.get_name())


if __name__ == '__main__':
    import rosunit

    rosunit.unitrun("knowledge_representation", 'test_bare_bones', TestLTMC)
