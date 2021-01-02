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

#include "GroupNode.h"

#include "FloatType.h"
#include "Model/Brush.h"
#include "Model/BrushError.h"
#include "Model/BrushNode.h"
#include "Model/Entity.h"
#include "Model/EntityNode.h"
#include "Model/IssueGenerator.h"
#include "Model/LayerNode.h"
#include "Model/ModelUtils.h"
#include "Model/PickResult.h"
#include "Model/TagVisitor.h"
#include "Model/WorldNode.h"

#include <kdl/overload.h>
#include <kdl/result.h>
#include <kdl/string_utils.h>
#include <kdl/vector_utils.h>

#include <vecmath/ray.h>

#include <ostream>
#include <string>
#include <vector>

namespace TrenchBroom {
    namespace Model {
        bool operator==(const UpdateLinkedGroupsError& lhs, const UpdateLinkedGroupsError& rhs) {
            return lhs.message == rhs.message;
        }

        bool operator!=(const UpdateLinkedGroupsError& lhs, const UpdateLinkedGroupsError& rhs) {
            return !(lhs == rhs);
        }

        std::ostream& operator<<(std::ostream& str, const UpdateLinkedGroupsError& e) {
            str << e.message;
            return str;
        }

        struct GroupNode::SharedData {
            std::vector<GroupNode*> linkedGroups;
            std::optional<IdType> persistentId;
        };

        GroupNode::GroupNode(Group group) :
        m_group(std::move(group)),
        m_sharedData(std::make_shared<SharedData>()),
        m_editState(EditState::Closed),
        m_boundsValid(false) {}

        const Group& GroupNode::group() const {
            return m_group;
        }

        Group GroupNode::setGroup(Group group) {
            using std::swap;
            swap(m_group, group);
            return group;
        }

        bool GroupNode::opened() const {
            return m_editState == EditState::Open;
        }

        bool GroupNode::hasOpenedDescendant() const {
            return m_editState == EditState::DescendantOpen;
        }

        bool GroupNode::closed() const {
            return m_editState == EditState::Closed;
        }

        void GroupNode::open() {
            assert(m_editState == EditState::Closed);
            setEditState(EditState::Open);
            openAncestors();
        }

        void GroupNode::close() {
            assert(m_editState == EditState::Open);
            setEditState(EditState::Closed);
            closeAncestors();
        }

        const std::optional<IdType>& GroupNode::persistentId() const {
            return m_persistentId;
        }

        void GroupNode::setPersistentId(const IdType persistentId) {
            m_persistentId = persistentId;
            if (!m_sharedData->persistentId) {
                m_sharedData->persistentId = persistentId;
            }
        }

        const std::optional<IdType>& GroupNode::sharedPersistentId() const {
            return m_sharedData->persistentId;
        }

        const std::vector<GroupNode*> GroupNode::linkedGroups() const {
            return m_sharedData->linkedGroups;
        }

        bool GroupNode::inLinkSetWith(const GroupNode& groupNode) const {
            return groupNode.m_sharedData == m_sharedData;
        }

        void GroupNode::addToLinkSet(GroupNode& groupNode) {
            groupNode.m_sharedData = m_sharedData;
        }

        bool GroupNode::linked() const {
            return kdl::vec_contains(m_sharedData->linkedGroups, this);
        }

        void GroupNode::link() {
            assert(!linked());
            m_sharedData->linkedGroups.push_back(this);
        }

        void GroupNode::unlink() {
            assert(linked());
            m_sharedData->linkedGroups = kdl::vec_erase(std::move(m_sharedData->linkedGroups), this);
        }

