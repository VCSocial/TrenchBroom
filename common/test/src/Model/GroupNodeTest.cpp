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

#include "Model/EntityNode.h"
#include "Model/GroupNode.h"

#include <vecmath/bbox.h>
#include <vecmath/bbox_io.h>

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
    }
}
