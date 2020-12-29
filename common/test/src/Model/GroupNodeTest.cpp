/*
 Copyright (C) 2020 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Model/Brush.h"
#include "Model/BrushNode.h"
#include "Model/Entity.h"
#include "Model/EntityNode.h"
#include "Model/Group.h"
#include "Model/GroupNode.h"

#include <vecmath/bbox.h>
#include <vecmath/bbox_io.h>
#include <vecmath/mat.h>
#include <vecmath/mat_ext.h>
#include <vecmath/mat_io.h>

#include <kdl/result.h>

#include <memory>
#include <vector>

#include "Catch2.h"

namespace TrenchBroom {
    namespace Model {
        TEST_CASE("GroupNodeTest.constructor", "[GroupNodeTest]") {
            auto groupNode = GroupNode(Group("name"));
            CHECK_FALSE(groupNode.linked());
            CHECK_THAT(groupNode.linkedGroups(), Catch::UnorderedEquals(std::vector<GroupNode*>{}));
        }

        TEST_CASE("GroupNodeTest.clone", "[GroupNodeTest]") {
            const auto worldBounds = vm::bbox3d(8192.0);
            auto groupNode = GroupNode(Group("name"));
            
            auto groupNodeClone = std::unique_ptr<GroupNode>{static_cast<GroupNode*>(groupNode.clone(worldBounds))};
            CHECK_FALSE(groupNodeClone->inLinkSetWith(groupNode));
        }

        TEST_CASE("GroupNodeTest.cloneRecursively", "[GroupNodeTest]") {
            const auto worldBounds = vm::bbox3d(8192.0);
            auto groupNode = GroupNode(Group("name"));
            
            auto groupNodeClone = std::unique_ptr<GroupNode>{static_cast<GroupNode*>(groupNode.cloneRecursively(worldBounds))};
            CHECK_FALSE(groupNodeClone->inLinkSetWith(groupNode));
        }

        TEST_CASE("GroupNodeTest.inLinkSetWith", "[GroupNodeTest]") {
            const auto worldBounds = vm::bbox3d(8192.0);
            
            auto groupNode = GroupNode(Group("name"));
            CHECK(groupNode.inLinkSetWith(groupNode));

            auto* entityNode = new EntityNode();
            groupNode.addChild(entityNode);
            REQUIRE(groupNode.childCount() == 1u);

            auto groupNodeClone = std::unique_ptr<GroupNode>{static_cast<GroupNode*>(groupNode.cloneRecursively(worldBounds))};
            CHECK(!groupNode.inLinkSetWith(*groupNodeClone));
            CHECK(!groupNodeClone->inLinkSetWith(groupNode));
            groupNode.addToLinkSet(*groupNodeClone);
            CHECK(groupNode.inLinkSetWith(*groupNodeClone));
            CHECK(groupNodeClone->inLinkSetWith(groupNode));

            auto otherGroupNode = GroupNode(Group("other"));
            CHECK_FALSE(otherGroupNode.inLinkSetWith(groupNode));
            CHECK_FALSE(groupNode.inLinkSetWith(otherGroupNode));
            CHECK_FALSE(otherGroupNode.inLinkSetWith(*groupNodeClone));
            CHECK_FALSE(groupNodeClone->inLinkSetWith(otherGroupNode));
        }

        TEST_CASE("GroupNodeTest.addToLinkSet", "[GroupNodeTest]") {
            const auto worldBounds = vm::bbox3d(8192.0);
            
            auto groupNode = GroupNode(Group("name"));
            auto* entityNode = new EntityNode();
            groupNode.addChild(entityNode);
            REQUIRE(groupNode.childCount() == 1u);
            
            auto groupNodeClone = std::unique_ptr<GroupNode>{static_cast<GroupNode*>(groupNode.cloneRecursively(worldBounds))};
            REQUIRE(!groupNodeClone->inLinkSetWith(groupNode));
            REQUIRE(!groupNodeClone->linked());

            groupNode.addToLinkSet(*groupNodeClone);
            CHECK(groupNodeClone->inLinkSetWith(groupNode));
            CHECK(!groupNodeClone->linked());
        }

        TEST_CASE("GroupNodeTest.link", "[GroupNodeTest]") {
            const auto worldBounds = vm::bbox3d(8192.0);
            
            auto groupNode = GroupNode(Group("name"));
            groupNode.link();

            auto groupNodeClone = std::unique_ptr<GroupNode>{static_cast<GroupNode*>(groupNode.cloneRecursively(worldBounds))};
            groupNode.addToLinkSet(*groupNodeClone);
            REQUIRE(!groupNodeClone->linked());

            groupNodeClone->link();
            CHECK(groupNodeClone->linked());
            CHECK_THAT(groupNodeClone->linkedGroups(), Catch::UnorderedEquals(std::vector<GroupNode*>{&groupNode, groupNodeClone.get()}));

            groupNodeClone->unlink();
            CHECK_FALSE(groupNodeClone->linked());
            CHECK_THAT(groupNodeClone->linkedGroups(), Catch::UnorderedEquals(std::vector<GroupNode*>{&groupNode}));
        }

        static void transform(Node& node, const vm::mat4x4& transformation, const vm::bbox3& worldBounds) {
            node.accept(kdl::overload(
                [](const WorldNode*) {},
                [](const LayerNode*) {},
                [&](auto&& thisLambda, GroupNode* groupNode) {
                    auto group = groupNode->group();
                    group.transform(transformation);
                    groupNode->setGroup(std::move(group));

                    groupNode->visitChildren(thisLambda);
                },
                [&](auto&& thisLambda, EntityNode* entityNode) {
                    auto entity = entityNode->entity();
                    entity.transform(transformation);
                    entityNode->setEntity(std::move(entity));

                    entityNode->visitChildren(thisLambda);
                },
                [&](BrushNode* brushNode) {
                    auto brush = brushNode->brush();
                    REQUIRE(brush.transform(worldBounds, transformation, false).is_success());
                    brushNode->setBrush(std::move(brush));
                }
            ));
        }

        TEST_CASE("GroupNodeTest.transform", "[GroupNodeTest]") {
            const auto worldBounds = vm::bbox3(8192.0);

            auto groupNode = GroupNode(Group("name"));
            REQUIRE(groupNode.group().transformation() == vm::mat4x4());

            auto* entityNode = new EntityNode();
            groupNode.addChild(entityNode);
            
            transform(groupNode, vm::translation_matrix(vm::vec3(32.0, 0.0, 0.0)), worldBounds);
            CHECK(groupNode.group().transformation() == vm::translation_matrix(vm::vec3(32.0, 0.0, 0.0)));

            transform(groupNode, vm::rotation_matrix(0.0, 0.0, vm::to_radians(90.0)), worldBounds);
            CHECK(groupNode.group().transformation() == vm::rotation_matrix(0.0, 0.0, vm::to_radians(90.0)) * vm::translation_matrix(vm::vec3(32.0, 0.0, 0.0)));

            auto testEntityNode = EntityNode();
            transform(testEntityNode, groupNode.group().transformation(), worldBounds);

            CHECK(testEntityNode.entity() == entityNode->entity());
        }

        TEST_CASE("GroupNodeTest.updateLinkedGroups", "[GroupNodeTest]") {
            const auto worldBounds = vm::bbox3(8192.0);
            
            auto groupNode = GroupNode(Group("name"));
            groupNode.link();

            auto* entityNode = new EntityNode();
            groupNode.addChild(entityNode);

            transform(groupNode, vm::translation_matrix(vm::vec3(1.0, 0.0, 0.0)), worldBounds);
            REQUIRE(groupNode.group().transformation() == vm::translation_matrix(vm::vec3(1.0, 0.0, 0.0)));
            REQUIRE(entityNode->entity().origin() == vm::vec3(1.0, 0.0, 0.0));

            SECTION("Update linked groups of a singleton group") {
                const auto updateResult = groupNode.updateLinkedGroups(worldBounds);
                updateResult.visit(kdl::overload(
                    [&](const UpdateLinkedGroupsResult& r) {
                        CHECK(r.empty());
                    },
                    [](const UpdateLinkedGroupsError&) {
                        FAIL();
                    }
                ));
            }

            SECTION("Update linked groups of a non-singleton group") {
                auto groupNodeClone = std::unique_ptr<GroupNode>{static_cast<GroupNode*>(groupNode.cloneRecursively(worldBounds))};
                REQUIRE(groupNodeClone->group().transformation() == vm::translation_matrix(vm::vec3(1.0, 0.0, 0.0)));
                groupNode.addToLinkSet(*groupNodeClone);
                groupNodeClone->link();

                transform(*groupNodeClone, vm::translation_matrix(vm::vec3(0.0, 2.0, 0.0)), worldBounds);
                REQUIRE(groupNodeClone->group().transformation() == vm::translation_matrix(vm::vec3(1.0, 2.0, 0.0)));
                REQUIRE(static_cast<EntityNode*>(groupNodeClone->children().front())->entity().origin() == vm::vec3(1.0, 2.0, 0.0));


                transform(*entityNode, vm::translation_matrix(vm::vec3(0.0, 0.0, 3.0)), worldBounds);
                REQUIRE(entityNode->entity().origin() == vm::vec3(1.0, 0.0, 3.0));

                const auto updateResult = groupNode.updateLinkedGroups(worldBounds);
                updateResult.visit(kdl::overload(
                    [&](const UpdateLinkedGroupsResult& r) {
                        CHECK(r.size() == 1u);

                        const auto& p = r.front();
                        const auto& [oldGroupNode, replacementNode] = p;

                        CHECK(oldGroupNode == groupNodeClone.get());

                        CHECK(replacementNode->inLinkSetWith(groupNode));
                        CHECK(replacementNode->group() == groupNodeClone->group());
                        CHECK(replacementNode->childCount() == 1u);

                        const auto* newEntityNode = dynamic_cast<EntityNode*>(replacementNode->children().front());
                        CHECK(newEntityNode != nullptr);

                        CHECK(newEntityNode->entity().origin() == vm::vec3(1.0, 2.0, 3.0));
                    },
                    [](const UpdateLinkedGroupsError&) {
                        FAIL();
                    }
                ));
            }
        }

        TEST_CASE("GroupNodeTest.updateNestedLinkedGroup", "[GroupNodeTest]") {
            const auto worldBounds = vm::bbox3(8192.0);
            
            auto outerGroupNode = GroupNode(Group("outer"));
            outerGroupNode.link();

            auto* innerGroupNode = new GroupNode(Group("inner"));
            outerGroupNode.addChild(innerGroupNode);
            innerGroupNode->link();

            auto* innerGroupEntityNode = new EntityNode();
            innerGroupNode->addChild(innerGroupEntityNode);

            auto innerGroupNodeClone = std::unique_ptr<GroupNode>{static_cast<GroupNode*>(innerGroupNode->cloneRecursively(worldBounds))};
            REQUIRE(innerGroupNodeClone->group().transformation() == vm::mat4x4());
            innerGroupNode->addToLinkSet(*innerGroupNodeClone);
            innerGroupNodeClone->link();

            transform(*innerGroupNodeClone, vm::translation_matrix(vm::vec3(0.0, 2.0, 0.0)), worldBounds);
            REQUIRE(innerGroupNodeClone->group().transformation() == vm::translation_matrix(vm::vec3(0.0, 2.0, 0.0)));

            SECTION("Transforming the outer group node and updating the linked group") {
                transform(outerGroupNode, vm::translation_matrix(vm::vec3(1.0, 0.0, 0.0)), worldBounds);
                REQUIRE(outerGroupNode.group().transformation() == vm::translation_matrix(vm::vec3(1.0, 0.0, 0.0)));
                REQUIRE(innerGroupNode->group().transformation() == vm::translation_matrix(vm::vec3(1.0, 0.0, 0.0)));
                REQUIRE(innerGroupEntityNode->entity().origin() == vm::vec3(1.0, 0.0, 0.0));
                REQUIRE(innerGroupNodeClone->group().transformation() == vm::translation_matrix(vm::vec3(0.0, 2.0, 0.0)));

                const auto updateResult = outerGroupNode.updateLinkedGroups(worldBounds);
                updateResult.visit(kdl::overload(
                    [&](const UpdateLinkedGroupsResult& r) {
                        CHECK(r.empty());
                    },
                    [](const UpdateLinkedGroupsError&) {
                        FAIL();
                    }
                ));
            }

            SECTION("Transforming the inner group node and updating the linked group") {
                transform(*innerGroupNode, vm::translation_matrix(vm::vec3(1.0, 0.0, 0.0)), worldBounds);
                REQUIRE(outerGroupNode.group().transformation() == vm::mat4x4());
                REQUIRE(innerGroupNode->group().transformation() == vm::translation_matrix(vm::vec3(1.0, 0.0, 0.0)));
                REQUIRE(innerGroupEntityNode->entity().origin() == vm::vec3(1.0, 0.0, 0.0));
                REQUIRE(innerGroupNodeClone->group().transformation() == vm::translation_matrix(vm::vec3(0.0, 2.0, 0.0)));

                const auto updateResult = innerGroupNode->updateLinkedGroups(worldBounds);
                updateResult.visit(kdl::overload(
                    [&](const UpdateLinkedGroupsResult& r) {
                        CHECK(r.size() == 1u);

                        const auto& p = r.front();
                        const auto& [oldGroupNode, replacementNode] = p;

                        CHECK(oldGroupNode == innerGroupNodeClone.get());

                        CHECK(replacementNode->inLinkSetWith(*innerGroupNode));
                        CHECK(replacementNode->group() == innerGroupNodeClone->group());
                        CHECK(replacementNode->childCount() == 1u);

                        const auto* newEntityNode = dynamic_cast<EntityNode*>(replacementNode->children().front());
                        CHECK(newEntityNode != nullptr);

                        CHECK(newEntityNode->entity().origin() == vm::vec3(0.0, 2.0, 0.0));
                    },
                    [](const UpdateLinkedGroupsError&) {
                        FAIL();
                    }
                ));
            }

            SECTION("Transforming the inner group node's entity and updating the linked group") {
                transform(*innerGroupEntityNode, vm::translation_matrix(vm::vec3(1.0, 0.0, 0.0)), worldBounds);
                REQUIRE(outerGroupNode.group().transformation() == vm::mat4x4());
                REQUIRE(innerGroupNode->group().transformation() == vm::mat4x4());
                REQUIRE(innerGroupEntityNode->entity().origin() == vm::vec3(1.0, 0.0, 0.0));
                REQUIRE(innerGroupNodeClone->group().transformation() == vm::translation_matrix(vm::vec3(0.0, 2.0, 0.0)));

                const auto updateResult = innerGroupNode->updateLinkedGroups(worldBounds);
                updateResult.visit(kdl::overload(
                    [&](const UpdateLinkedGroupsResult& r) {
                        CHECK(r.size() == 1u);

                        const auto& p = r.front();
                        const auto& [oldGroupNode, replacementNode] = p;

                        CHECK(oldGroupNode == innerGroupNodeClone.get());

                        CHECK(replacementNode->inLinkSetWith(*innerGroupNode));
                        CHECK(replacementNode->group() == innerGroupNodeClone->group());
                        CHECK(replacementNode->childCount() == 1u);

                        const auto* newEntityNode = dynamic_cast<EntityNode*>(replacementNode->children().front());
                        CHECK(newEntityNode != nullptr);

                        CHECK(newEntityNode->entity().origin() == vm::vec3(1.0, 2.0, 0.0));
                    },
                    [](const UpdateLinkedGroupsError&) {
                        FAIL();
                    }
                ));
            }
        }

        TEST_CASE("GroupNodeTest.updateLinkedGroupsRecursively", "[GroupNodeTest]") {
            const auto worldBounds = vm::bbox3(8192.0);
            
            auto outerGroupNode = GroupNode(Group("outer"));
            outerGroupNode.link();

            /*
            outerGroupNode
            */

            auto* innerGroupNode = new GroupNode(Group("inner"));
            outerGroupNode.addChild(innerGroupNode);
            innerGroupNode->link();

            /*
            outerGroupNode
            +- innerGroupNode
            */

            auto* innerGroupEntityNode = new EntityNode();
            innerGroupNode->addChild(innerGroupEntityNode);

            /*
            outerGroupNode
            +-innerGroupNode
               +-innerGroupEntityNode
            */

            auto outerGroupNodeClone = std::unique_ptr<GroupNode>{static_cast<GroupNode*>(outerGroupNode.cloneRecursively(worldBounds))};
            REQUIRE(outerGroupNodeClone->group().transformation() == vm::mat4x4());
            REQUIRE(outerGroupNodeClone->childCount() == 1u);
            outerGroupNode.addToLinkSet(*outerGroupNodeClone);
            outerGroupNodeClone->link();

            /*
            outerGroupNode
            +-innerGroupNode
               +-innerGroupEntityNode
            outerGroupNodeClone
            +-innerGroupNodeClone
               +-innerGroupEntityNodeClone
            */

            auto* innerGroupNodeClone = dynamic_cast<GroupNode*>(outerGroupNodeClone->children().front());
            REQUIRE(innerGroupNodeClone != nullptr);
            REQUIRE(innerGroupNodeClone->childCount() == 1u);

            auto* innerGroupEntityNodeClone = dynamic_cast<EntityNode*>(innerGroupNodeClone->children().front());
            REQUIRE(innerGroupEntityNodeClone != nullptr);

            const auto updateResult = outerGroupNode.updateLinkedGroups(worldBounds);
            updateResult.visit(kdl::overload(
                [&](const UpdateLinkedGroupsResult& r) {
                    REQUIRE(r.size() == 1u);
                    const auto& [originalNode, replacementNode] = r.front();

                    REQUIRE(originalNode == outerGroupNodeClone.get());
                    REQUIRE(replacementNode->group() == originalNode->group());
                    REQUIRE(replacementNode->childCount() == 1u);

                    auto* newInnerGroupNodeClone = dynamic_cast<GroupNode*>(replacementNode->children().front());
                    CHECK(newInnerGroupNodeClone != nullptr);
                    CHECK(newInnerGroupNodeClone->group() == innerGroupNode->group());
                    CHECK(newInnerGroupNodeClone->childCount() == 1u);

                    auto* newInnerGroupEntityNodeClone = dynamic_cast<EntityNode*>(newInnerGroupNodeClone->children().front());
                    CHECK(newInnerGroupEntityNodeClone != nullptr);
                    CHECK(*newInnerGroupEntityNodeClone == *innerGroupEntityNode);
                },
                [](const UpdateLinkedGroupsError&) {
                    FAIL();
                }
            ));
        }

        TEST_CASE("GroupNodeTest.updateLinkedGroupsExceedsWorldBounds", "[GroupNodeTest]") {
            const auto worldBounds = vm::bbox3(8192.0);
            
            auto groupNode = GroupNode(Group("name"));
            groupNode.link();

            auto* entityNode = new EntityNode();
            groupNode.addChild(entityNode);


            auto groupNodeClone = std::unique_ptr<GroupNode>{static_cast<GroupNode*>(groupNode.cloneRecursively(worldBounds))};
            groupNode.addToLinkSet(*groupNodeClone);
            groupNodeClone->link();

            transform(*groupNodeClone, vm::translation_matrix(vm::vec3(8192.0 - 8.0, 0.0, 0.0)), worldBounds);
            REQUIRE(groupNodeClone->children().front()->logicalBounds() == vm::bbox3(vm::vec3(8192.0 - 16.0, -8.0, -8.0), vm::vec3(8192.0, 8.0, 8.0)));


            transform(*entityNode, vm::translation_matrix(vm::vec3(1.0, 0.0, 0.0)), worldBounds);
            REQUIRE(entityNode->entity().origin() == vm::vec3(1.0, 0.0, 0.0));

            const auto updateResult = groupNode.updateLinkedGroups(worldBounds);
            updateResult.visit(kdl::overload(
                [&](const UpdateLinkedGroupsResult&) {
                    FAIL();
                },
                [](const UpdateLinkedGroupsError& e) {
                    CHECK(e == UpdateLinkedGroupsError{"Linked node exceeds world bounds"});
                }
            ));
        }

        TEST_CASE("GroupNodeTest.updateLinkedGroupsAndPreserveEntityProperties", "[GroupNodeTest]") {
            const auto worldBounds = vm::bbox3(8192.0);

            auto sourceGroupNode = GroupNode(Group("name"));
            sourceGroupNode.link();

            auto* sourceEntityNode = new EntityNode();
            sourceGroupNode.addChild(sourceEntityNode);

            auto targetGroupNode = std::unique_ptr<GroupNode>{static_cast<GroupNode*>(sourceGroupNode.cloneRecursively(worldBounds))};
            sourceGroupNode.addToLinkSet(*targetGroupNode);
            targetGroupNode->link();

            auto* targetEntityNode = static_cast<EntityNode*>(targetGroupNode->children().front());
            REQUIRE_THAT(targetEntityNode->entity().properties(), Catch::Equals(sourceEntityNode->entity().properties()));

            using T = std::tuple<std::vector<std::string>, std::vector<std::string>, std::vector<EntityProperty>, std::vector<EntityProperty>, std::vector<EntityProperty>>;

            const auto
            [ srcPresProperties, trgtPresProperties, sourceProperties, 
                                                     targetProperties, 
                                                     expectedProperties ] = GENERATE(values<T>({
            // properties remain unchanged
            { {},                {},                 { { "some_key", "some_value" } },
                                                     { { "some_key", "some_value" } },
                                                     { { "some_key", "some_value" } } },

            { {},                { "some_key" },     { { "some_key", "some_value" } },
                                                     { { "some_key", "some_value" } },
                                                     { { "some_key", "some_value" } } },

            { { "some_key" },    {},                 { { "some_key", "some_value" } },
                                                     { { "some_key", "some_value" } },
                                                     { { "some_key", "some_value" } } },

            { { "some_key" },    { "some_key" },     { { "some_key", "some_value" } },
                                                     { { "some_key", "some_value" } },
                                                     { { "some_key", "some_value" } } },

            // property was added to source
            { {},                {},                 { { "some_key", "some_value" } },
                                                     {},
                                                     { { "some_key", "some_value" } } },

            { {},                { "some_key" },     { { "some_key", "some_value" } },
                                                     {},
                                                     {} },

            { { "some_key" },    {},                 { { "some_key", "some_value" } },
                                                     {},
                                                     {} },

            { { "some_key" },    { "some_key" },     { { "some_key", "some_value" } },
                                                     {},
                                                     {} },

            // property was changed in source
            { {},                {},                 { { "some_key", "other_value" } },
                                                     { { "some_key", "some_value" } },
                                                     { { "some_key", "other_value" } } },

            { { "some_key" },    {},                 { { "some_key", "other_value" } },
                                                     { { "some_key", "some_value" } },
                                                     { { "some_key", "some_value" } } },

            { {},                { "some_key" },     { { "some_key", "other_value" } },
                                                     { { "some_key", "some_value" } },
                                                     { { "some_key", "some_value" } } },

            { { "some_key" },    { "some_key" },     { { "some_key", "other_value" } },
                                                     { { "some_key", "some_value" } },
                                                     { { "some_key", "some_value" } } },

            // property was removed in source
            { {},                {},                 {},
                                                     { { "some_key", "some_value" } },
                                                     {} },

            { { "some_key" },    {},                 {},
                                                     { { "some_key", "some_value" } },
                                                     { { "some_key", "some_value" } } },

            { {},                { "some_key" },     {},
                                                     { { "some_key", "some_value" } },
                                                     { { "some_key", "some_value" } } },

            { { "some_key" },    { "some_key" },     {},
                                                     { { "some_key", "some_value" } },
                                                     { { "some_key", "some_value" } } },

            // numbered property was added to source
            { {},                {},                 { { "some_key1", "some_value1" },
                                                       { "some_key2", "some_value2" } },
                                                     { { "some_key1", "some_value1" } },
                                                     { { "some_key1", "some_value1" },
                                                       { "some_key2", "some_value2" } } },

            { {},                { "some_key" },     { { "some_key1", "some_value1" },
                                                       { "some_key2", "some_value2" } },
                                                     { { "some_key1", "some_value1" } },
                                                     { { "some_key1", "some_value1" } } },

            { { "some_key" },    {},                 { { "some_key1", "some_value1" },
                                                       { "some_key2", "some_value2" } },
                                                     { { "some_key1", "some_value1" } },
                                                     { { "some_key1", "some_value1" } } },

            { { "some_key" },    { "some_key" },     { { "some_key1", "some_value1" },
                                                       { "some_key2", "some_value2" } },
                                                     { { "some_key1", "some_value1" } },
                                                     { { "some_key1", "some_value1" } } },

            // numbered property was changed in source
            { {},                {},                 { { "some_key1", "other_value" } },
                                                     { { "some_key1", "some_value" } },
                                                     { { "some_key1", "other_value" } } },

            { { "some_key" },    {},                 { { "some_key1", "other_value" } },
                                                     { { "some_key1", "some_value" } },
                                                     { { "some_key1", "some_value" } } },

            { {},                { "some_key" },     { { "some_key1", "other_value" } },
                                                     { { "some_key1", "some_value" } },
                                                     { { "some_key1", "some_value" } } },

            { { "some_key" },    { "some_key" },     { { "some_key1", "other_value" } },
                                                     { { "some_key1", "some_value" } },
                                                     { { "some_key1", "some_value" } } },

            // numbered property was removed in source
            { {},                {},                 { { "some_key2", "some_value2" } },
                                                     { { "some_key1", "some_value1" },
                                                       { "some_key2", "some_value2" } },
                                                     { { "some_key2", "some_value2" } } },

            { { "some_key" },    {},                 { { "some_key2", "some_value2" } },
                                                     { { "some_key1", "some_value1" },
                                                       { "some_key2", "some_value2" } },
                                                     { { "some_key1", "some_value1" },
                                                       { "some_key2", "some_value2" } } },

            { {},                { "some_key" },     { { "some_key2", "some_value2" } },
                                                     { { "some_key1", "some_value1" },
                                                       { "some_key2", "some_value2" } },
                                                     { { "some_key1", "some_value1" },
                                                       { "some_key2", "some_value2" } } },

            { { "some_key" },    { "some_key" },     { { "some_key2", "some_value2" } },
                                                     { { "some_key1", "some_value1" },
                                                       { "some_key2", "some_value2" } },
                                                     { { "some_key1", "some_value1" },
                                                       { "some_key2", "some_value2" } } },
            }));

            CAPTURE(srcPresProperties, trgtPresProperties, sourceProperties, targetProperties, expectedProperties);

            {
                auto entity = sourceEntityNode->entity();
                entity.setProperties(sourceProperties);
                entity.setPreservedProperties(srcPresProperties);
                sourceEntityNode->setEntity(std::move(entity));
            }

            {
                auto entity = targetEntityNode->entity();
                entity.setProperties(targetProperties);
                entity.setPreservedProperties(trgtPresProperties);
                targetEntityNode->setEntity(std::move(entity));
            }

            // lambda can't capture structured bindings
            const auto expectedTargetProperties = expectedProperties;

            const auto updateResult = sourceGroupNode.updateLinkedGroups(worldBounds);
            updateResult.visit(kdl::overload(
                [&](const UpdateLinkedGroupsResult& r) {
                    REQUIRE(r.size() == 1u);
                    const auto& p = r.front();

                    const auto& newLinkedNode = p.second;
                    REQUIRE(newLinkedNode->childCount() == 1u);

                    const auto* newEntityNode = dynamic_cast<EntityNode*>(newLinkedNode->children().front());
                    REQUIRE(newEntityNode != nullptr);

                    CHECK_THAT(newEntityNode->entity().properties(), Catch::UnorderedEquals(expectedTargetProperties));
                    CHECK_THAT(newEntityNode->entity().preservedProperties(), Catch::UnorderedEquals(targetEntityNode->entity().preservedProperties()));
                },
                [](const UpdateLinkedGroupsError&) {
                    FAIL();
                }
            ));
        }
    }
}