        static kdl::result<std::vector<std::unique_ptr<Node>>, UpdateLinkedGroupsError> cloneAndTransformChildren(const Node& node, const vm::bbox3& worldBounds, const vm::mat4x4& transformation) {
            auto result = std::vector<std::unique_ptr<Node>>{};
            result.reserve(node.childCount());

            for (const auto* childNode : node.children()) {
                using VisitResult = kdl::result<std::unique_ptr<Node>, UpdateLinkedGroupsError>;

                UpdateLinkedGroupsError error;
                const auto success = childNode->accept(kdl::overload(
                    [] (const WorldNode*) -> VisitResult { return VisitResult::error(UpdateLinkedGroupsError{"Visited world node while updating linked groups"}); },
                    [] (const LayerNode*) -> VisitResult { return VisitResult::error(UpdateLinkedGroupsError{"Visited layer node while updating linked groups"}); },
                    [&](const GroupNode* groupNode) -> VisitResult {
                        auto group = groupNode->group();
                        group.transform(transformation);
                        return VisitResult::success(std::make_unique<GroupNode>(std::move(group)));
                    },
                    [&](const EntityNode* entityNode)-> VisitResult {
                        auto entity = entityNode->entity();
                        entity.transform(transformation);
                        return VisitResult::success(std::make_unique<EntityNode>(std::move(entity)));
                    },
                    [&](const BrushNode* brushNode) -> VisitResult {
                        auto brush = brushNode->brush();
                        return brush.transform(worldBounds, transformation, true)
                            .visit(kdl::overload(
                                [&]() -> VisitResult {
                                    return VisitResult::success(std::make_unique<BrushNode>(std::move(brush)));
                                },
                                [](const BrushError e) -> VisitResult {
                                    return VisitResult::error(UpdateLinkedGroupsError{kdl::str_to_string(e)});
                                }
                            ));
                    }
                )).and_then([&](std::unique_ptr<Node>&& newChildNode) {
                    if (!worldBounds.contains(newChildNode->logicalBounds())) {
                        return kdl::result<void, UpdateLinkedGroupsError>::error(UpdateLinkedGroupsError{"Linked node exceeds world bounds"});
                    }
                    return cloneAndTransformChildren(*childNode, worldBounds, transformation)
                        .and_then([&](std::vector<std::unique_ptr<Node>>&& newChildren) {
                            newChildNode->addChildren(kdl::vec_transform(std::move(newChildren), [](std::unique_ptr<Node>&& child) { return child.release(); }));
                            result.push_back(std::move(newChildNode));
                            return kdl::void_result;
                        });
                }).handle_errors([&](const UpdateLinkedGroupsError& e) {
                    error = e;
                });

                if (!success) {
                    return kdl::result<std::vector<std::unique_ptr<Node>>, UpdateLinkedGroupsError>::error(error);
                }
            }

            return kdl::result<std::vector<std::unique_ptr<Node>>, UpdateLinkedGroupsError>::success(std::move(result));
        }

        static void preserveEntityProperties(EntityNode& clonedEntityNode, const EntityNode& correspondingEntityNode) {
            if (clonedEntityNode.entity().preservedProperties().empty() && 
                correspondingEntityNode.entity().preservedProperties().empty()) {
                return;
            }

            auto clonedEntity = clonedEntityNode.entity();
            const auto& correspondingEntity = correspondingEntityNode.entity();

            const auto allPreservedProperties = kdl::vec_sort_and_remove_duplicates(
                kdl::vec_concat(
                    clonedEntity.preservedProperties(),
                    correspondingEntity.preservedProperties()));

            clonedEntity.setPreservedProperties(correspondingEntity.preservedProperties());

            for (const auto& propertyKey : allPreservedProperties) {
                // this can change the order of properties
                clonedEntity.removeProperty(propertyKey);
                if (const auto* propertyValue = correspondingEntity.property(propertyKey)) {
                    clonedEntity.addOrUpdateProperty(propertyKey, *propertyValue);
                }

                clonedEntity.removeNumberedProperty(propertyKey);
                for (const auto& numberedProperty : correspondingEntity.numberedProperties(propertyKey)) {
                    clonedEntity.addOrUpdateProperty(numberedProperty.key(), numberedProperty.value());
                }
            }

            clonedEntityNode.setEntity(std::move(clonedEntity));
        }

