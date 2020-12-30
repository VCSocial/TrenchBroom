/*
 Copyright (C) 2010-2017 Kristian Duske

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

#include "Model/BrushNode.h"
#include "Model/Entity.h"
#include "Model/EntityNode.h"
#include "Model/GroupNode.h"
#include "Model/LayerNode.h"
#include "Model/WorldNode.h"
#include "View/MapDocumentTest.h"
#include "View/PasteType.h"

#include <kdl/vector_utils.h>

#include <set>

#include "Catch2.h"

namespace TrenchBroom {
    namespace View {
        class GroupNodesTest : public MapDocumentTest {};

        TEST_CASE_METHOD(GroupNodesTest, "GroupNodesTest.createEmptyGroup", "[GroupNodesTest]") {
            CHECK(document->groupSelection("test") == nullptr);
        }

        TEST_CASE_METHOD(GroupNodesTest, "GroupNodesTest.createGroupWithOneNode", "[GroupNodesTest]") {
            Model::BrushNode* brush = createBrushNode();
            document->addNode(brush, document->parentForNodes());
            document->select(brush);

            Model::GroupNode* group = document->groupSelection("test");
            CHECK(group != nullptr);

            CHECK(brush->parent() == group);
            CHECK(group->selected());
            CHECK_FALSE(brush->selected());

            document->undoCommand();
            CHECK(group->parent() == nullptr);
            CHECK(brush->parent() == document->parentForNodes());
            CHECK(brush->selected());
        }

        TEST_CASE_METHOD(GroupNodesTest, "GroupNodesTest.createGroupWithPartialBrushEntity", "[GroupNodesTest]") {
            Model::BrushNode* brush1 = createBrushNode();
            document->addNode(brush1, document->parentForNodes());

            Model::BrushNode* brush2 = createBrushNode();
            document->addNode(brush2, document->parentForNodes());

            Model::EntityNode* entity = new Model::EntityNode();
            document->addNode(entity, document->parentForNodes());
            document->reparentNodes(entity, { brush1, brush2 });

            document->select(brush1);

            Model::GroupNode* group = document->groupSelection("test");
            CHECK(group != nullptr);

            CHECK(brush1->parent() == entity);
            CHECK(brush2->parent() == entity);
            CHECK(entity->parent() == group);
            CHECK(group->selected());
            CHECK_FALSE(brush1->selected());

            document->undoCommand();
            CHECK(group->parent() == nullptr);
            CHECK(brush1->parent() == entity);
            CHECK(brush2->parent() == entity);
            CHECK(entity->parent() == document->parentForNodes());
            CHECK_FALSE(group->selected());
            CHECK(brush1->selected());
        }

        TEST_CASE_METHOD(GroupNodesTest, "GroupNodesTest.createGroupWithFullBrushEntity", "[GroupNodesTest]") {
            Model::BrushNode* brush1 = createBrushNode();
            document->addNode(brush1, document->parentForNodes());

            Model::BrushNode* brush2 = createBrushNode();
            document->addNode(brush2, document->parentForNodes());

            Model::EntityNode* entity = new Model::EntityNode();
            document->addNode(entity, document->parentForNodes());
            document->reparentNodes(entity, { brush1, brush2 });

            document->select(std::vector<Model::Node*>({ brush1, brush2 }));

            Model::GroupNode* group = document->groupSelection("test");
            CHECK(group != nullptr);

            CHECK(brush1->parent() == entity);
            CHECK(brush2->parent() == entity);
            CHECK(entity->parent() == group);
            CHECK(group->selected());
            CHECK_FALSE(brush1->selected());
            CHECK_FALSE(brush2->selected());

            document->undoCommand();
            CHECK(group->parent() == nullptr);
            CHECK(brush1->parent() == entity);
            CHECK(brush2->parent() == entity);
            CHECK(entity->parent() == document->parentForNodes());
            CHECK_FALSE(group->selected());
            CHECK(brush1->selected());
            CHECK(brush2->selected());
        }

        TEST_CASE_METHOD(GroupNodesTest, "GroupNodesTest.pasteInGroup", "[GroupNodesTest]") {
            // https://github.com/TrenchBroom/TrenchBroom/issues/1734

            const std::string data("{"
                              "\"classname\" \"light\""
                              "\"origin\" \"0 0 0\""
                              "}");

            Model::BrushNode* brush = createBrushNode();
            document->addNode(brush, document->parentForNodes());
            document->select(brush);

            Model::GroupNode* group = document->groupSelection("test");
            document->openGroup(group);

            CHECK(document->paste(data) == PasteType::Node);
            CHECK(document->selectedNodes().hasOnlyEntities());
            CHECK(document->selectedNodes().entityCount() == 1u);

            Model::EntityNode* light = document->selectedNodes().entities().front();
            CHECK(light->parent() == group);
        }

        static bool hasEmptyName(const std::vector<std::string>& names) {
            for (const auto& name : names) {
                if (name.empty()) {
                    return true;
                }
            }
            return false;
        }

        TEST_CASE_METHOD(GroupNodesTest, "GroupNodesTest.undoMoveGroupContainingBrushEntity", "[GroupNodesTest]") {
            // Test for issue #1715

            Model::BrushNode* brush1 = createBrushNode();
            document->addNode(brush1, document->parentForNodes());

            Model::EntityNode* entityNode = new Model::EntityNode();
            document->addNode(entityNode, document->parentForNodes());
            document->reparentNodes(entityNode, { brush1 });

            document->select(brush1);

            Model::GroupNode* group = document->groupSelection("test");
            CHECK(group->selected());

            CHECK(document->translateObjects(vm::vec3(16,0,0)));

            CHECK_FALSE(hasEmptyName(entityNode->entity().propertyKeys()));

            document->undoCommand();

            CHECK_FALSE(hasEmptyName(entityNode->entity().propertyKeys()));
        }

        TEST_CASE_METHOD(GroupNodesTest, "GroupNodesTest.rotateGroupContainingBrushEntity", "[GroupNodesTest]") {
            // Test for issue #1754

            Model::BrushNode* brush1 = createBrushNode();
            document->addNode(brush1, document->parentForNodes());

            Model::EntityNode* entityNode = new Model::EntityNode();
            document->addNode(entityNode, document->parentForNodes());
            document->reparentNodes(entityNode, { brush1 });

            document->select(brush1);

            Model::GroupNode* group = document->groupSelection("test");
            CHECK(group->selected());

            CHECK_FALSE(entityNode->entity().hasProperty("origin"));
            CHECK(document->rotateObjects(vm::vec3::zero(), vm::vec3::pos_z(), static_cast<FloatType>(10.0)));
            CHECK_FALSE(entityNode->entity().hasProperty("origin"));

            document->undoCommand();

            CHECK_FALSE(entityNode->entity().hasProperty("origin"));
        }

        TEST_CASE_METHOD(GroupNodesTest, "GroupNodesTest.renameGroup", "[GroupNodesTest]") {
            Model::BrushNode* brush1 = createBrushNode();
            document->addNode(brush1, document->parentForNodes());
            document->select(brush1);

            Model::GroupNode* group = document->groupSelection("test");
            
            document->renameGroups("abc");
            CHECK(group->name() == "abc");
            
            document->undoCommand();
            CHECK(group->name() == "test");

            document->redoCommand();
            CHECK(group->name() == "abc");
        }

        TEST_CASE_METHOD(GroupNodesTest, "GroupNodesTest.duplicateNodeInGroup", "[GroupNodesTest]") {
            Model::BrushNode* brush = createBrushNode();
            document->addNode(brush, document->parentForNodes());
            document->select(brush);

            Model::GroupNode* group = document->groupSelection("test");
            REQUIRE(group != nullptr);

            document->openGroup(group);

            document->select(brush);
            REQUIRE(document->duplicateObjects());

            Model::BrushNode* brushCopy = document->selectedNodes().brushes().at(0u);
            CHECK(brushCopy->parent() == group);
        }

        TEST_CASE_METHOD(GroupNodesTest, "GroupNodesTest.createLinkedGroup", "[GroupNodesTest]") {
            auto* brushNode = createBrushNode();
            document->addNode(brushNode, document->parentForNodes());
            document->select(brushNode);

            auto* groupNode = document->groupSelection("test");
            REQUIRE(groupNode != nullptr);

            document->deselectAll();
            document->select(groupNode);

            auto* linkedGroupNode = document->createLinkedGroup();
            CHECK(linkedGroupNode != nullptr);

            CHECK(groupNode->linked());
            CHECK_THAT(groupNode->linkedGroups(), Catch::UnorderedEquals(std::vector<Model::GroupNode*>{groupNode, linkedGroupNode}));
            
            CHECK(linkedGroupNode->linked());
            CHECK_THAT(linkedGroupNode->linkedGroups(), Catch::UnorderedEquals(std::vector<Model::GroupNode*>{groupNode, linkedGroupNode}));
        }

        TEST_CASE_METHOD(GroupNodesTest, "GroupNodesTest.updateLinkedGroups", "[GroupNodesTest]") {
            auto* brushNode = createBrushNode();
            document->addNode(brushNode, document->parentForNodes());
            document->select(brushNode);

            auto* groupNode = document->groupSelection("test");
            REQUIRE(groupNode != nullptr);

            document->deselectAll();
            document->select(groupNode);

            auto* linkedGroupNode = document->createLinkedGroup();
            REQUIRE(linkedGroupNode != nullptr);

            document->deselectAll();
            document->select(linkedGroupNode);

            document->translateObjects(vm::vec3(32.0, 0.0, 0.0));
            REQUIRE(linkedGroupNode->children().front()->physicalBounds() == brushNode->physicalBounds().translate(vm::vec3(32.0, 0.0, 0.0)));

            document->deselectAll();
            document->select(groupNode);
            document->openGroup(groupNode);
            
            const auto originalBrushBounds = brushNode->physicalBounds();

            document->select(brushNode);
            document->translateObjects(vm::vec3(0.0, 16.0, 0.0));
            document->deselectAll();
            document->closeGroup();

            REQUIRE(brushNode->physicalBounds() == originalBrushBounds.translate(vm::vec3(0.0, 16.0, 0.0)));

            // changes were propagated
            REQUIRE(document->world()->defaultLayer()->childCount() == 2u);
            auto* newLinkedGroupNode = document->world()->defaultLayer()->children().back();
            REQUIRE(newLinkedGroupNode != groupNode);

            CHECK(newLinkedGroupNode->children().front()->physicalBounds() == brushNode->physicalBounds().translate(vm::vec3(32.0, 0.0, 0.0)));
        }

        TEST_CASE_METHOD(GroupNodesTest, "GroupNodesTest.updateNestedLinkedGroups", "[GroupNodesTest]") {
            auto* brushNode = createBrushNode();
            document->addNode(brushNode, document->parentForNodes());
            document->select(brushNode);

            /*
            world
            +-defaultLayer
              +-brushNode
            */

            auto* innerGroupNode = document->groupSelection("inner");
            REQUIRE(innerGroupNode != nullptr);

            /*
            world
            +-defaultLayer
              +-innerGroupNode
                +-brushNode
            */

            document->deselectAll();
            document->select(innerGroupNode);

            auto* outerGroupNode = document->groupSelection("outer");
            REQUIRE(outerGroupNode != nullptr);

            /*
            world
            +-defaultLayer
              +-outerGroupNode
                +-innerGroupNode
                  +-brushNode
            */

            document->deselectAll();
            document->select(outerGroupNode);

            auto* linkedOuterGroupNode = document->createLinkedGroup();
            REQUIRE(linkedOuterGroupNode != nullptr);
            REQUIRE(linkedOuterGroupNode->childCount() == 1u);

            auto* linkedInnerGroupNode = linkedOuterGroupNode->children().front();
            REQUIRE(linkedInnerGroupNode->childCount() == 1u);

            /*
            world
            +-defaultLayer
              +-outerGroupNode
                +-innerGroupNode
                  +-brushNode
              +-linkedOuterGroupNode
                +-linkedInnerGroupNode
                  +-brushNode (linked clone)
            */

            document->deselectAll();
            document->select(linkedOuterGroupNode);

            document->translateObjects(vm::vec3(32.0, 0.0, 0.0));
            REQUIRE(linkedOuterGroupNode->children().front()->physicalBounds() == brushNode->physicalBounds().translate(vm::vec3(32.0, 0.0, 0.0)));
            REQUIRE(linkedInnerGroupNode->children().front()->physicalBounds() == brushNode->physicalBounds().translate(vm::vec3(32.0, 0.0, 0.0)));

            /*
            world
            +-defaultLayer
              +-outerGroupNode
                +-innerGroupNode
                  +-brushNode
              +-linkedOuterGroupNode (translated by 32 0 0)
                +-linkedInnerGroupNode (translated by 32 0 0)
                  +-brushNode (linked clone) (translated by 32 0 0)
            */

            document->deselectAll();
            document->select(outerGroupNode);
            document->openGroup(outerGroupNode);
            document->select(innerGroupNode);
            document->openGroup(innerGroupNode);
            
            const auto originalBrushBounds = brushNode->physicalBounds();

            document->select(brushNode);
            document->translateObjects(vm::vec3(0.0, 16.0, 0.0));
            REQUIRE(brushNode->physicalBounds() == originalBrushBounds.translate(vm::vec3(0.0, 16.0, 0.0)));

            /*
            world
            +-defaultLayer
              +-outerGroupNode
                +-innerGroupNode
                  +-brushNode (translated by 0 16 0)
              +-linkedOuterGroupNode (translated by 32 0 0)
                +-linkedInnerGroupNode (translated by 32 0 0)
                  +-brushNode (linked clone) (translated by 32 0 0)
            */

            document->deselectAll();
            document->closeGroup(); // innerGroupNode

            /*
            world
            +-defaultLayer
              +-outerGroupNode
                +-innerGroupNode
                  +-brushNode (translated by 0 16 0)
              +-linkedOuterGroupNode (translated by 32 0 0)
                +-newLinkedInnerGroupNode (translated by 32 0 0)
                  +-brushNode (linked clone) (translated by 32 16 0)
            */

            document->closeGroup(); // outerGroupNode

            /*
            world
            +-defaultLayer
              +-outerGroupNode
                +-innerGroupNode
                  +-brushNode (translated by 0 16 0)
              +-newLinkedOuterGroupNode (translated by 32 0 0)
                +-newLinkedInnerGroupNodeClone (translated by 32 0 0)
                  +-brushNode (linked clone) (translated by 32 16 0)
            */

            // changes were propagated
            REQUIRE(document->world()->defaultLayer()->childCount() == 2u);
            auto* newLinkedOuterGroupNode = document->world()->defaultLayer()->children().back();
            
            REQUIRE(newLinkedOuterGroupNode->childCount() == 1u);
            auto* newLinkedInnerGroupNode = newLinkedOuterGroupNode->children().front();
            
            REQUIRE(newLinkedInnerGroupNode->childCount() == 1u);
            CHECK(newLinkedInnerGroupNode->children().front()->physicalBounds() == brushNode->physicalBounds().translate(vm::vec3(32.0, 0.0, 0.0)));
        }
    }
}
