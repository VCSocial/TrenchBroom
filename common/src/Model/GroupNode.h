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

#pragma once

#include "FloatType.h"
#include "Macros.h"
#include "Model/Group.h"
#include "Model/IdType.h"
#include "Model/Node.h"
#include "Model/Object.h"

#include <kdl/result_forward.h>

#include <vecmath/bbox.h>

#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace TrenchBroom {
    namespace Model {
        using UpdateLinkedGroupsResult = std::vector<std::pair<GroupNode*, std::unique_ptr<GroupNode>>>;

        struct UpdateLinkedGroupsError {
            std::string message;
        };

        bool operator==(const UpdateLinkedGroupsError& lhs, const UpdateLinkedGroupsError& rhs);
        bool operator!=(const UpdateLinkedGroupsError& lhs, const UpdateLinkedGroupsError& rhs);
        std::ostream& operator<<(std::ostream& str, const UpdateLinkedGroupsError& e);

        /**
         * A node that groups other nodes to make them editable as one. Multiple groups can form a
         * link set; a link set is a set of groups such that changes to the children of of one of the
         * member of the link set are reflected in the other members of the link set.
         *
         * A group can be in one of three states: singleton, linkable, and linked. A singleton group
         * is not part of a link set. A linkable group is part of a link set, but it is disconnected
         * from it, so that changes to the other groups are not reflected in the linkable group, and
         * changes to the disconnected member are also not reflected in the other members of the link
         * set. A linked group is a linkable group that is connected to the set, so that changes to
         * the other link set members are reflected in it and vice versa.
         */
        class GroupNode : public Node, public Object {
        private:
            struct SharedData;

            enum class EditState {
                Open,
                Closed,
                DescendantOpen
            };

            Group m_group;
            std::shared_ptr<SharedData> m_sharedData;
            EditState m_editState;
            mutable vm::bbox3 m_logicalBounds;
            mutable vm::bbox3 m_physicalBounds;
            mutable bool m_boundsValid;

            /**
             * The ID used to serialize group nodes (see MapReader and NodeSerializer). This is set by MapReader when a
             * layer is read, or by WorldNode when a group is added that doesn't yet have a persistent ID.
             */
            std::optional<IdType> m_persistentId;
        public:
            explicit GroupNode(Group group);

            const Group& group() const;
            Group setGroup(Group group);

            bool opened() const;
            bool hasOpenedDescendant() const;
            bool closed() const;
            void open();
            void close();

            const std::optional<IdType>& persistentId() const;
            void setPersistentId(IdType persistentId);

            const std::optional<IdType>& sharedPersistentId() const;

            /**
             * Returns the members of the link set. If this group is disconnected from the link set,
             * then it will not be included in the returned vector.
             */
            const std::vector<GroupNode*> linkedGroups() const;

            /**
             * Indicates that this and the given group node are members of the same link set.
             */
            bool inLinkSetWith(const GroupNode& groupNode) const;

            /**
             * Adds the given group to this group's link set. The given group will not be linked to its new link set.
             *
             * The given group node is removed from its own link set.
             */
            void addToLinkSet(GroupNode& groupNode);
            
            /**
             * Indicates whether this group node is connected to its link set.
             */
            bool linked() const;

            /**
             * Transitions this group from the linkable state to the link state, that is, the
             * group is connected to its link set.
             *
             * Expects that this group is not currently connected to its link set.
             */
            void link();

            /**
             * Transitions this group from the linked state to the linkable state, that is, the
             * group is disconnected from its link set.
             *
             * Expects that this group is currenctly connected to its link set.
             */
            void unlink();

            /**
             * Updates all linked groups in this groups' link set.
             *
             * The children of this node are cloned (recursively) and transformed into the linked nodes by means of the
             * recorded transformations of this group and the linked groups.
             *
             * Depending on the preserved property keys of the cloned entities and their corresponding entities in the
             * linked groups, some entity property changes may not be propagated from this group to the linked groups.
             * Specifically, if an entity property is preserved in either the cloned entity or its corresponding entity
             * in a linked group, then changes to that entity property incl. removal are not propagated. This also
             * applies to numbered properties, i.e. properties whose names end in a number. So if the entity property
             * "target" is preserved, then changes to the property "target2" are not propagated or overwritten during
             * propagation.
             *
             * If this operation fails for any child and linked group, then an error is returned. The operation can fail
             * if any of the following conditions arises:
             *
             * - the transformation of this group node is not invertible
             * - transforming any of this node's children fails
             * - any of the transformed children is no longer within the world bounds
             *
             * If this operation succeeds, a vector of pairs is returned where each pair consists of the linked node that
             * should be updated, and a replacement group node.
             */
            kdl::result<UpdateLinkedGroupsResult, UpdateLinkedGroupsError> updateLinkedGroups(const vm::bbox3& worldBounds);
        private:
            void setEditState(EditState editState);
            void setAncestorEditState(EditState editState);

            void openAncestors();
            void closeAncestors();
        private: // implement methods inherited from Node
            const std::string& doGetName() const override;
            const vm::bbox3& doGetLogicalBounds() const override;
            const vm::bbox3& doGetPhysicalBounds() const override;

            Node* doClone(const vm::bbox3& worldBounds) const override;

            bool doCanAddChild(const Node* child) const override;
            bool doCanRemoveChild(const Node* child) const override;
            bool doRemoveIfEmpty() const override;

            bool doShouldAddToSpacialIndex() const override;

            void doChildWasAdded(Node* node) override;
            void doChildWasRemoved(Node* node) override;

            void doNodePhysicalBoundsDidChange() override;
            void doChildPhysicalBoundsDidChange() override;

            bool doSelectable() const override;

            void doPick(const vm::ray3& ray, PickResult& pickResult) override;
            void doFindNodesContaining(const vm::vec3& point, std::vector<Node*>& result) override;

            void doGenerateIssues(const IssueGenerator* generator, std::vector<Issue*>& issues) override;
            void doAccept(NodeVisitor& visitor) override;
            void doAccept(ConstNodeVisitor& visitor) const override;
        private: // implement methods inherited from Object
            Node* doGetContainer() override;
            LayerNode* doGetContainingLayer() override;
            GroupNode* doGetContainingGroup() override;

            bool doContains(const Node* node) const override;
            bool doIntersects(const Node* node) const override;
        private:
            void invalidateBounds();
            void validateBounds() const;
        private: // implement Taggable interface
            void doAcceptTagVisitor(TagVisitor& visitor) override;
            void doAcceptTagVisitor(ConstTagVisitor& visitor) const override;
        private:
            deleteCopyAndMove(GroupNode)
        };
    }
}