        template <typename T>
        static void preserveEntityProperties(const std::vector<T>& clonedNodes, const std::vector<Node*>& correspondingNodes) {
            auto clIt = std::begin(clonedNodes);
            auto coIt = std::begin(correspondingNodes);
            while (clIt != std::end(clonedNodes) && coIt != std::end(correspondingNodes)) {
                auto& clonedNode = *clIt; // deduces either to std::unique_ptr<Node>& or Node*& depending on T
                const auto* correspondingNode = *coIt;

                clonedNode->accept(kdl::overload(
                    [] (WorldNode*) {},
                    [] (LayerNode*) {},
                    [&](GroupNode* clonedGroupNode) {
                        if (const auto* correspondingGroupNode = dynamic_cast<const GroupNode*>(correspondingNode)) {
                            preserveEntityProperties(clonedGroupNode->children(), correspondingGroupNode->children());
                        }
                    },
                    [&](EntityNode* clonedEntityNode) {
                        if (const auto* correspondingEntityNode = dynamic_cast<const EntityNode*>(correspondingNode)) {
                            preserveEntityProperties(*clonedEntityNode, *correspondingEntityNode);
                        }
                    },
                    [] (BrushNode*) {}
                ));

                ++clIt;
                ++coIt;
            }
        }

        kdl::result<UpdateLinkedGroupsResult, UpdateLinkedGroupsError> GroupNode::updateLinkedGroups(const vm::bbox3& worldBounds) {
            assert(linked());

            auto result = UpdateLinkedGroupsResult{};
            result.reserve(m_sharedData->linkedGroups.size());

            const auto [success, myInvertedTransformation] = vm::invert(m_group.transformation());
            if (!success) {
                return kdl::result<UpdateLinkedGroupsResult, UpdateLinkedGroupsError>::error(UpdateLinkedGroupsError{"Group transformation is not invertible"});
            }

            for (auto* linkedGroup : m_sharedData->linkedGroups) {
                if (linkedGroup != this) {
                    const auto transformation = linkedGroup->group().transformation() * myInvertedTransformation;

                    UpdateLinkedGroupsError error;
                    const auto success = cloneAndTransformChildren(*this, worldBounds, transformation)
                        .and_then([&](std::vector<std::unique_ptr<Node>>&& newChildren) {
                            preserveEntityProperties(newChildren, linkedGroup->children());

                            auto linkedGroupClone = std::unique_ptr<GroupNode>{static_cast<GroupNode*>(linkedGroup->clone(worldBounds))};
                            addToLinkSet(*linkedGroupClone);
                            linkedGroupClone->addChildren(kdl::vec_transform(std::move(newChildren), [](auto c) { return c.release(); }));

                            result.emplace_back(linkedGroup, std::move(linkedGroupClone));
                            return kdl::void_result;
                        }).handle_errors([&](const UpdateLinkedGroupsError e) {
                            error = e;
                        });
                    
                    if (!success) {
                        return kdl::result<UpdateLinkedGroupsResult, UpdateLinkedGroupsError>::error(error);
                    }
                }
            }

            return kdl::result<UpdateLinkedGroupsResult, UpdateLinkedGroupsError>::success(std::move(result));
        }

        void GroupNode::setEditState(const EditState editState) {
            m_editState = editState;
        }

        void GroupNode::setAncestorEditState(const EditState editState) {
            visitParent(kdl::overload(
                [=](auto&& thisLambda, WorldNode* world)   -> void { world->visitParent(thisLambda); },
                [=](auto&& thisLambda, LayerNode* layer)   -> void { layer->visitParent(thisLambda); },
                [=](auto&& thisLambda, GroupNode* group)   -> void { group->setEditState(editState); group->visitParent(thisLambda); },
                [=](auto&& thisLambda, EntityNode* entity) -> void { entity->visitParent(thisLambda); },
                [=](auto&& thisLambda, BrushNode* brush)   -> void { brush->visitParent(thisLambda); }
            ));
        }

        void GroupNode::openAncestors() {
            setAncestorEditState(EditState::DescendantOpen);
        }

        void GroupNode::closeAncestors() {
            setAncestorEditState(EditState::Closed);
        }

        const std::string& GroupNode::doGetName() const {
            return m_group.name();
        }

