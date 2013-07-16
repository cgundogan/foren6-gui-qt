#include "rplNetworkInfoManager.h"
#include <rpl_packet_parser.h>
#include "rplLink.h"
#include "rplNode.h"
#include <QGraphicsWidget>
#include <data_info/hash_container.h>

namespace rpl
{

NetworkInfoManager *NetworkInfoManager::_thisInstance = 0;

NetworkInfoManager::NetworkInfoManager()
{
	rpl_event_callbacks_t callbacks = {
		&onNodeCreated,
		&onNodeDeleted,
		&onNodeUpdated,
		0,
		0,
		0,
		&onLinkCreated,
		&onLinkDeleted,
		0,
		0,
		0,
		0
	};
	currentVersion = 0;
	_thisInstance = this;
	//rpl_tool_set_callbacks(&callbacks);
	_checkPendingActionsTimer.setInterval(100);
	_checkPendingActionsTimer.setSingleShot(false);
	QObject::connect(&_checkPendingActionsTimer, SIGNAL(timeout()), this, SLOT(checkPendingActions()));
	_checkPendingActionsTimer.start();
}

NetworkInfoManager::~NetworkInfoManager() {
	_checkPendingActionsTimer.stop();
	QMutexLocker lockMutex(&_thisInstance->_pendingActionsMutex);
	_collected_data = 0;
	_thisInstance = 0;

}

void NetworkInfoManager::onNodeCreated(di_node_t *node) {
	QMutexLocker lockMutex(&_thisInstance->_pendingActionsMutex);
	Action *action = new Action;
	action->event = Action::AE_Created;
	action->type = Action::AT_Node;
	action->ptr = node;
	qDebug("Node created: %p, %llX", node, node_get_mac64(node));
	_thisInstance->_pendingActions.append(action);
}

void NetworkInfoManager::onNodeUpdated(di_node_t *node) {
	QMutexLocker lockMutex(&_thisInstance->_pendingActionsMutex);
	Action *action = new Action;
	action->event = Action::AE_Updated;
	action->type = Action::AT_Node;
	action->ptr = node;
	_thisInstance->_pendingActions.append(action);
}

void NetworkInfoManager::onNodeDeleted(di_node_t *node) {
	QMutexLocker lockMutex(&_thisInstance->_pendingActionsMutex);
	Action *action = new Action;
	action->event = Action::AE_Deleted;
	action->type = Action::AT_Node;
	action->ptr = node_get_user_data(node);
	qDebug("Node deleted: %p, %llX", node, node_get_mac64(node));
	_thisInstance->_pendingActions.append(action);
}

void NetworkInfoManager::onDodagCreated(di_dodag_t *dodag) {
	QMutexLocker lockMutex(&_thisInstance->_pendingActionsMutex);
	Action *action = new Action;
	action->event = Action::AE_Created;
	action->type = Action::AT_Dodag;
	action->ptr = dodag;
	_thisInstance->_pendingActions.append(action);
}

void NetworkInfoManager::onDodagUpdated(di_dodag_t *dodag) {
	QMutexLocker lockMutex(&_thisInstance->_pendingActionsMutex);
	Action *action = new Action;
	action->event = Action::AE_Updated;
	action->type = Action::AT_Dodag;
	action->ptr = dodag;
	_thisInstance->_pendingActions.append(action);
}

void NetworkInfoManager::onRplInstanceCreated(di_rpl_instance_t *rplInstance) {
	QMutexLocker lockMutex(&_thisInstance->_pendingActionsMutex);
	Action *action = new Action;
	action->event = Action::AE_Created;
	action->type = Action::AT_RplInstance;
	action->ptr = rplInstance;
	_thisInstance->_pendingActions.append(action);
}

void NetworkInfoManager::onRplInstanceUpdated(di_rpl_instance_t *rplInstance) {
	QMutexLocker lockMutex(&_thisInstance->_pendingActionsMutex);
	Action *action = new Action;
	action->event = Action::AE_Updated;
	action->type = Action::AT_RplInstance;
	action->ptr = rplInstance;
	_thisInstance->_pendingActions.append(action);
}

void NetworkInfoManager::onLinkCreated(di_link_t *link) {
	QMutexLocker lockMutex(&_thisInstance->_pendingActionsMutex);
	Action *action = new Action;
	action->event = Action::AE_Created;
	action->type = Action::AT_Link;
	action->ptr = link;
	qDebug("Link created: %p, %llX -> %llX", link, link_get_key(link)->ref.child.wpan_address, link_get_key(link)->ref.parent.wpan_address);
	_thisInstance->_pendingActions.append(action);
}

void NetworkInfoManager::onLinkDeleted(di_link_t *link) {
	QMutexLocker lockMutex(&_thisInstance->_pendingActionsMutex);
	Action *action = new Action;
	action->event = Action::AE_Deleted;
	action->type = Action::AT_Link;
	action->ptr = link_get_user_data(link);
	qDebug("Link deleted: %p, %llX -> %llX", link, link_get_key(link)->ref.child.wpan_address, link_get_key(link)->ref.parent.wpan_address);
	_thisInstance->_pendingActions.append(action);
}

void NetworkInfoManager::useVersion(uint32_t version) {
	hash_iterator_ptr it, itEnd;
	hash_container_ptr node_container = rpldata_get_nodes(version);
	hash_container_ptr link_container = rpldata_get_links(version);
	QHash<addr_wpan_t, Node*> presentNodes;
	QGraphicsItem *currentItem;
	Node *currentNode;

	if(version && version == currentVersion)
		return;  //already at that version, nothing to do. Version 0 is a dynamic version and always change

	currentVersion = version;

	foreach(currentItem, _scene.items()) {
		currentNode = dynamic_cast<Node*>(currentItem);
		if(currentNode) {
			presentNodes.insert(node_get_mac64(currentNode->getNodeData()), currentNode);
			//_scene.removeItem(currentItem);
		} else if(currentItem->group() == 0)
			_scene.removeItem(currentItem);
	}

	_scene.removeAllLinks();

	it = hash_begin(NULL, NULL);
	itEnd = hash_begin(NULL, NULL);

	if(node_container) {
		for(hash_begin(node_container, it), hash_end(node_container, itEnd); !hash_it_equ(it, itEnd); hash_it_inc(it)) {
			di_node_t *node = (di_node_t *)hash_it_value(it);
			Node *newnode;

			newnode = presentNodes.value(node_get_mac64(node), 0);
			if(newnode) {
				presentNodes.remove(node_get_mac64(node));
			} else {
				newnode = new Node(node, QString::number(node_get_mac64(node), 16));
				_scene.addNode(newnode);
			}
			node_set_user_data(node, newnode);
		}
	}

	if(link_container) {
		for(hash_begin(link_container, it), hash_end(link_container, itEnd); !hash_it_equ(it, itEnd); hash_it_inc(it)) {
			di_link_t *link = (di_link_t *)hash_it_value(it);
			Link *linkNodes;
			Node *from, *to;

			from = (Node*) node_get_user_data((di_node_t*)hash_value(node_container, hash_key_make(link_get_key(link)->ref.child), HVM_FailIfNonExistant, NULL));
			to =   (Node*) node_get_user_data((di_node_t*)hash_value(node_container, hash_key_make(link_get_key(link)->ref.parent), HVM_FailIfNonExistant, NULL));

			linkNodes = new Link(link, from, to);
			link_set_user_data(link, linkNodes);
			_scene.addLink(linkNodes);
		}
	}

	foreach(currentNode, presentNodes) {
		_scene.removeNode(currentNode);
		delete currentNode;
	}

	hash_it_destroy(it);
	hash_it_destroy(itEnd);
}

void NetworkInfoManager::checkPendingActions() {
	QMutexLocker lockMutex(&_pendingActionsMutex);
	Action* action;

	/*foreach(action, _pendingActions) {
		switch(action->type) {
			case Action::AT_Link: {
				if(action->event == Action::AE_Created) {
					di_link_t *link = static_cast<di_link_t*>(action->ptr);
					Link *linkNodes;
					Node *from, *to;
					di_node_key_t node_key;

					node_key = (di_node_key_t){link->key.ref.child, 0};
					from = (Node*) node_get_user_data((di_node_t*)hash_value(_collected_data->nodes, hash_key_make(node_key), HVM_FailIfNonExistant, NULL));


					node_key = (di_node_key_t){link->key.ref.parent, 0};
					to =   (Node*) node_get_user_data((di_node_t*)hash_value(_collected_data->nodes, hash_key_make(node_key), HVM_FailIfNonExistant, NULL));

					linkNodes = new Link(link, from, to);
					link->user_data = linkNodes;
					_thisInstance->_scene.addLink(linkNodes);
				} else if(action->event == Action::AE_Deleted) {
					Link *link = static_cast<Link*>(action->ptr);
					_thisInstance->_scene.removeLink(link);
					delete link;
				}
				break;
			}
			case Action::AT_Node: {
				di_node_t *node = static_cast<di_node_t*>(action->ptr);
				if(action->event == Action::AE_Created) {
					Node *newnode;

					newnode = new Node(node, QString::number(node_get_mac64(node), 16));
					node_set_user_data(node, newnode);
					_thisInstance->_scene.addNode(newnode);
				} else if(action->event == Action::AE_Deleted) {
					Node *node = static_cast<Node*>(action->ptr);
					_thisInstance->_scene.removeNode(node);
					delete node;
				} else if(action->event == Action::AE_Updated) {
					static_cast<Node*>(node_get_user_data(node))->onNodeChanged();
				}
				break;
			}
		}
		delete action;
	}
	*/
	_pendingActions.clear();

	if(currentVersion == 0)
		useVersion(0);  //update only in realtime mode
}

}
