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

#include "Model/GroupNode.h"
#include "View/MapDocumentTest.h"
#include "View/MapDocument.h"

#include "Catch2.h"

namespace TrenchBroom {
    namespace View {
        class AddNodesTest : public MapDocumentTest {};

        TEST_CASE_METHOD(AddNodesTest, "AddNodesTest.linkAddedSingletonGroups") {
            Model::GroupNode* group = new Model::GroupNode(Model::Group("group"));
            REQUIRE_FALSE(group->linked());
            
            document->addNode(group, document->parentForNodes());
            CHECK(group->linked());

            document->undoCommand();
            CHECK_FALSE(group->linked());
        }

        TEST_CASE_METHOD(AddNodesTest, "AddNodesTest.recursivelyLinkAddedSingletonGroups") {
            Model::GroupNode* outer = new Model::GroupNode(Model::Group("outer"));
            Model::GroupNode* inner = new Model::GroupNode(Model::Group("inner"));
            outer->addChild(inner);

            REQUIRE_FALSE(outer->linked());
            REQUIRE_FALSE(inner->linked());
            
            document->addNode(outer, document->parentForNodes());
            CHECK(outer->linked());
            CHECK(inner->linked());

            document->undoCommand();
            CHECK_FALSE(outer->linked());
            CHECK_FALSE(inner->linked());
        }
    }
}