        const vm::bbox3& GroupNode::doGetLogicalBounds() const {
            if (!m_boundsValid) {
                validateBounds();
            }
            return m_logicalBounds;
        }

        const vm::bbox3& GroupNode::doGetPhysicalBounds() const {
            if (!m_boundsValid) {
                validateBounds();
            }
            return m_physicalBounds;
        }

        Node* GroupNode::doClone(const vm::bbox3& /* worldBounds */) const {
            GroupNode* group = new GroupNode(m_group);
            cloneAttributes(group);
            return group;
        }

        bool GroupNode::doCanAddChild(const Node* child) const {
            return child->accept(kdl::overload(
                [](const WorldNode*)  { return false; },
                [](const LayerNode*)  { return false; },
                [](const GroupNode*)  { return true;  },
                [](const EntityNode*) { return true;  },
                [](const BrushNode*)  { return true;  }
            ));
        }

        bool GroupNode::doCanRemoveChild(const Node* /* child */) const {
            return true;
        }

        bool GroupNode::doRemoveIfEmpty() const {
            return true;
        }

        bool GroupNode::doShouldAddToSpacialIndex() const {
            return false;
        }

        void GroupNode::doChildWasAdded(Node* /* node */) {
            nodePhysicalBoundsDidChange(physicalBounds());
        }

        void GroupNode::doChildWasRemoved(Node* /* node */) {
            nodePhysicalBoundsDidChange(physicalBounds());
        }

        void GroupNode::doNodePhysicalBoundsDidChange() {
            invalidateBounds();
        }

        void GroupNode::doChildPhysicalBoundsDidChange() {
            const vm::bbox3 myOldBounds = physicalBounds();
            invalidateBounds();
            if (physicalBounds() != myOldBounds) {
                nodePhysicalBoundsDidChange(myOldBounds);
            }
        }

        bool GroupNode::doSelectable() const {
            return true;
        }

        void GroupNode::doPick(const vm::ray3& /* ray */, PickResult&) {
            // For composite nodes (Groups, brush entities), pick rays don't hit the group
            // but instead just the primitives inside (brushes, point entities).
            // This avoids a potential performance trap where we'd have to exhaustively
            // test many objects if most of the map was inside groups, but it means
            // the pick results need to be postprocessed to account for groups (if desired).
            // See: https://github.com/TrenchBroom/TrenchBroom/issues/2742
        }

        void GroupNode::doFindNodesContaining(const vm::vec3& point, std::vector<Node*>& result) {
            if (logicalBounds().contains(point)) {
                result.push_back(this);
            }

            for (auto* child : Node::children()) {
                child->findNodesContaining(point, result);
            }
        }

        void GroupNode::doGenerateIssues(const IssueGenerator* generator, std::vector<Issue*>& issues) {
            generator->generate(this, issues);
        }

        void GroupNode::doAccept(NodeVisitor& visitor) {
            visitor.visit(this);
        }

        void GroupNode::doAccept(ConstNodeVisitor& visitor) const {
            visitor.visit(this);
        }

        Node* GroupNode::doGetContainer() {
            return parent();
        }

        LayerNode* GroupNode::doGetContainingLayer() {
            return findContainingLayer(this);
        }

        GroupNode* GroupNode::doGetContainingGroup() {
            return findContainingGroup(this);
        }

        bool GroupNode::doContains(const Node* node) const {
            return boundsContainNode(logicalBounds(), node);
        }

        bool GroupNode::doIntersects(const Node* node) const {
            return boundsIntersectNode(logicalBounds(), node);
        }

        void GroupNode::invalidateBounds() {
            m_boundsValid = false;
        }

        void GroupNode::validateBounds() const {
            m_logicalBounds = computeLogicalBounds(children(), vm::bbox3(0.0));
            m_physicalBounds = computePhysicalBounds(children(), vm::bbox3(0.0));
            m_boundsValid = true;
        }

        void GroupNode::doAcceptTagVisitor(TagVisitor& visitor) {
            visitor.visit(*this);
        }

        void GroupNode::doAcceptTagVisitor(ConstTagVisitor& visitor) const {
            visitor.visit(*this);
        }
    }
}
