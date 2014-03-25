/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Joshua Leung
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Defines and code for core node types
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_utildefines.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_types.h"
} /* extern "C" */

#include "depsgraph_types.h"
#include "depsgraph_intern.h"

#include "stubs.h" // XXX: THIS MUST BE REMOVED WHEN THE DEPSGRAPH REFACTOR IS DONE

void BKE_animsys_eval_driver(void *context, void *item) {}

void BKE_constraints_evaluate(void *context, void *item) {}
void BKE_pose_iktree_evaluate(void *context, void *item) {}
void BKE_pose_splineik_evaluate(void *context, void *item) {}
void BKE_pose_eval_bone(void *context, void *item) {}

void BKE_pose_rebuild_op(void *context, void *item) {}
void BKE_pose_eval_init(void *context, void *item) {}
void BKE_pose_eval_flush(void *context, void *item) {}

void BKE_particle_system_eval(void *context, void *item) {}

void BKE_rigidbody_rebuild_sim(void *context, void *item) {}
void BKE_rigidbody_eval_simulation(void *context, void *item) {}
void BKE_rigidbody_object_sync_transforms(void *context, void *item) {}

void BKE_object_eval_local_transform(void *context, void *item) {}
void BKE_object_eval_parent(void *context, void *item) {}

void BKE_mesh_eval_geometry(void *context, void *item) {}
void BKE_mball_eval_geometry(void *context, void *item) {}
void BKE_curve_eval_geometry(void *context, void *item) {}
void BKE_curve_eval_path(void *context, void *item) {}
void BKE_lattice_eval_geometry(void *context, void *item) {}

/* ******************************************************** */
/* Generic Nodes */

/* Root Node ============================================== */

/* Add 'root' node to graph */
void RootDepsNode::add_to_graph(Depsgraph *graph, const ID *UNUSED(id))
{
	BLI_assert(graph->root_node == NULL);
	graph->root_node = this;
}

/* Remove 'root' node from graph */
void RootDepsNode::remove_from_graph(Depsgraph *graph)
{
	BLI_assert(graph->root_node == this);
	graph->root_node = NULL;
}

DEG_DEPSNODE_DEFINE(RootDepsNode, DEPSNODE_TYPE_ROOT, "Root DepsNode");
static DepsNodeFactoryImpl<RootDepsNode> DNTI_ROOT;

/* Time Source Node ======================================= */

/* Add 'time source' node to graph */
void TimeSourceDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	/* determine which node to attach timesource to */
	if (id) {
		/* get ID node */
//		DepsNode *id_node = graph->get_node(id, NULL, DEPSNODE_TYPE_ID_REF, NULL);
		
		/* depends on what this is... */
		switch (GS(id->name)) {
			case ID_SCE: /* Scene - Usually sequencer strip causing time remapping... */
			{
				// TODO...
			}
			break;
			
			case ID_GR: /* Group */
			{
				// TODO...
			}
			break;
			
			// XXX: time source...
			
			default:     /* Unhandled */
				printf("%s(): Unhandled ID - %s \n", __func__, id->name);
				break;
		}
	}
	else {
		/* root-node */
		graph->root_node->time_source = this;
		this->owner = graph->root_node;
	}
}

/* Remove 'time source' node from graph */
void TimeSourceDepsNode::remove_from_graph(Depsgraph *graph)
{
	BLI_assert(this->owner != NULL);
	
	switch(this->owner->type) {
		case DEPSNODE_TYPE_ROOT: /* root node - standard case */
		{
			graph->root_node->time_source = NULL;
			this->owner = NULL;
		}
		break;
		
		// XXX: ID node - as needed...
		
		default: /* unhandled for now */
			break;
	}
}

DEG_DEPSNODE_DEFINE(TimeSourceDepsNode, DEPSNODE_TYPE_TIMESOURCE, "Time Source");
static DepsNodeFactoryImpl<TimeSourceDepsNode> DNTI_TIMESOURCE;

/* ID Node ================================================ */

/* Initialise 'id' node - from pointer data given */
void IDDepsNode::init(const ID *id, const char *UNUSED(subdata))
{
	/* store ID-pointer */
	BLI_assert(id != NULL);
	this->id = (ID *)id;
	
	/* NOTE: components themselves are created if/when needed.
	 * This prevents problems with components getting added 
	 * twice if an ID-Ref needs to be created to house it...
	 */
}

/* Free 'id' node */
IDDepsNode::~IDDepsNode()
{
	for (IDDepsNode::ComponentMap::const_iterator it = this->components.begin(); it != this->components.end(); ++it) {
		const ComponentDepsNode *comp = it->second;
		delete comp;
	}
}

/* Copy 'id' node */
void IDDepsNode::copy(DepsgraphCopyContext *dcc, const IDDepsNode *src)
{
	/* iterate over items in original hash, adding them to new hash */
	for (IDDepsNode::ComponentMap::const_iterator it = this->components.begin(); it != this->components.end(); ++it) {
		/* get current <type : component> mapping */
		eDepsNode_Type c_type   = it->first;
		DepsNode *old_component = it->second;
		
		/* make a copy of component */
		ComponentDepsNode *component     = (ComponentDepsNode *)DEG_copy_node(dcc, old_component);
		
		/* add new node to hash... */
		this->components[c_type] = component;
	}
	
	// TODO: perform a second loop to fix up links?
}

/* Add 'id' node to graph */
void IDDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	/* add to hash so that it can be found */
	graph->id_hash[id] = this;
}

/* Remove 'id' node from graph */
void IDDepsNode::remove_from_graph(Depsgraph *graph)
{
	/* remove toplevel node and hash entry, but don't free... */
	graph->id_hash.erase(this->id);
}

/* Validate links between components */
void IDDepsNode::validate_links(Depsgraph *graph)
{
#if 0
	/* XXX WARNING: using ListBase is dangerous for virtual C++ classes,
	 * loses vtable info!
	 * Disabled for now due to unclear purpose, later use a std::vector or similar here
	 */
	
	ListBase dummy_list = {NULL, NULL}; // XXX: perhaps this should live in the node?
	
	/* get our components ......................................................................... */
	ComponentDepsNode *params = find_component(DEPSNODE_TYPE_PARAMETERS);
	ComponentDepsNode *anim = find_component(DEPSNODE_TYPE_ANIMATION);
	ComponentDepsNode *trans = find_component(DEPSNODE_TYPE_TRANSFORM);
	ComponentDepsNode *geom = find_component(DEPSNODE_TYPE_GEOMETRY);
	ComponentDepsNode *proxy = find_component(DEPSNODE_TYPE_PROXY);
	ComponentDepsNode *pose = find_component(DEPSNODE_TYPE_EVAL_POSE);
	ComponentDepsNode *psys = find_component(DEPSNODE_TYPE_EVAL_PARTICLES);
	ComponentDepsNode *seq = find_component(DEPSNODE_TYPE_SEQUENCER);
	
	/* enforce (gross) ordering of these components................................................. */
	// TODO: create relationships to do this...
	
	/* parameters should always exist... */
	#pragma message("DEPSGRAPH PORTING XXX: params not always created, assert disabled for now")
//	BLI_assert(params != NULL);
	BLI_addhead(&dummy_list, params);
	
	/* anim before params */
	if (anim && params) {
		BLI_addhead(&dummy_list, anim);
	}
	
	/* proxy before anim (or params) */
	if (proxy) {
		BLI_addhead(&dummy_list, proxy);
	}
	
	/* transform after params */
	if (trans) {
		BLI_addtail(&dummy_list, trans);
	}
	
	/* geometry after transform */
	if (geom) {
		BLI_addtail(&dummy_list, geom);
	}
	
	/* pose eval after transform */
	if (pose) {
		BLI_addtail(&dummy_list, pose);
	}
#endif
	
	/* for each component, validate it's internal nodes ............................................ */
	
	/* NOTE: this is done after the component-level restrictions are done,
	 * so that we can take those restrictions as a guide for our low-level
	 * component restrictions...
	 */
	for (IDDepsNode::ComponentMap::const_iterator it = this->components.begin(); it != this->components.end(); ++it) {
		DepsNode *component = it->second;
		component->validate_links(graph);
	}
}

ComponentDepsNode *IDDepsNode::find_component(eDepsNode_Type type) const
{
	ComponentMap::const_iterator it = components.find(type);
	return it != components.end() ? it->second : NULL;
}

DEG_DEPSNODE_DEFINE(IDDepsNode, DEPSNODE_TYPE_ID_REF, "ID Node");
static DepsNodeFactoryImpl<IDDepsNode> DNTI_ID_REF;

/* Subgraph Node ========================================== */

/* Initialise 'subgraph' node - from pointer data given */
void SubgraphDepsNode::init(const ID *id, const char *UNUSED(subdata))
{
	/* store ID-ref if provided */
	this->root_id = (ID *)id;
	
	/* NOTE: graph will need to be added manually,
	 * as we don't have any way of passing this down
	 */
}

/* Free 'subgraph' node */
SubgraphDepsNode::~SubgraphDepsNode()
{
	/* only free if graph not shared, of if this node is the first reference to it... */
	// XXX: prune these flags a bit...
	if ((this->flag & SUBGRAPH_FLAG_FIRSTREF) || !(this->flag & SUBGRAPH_FLAG_SHARED)) {
		/* free the referenced graph */
		DEG_graph_free(this->graph);
		this->graph = NULL;
	}
}

/* Copy 'subgraph' node - Assume that the subgraph doesn't get copied for now... */
void SubgraphDepsNode::copy(DepsgraphCopyContext *dcc, const SubgraphDepsNode *src)
{
	//const SubgraphDepsNode *src_node = (const SubgraphDepsNode *)src;
	//SubgraphDepsNode *dst_node       = (SubgraphDepsNode *)dst;
	
	/* for now, subgraph itself isn't copied... */
}

/* Add 'subgraph' node to graph */
void SubgraphDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	/* add to subnodes list */
	graph->subgraphs.insert(this);
	
	/* if there's an ID associated, add to ID-nodes lookup too */
	if (id) {
#if 0 /* XXX subgraph node is NOT a true IDDepsNode - what is this supposed to do? */
		// TODO: what to do if subgraph's ID has already been added?
		BLI_assert(!graph->find_id_node(id));
		graph->id_hash[id] = this;
#endif
	}
}

/* Remove 'subgraph' node from graph */
void SubgraphDepsNode::remove_from_graph(Depsgraph *graph)
{
	/* remove from subnodes list */
	graph->subgraphs.erase(this);
	
	/* remove from ID-nodes lookup */
	if (this->root_id) {
#if 0 /* XXX subgraph node is NOT a true IDDepsNode - what is this supposed to do? */
		BLI_assert(graph->find_id_node(this->root_id) == this);
		graph->id_hash.erase(this->root_id);
#endif
	}
}

/* Validate subgraph links... */
void SubgraphDepsNode::validate_links(Depsgraph *graph)
{
	
}

DEG_DEPSNODE_DEFINE(SubgraphDepsNode, DEPSNODE_TYPE_SUBGRAPH, "Subgraph Node");
static DepsNodeFactoryImpl<SubgraphDepsNode> DNTI_SUBGRAPH;

/* ******************************************************** */
/* Outer Nodes */

/* Standard Component Methods ============================= */

OperationDepsNode *ComponentDepsNode::find_operation(const char *name) const
{
	OperationMap::const_iterator it = this->operations.find(name);
	return it != this->operations.end() ? it->second : NULL;
}

/* Initialise 'component' node - from pointer data given */
void ComponentDepsNode::init(const ID *id, const char *subdata)
{
	/* hook up eval context? */
	// XXX: maybe this needs a special API?
}

/* Copy 'component' node */
void ComponentDepsNode::copy(DepsgraphCopyContext *dcc, const ComponentDepsNode *src)
{
	/* duplicate list of operation nodes */
	this->operations.clear();
	
	for (OperationMap::const_iterator it = src->operations.begin(); it != src->operations.end(); ++it) {
		const char *pchan_name = it->first;
		OperationDepsNode *src_op = it->second;
		
		/* recursive copy */
		DepsNodeFactory *factory = DEG_node_get_factory(src_op);
		OperationDepsNode *dst_op = (OperationDepsNode *)factory->copy_node(dcc, src_op);
		this->operations[pchan_name] = dst_op;
			
		/* fix links... */
		// ...
	}
	
	/* copy evaluation contexts */
	//
}

/* Free 'component' node */
ComponentDepsNode::~ComponentDepsNode()
{
	/* free nodes and list of nodes */
	for (OperationMap::const_iterator it = this->operations.begin(); it != this->operations.end(); ++it) {
		OperationDepsNode *op = it->second;
		delete op;
	}
}

/* Add 'component' node to graph */
void ComponentDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	/* find ID node that we belong to (and create it if it doesn't exist!) */
	IDDepsNode *id_node = (IDDepsNode *)graph->get_node(id, NULL, DEPSNODE_TYPE_ID_REF, NULL);
	BLI_assert(id_node != NULL);
	
	/* add component to id */
	id_node->components[this->type] = this;
	this->owner = id_node;
}

/* Remove 'component' node from graph */
void ComponentDepsNode::remove_from_graph(Depsgraph *graph)
{
	/* detach from owner (i.e. id-ref) */
	if (this->owner) {
		IDDepsNode *id_node = (IDDepsNode *)this->owner;
		id_node->components.erase(this->type);
		this->owner = NULL;
	}
	
	/* NOTE: don't need to do anything about relationships,
	 * as those are handled via the standard mechanism
	 */
}

/* Parameter Component Defines ============================ */

DEG_DEPSNODE_DEFINE(ParametersComponentDepsNode, DEPSNODE_TYPE_PARAMETERS, "Parameters Component");
static DepsNodeFactoryImpl<ParametersComponentDepsNode> DNTI_PARAMETERS;

/* Animation Component Defines ============================ */

DEG_DEPSNODE_DEFINE(AnimationComponentDepsNode, DEPSNODE_TYPE_ANIMATION, "Animation Component");
static DepsNodeFactoryImpl<AnimationComponentDepsNode> DNTI_ANIMATION;

/* Transform Component Defines ============================ */

DEG_DEPSNODE_DEFINE(TransformComponentDepsNode, DEPSNODE_TYPE_TRANSFORM, "Transform Component");
static DepsNodeFactoryImpl<TransformComponentDepsNode> DNTI_TRANSFORM;

/* Proxy Component Defines ================================ */

DEG_DEPSNODE_DEFINE(ProxyComponentDepsNode, DEPSNODE_TYPE_PROXY, "Proxy Component");
static DepsNodeFactoryImpl<ProxyComponentDepsNode> DNTI_PROXY;

/* Geometry Component Defines ============================= */

DEG_DEPSNODE_DEFINE(GeometryComponentDepsNode, DEPSNODE_TYPE_GEOMETRY, "Geometry Component");
static DepsNodeFactoryImpl<GeometryComponentDepsNode> DNTI_GEOMETRY;

/* Sequencer Component Defines ============================ */

DEG_DEPSNODE_DEFINE(SequencerComponentDepsNode, DEPSNODE_TYPE_SEQUENCER, "Sequencer Component");
static DepsNodeFactoryImpl<SequencerComponentDepsNode> DNTI_SEQUENCER;

/* Pose Component ========================================= */

BoneComponentDepsNode *PoseComponentDepsNode::find_bone_component(const char *name) const
{
	BoneComponentMap::const_iterator it = this->bone_hash.find(name);
	return it != this->bone_hash.end() ? it->second : NULL;
}

/* Initialise 'pose eval' node - from pointer data given */
void PoseComponentDepsNode::init(const ID *id, const char *subdata)
{
	/* generic component-node... */
	ComponentDepsNode::init(id, subdata);
}

/* Copy 'pose eval' node */
void PoseComponentDepsNode::copy(DepsgraphCopyContext *dcc, const PoseComponentDepsNode *src)
{
	/* generic component node... */
	ComponentDepsNode::copy(dcc, src);
	
	/* pose-specific data... */
	// copy bonehash...
}

/* Free 'pose eval' node */
PoseComponentDepsNode::~PoseComponentDepsNode()
{
}

/* Validate links for pose evaluation */
void PoseComponentDepsNode::validate_links(Depsgraph *graph)
{
	/* create our core operations... */
	if (!this->bone_hash.empty() || !this->operations.empty()) {
		OperationDepsNode *rebuild_op, *init_op, *cleanup_op;
		IDDepsNode *owner_node = (IDDepsNode *)this->owner;
		Object *ob;
		ID *id;
		
		/* get ID-block that pose-component belongs to */
		BLI_assert(owner_node && owner_node->id);
		
		id = owner_node->id;
		ob = (Object *)id;
		
		/* create standard pose evaluation start/end hooks */
		rebuild_op = DEG_add_operation(graph, id, NULL, DEPSNODE_TYPE_OP_POSE,
		                               DEPSOP_TYPE_REBUILD, BKE_pose_rebuild_op,
		                               "Rebuild Pose");
		RNA_pointer_create(id, &RNA_Pose, ob->pose, &rebuild_op->ptr);
		
		init_op = DEG_add_operation(graph, id, NULL, DEPSNODE_TYPE_OP_POSE,
		                            DEPSOP_TYPE_INIT, BKE_pose_eval_init,
		                            "Init Pose Eval");
		RNA_pointer_create(id, &RNA_Pose, ob->pose, &init_op->ptr);
		
		cleanup_op = DEG_add_operation(graph, id, NULL, DEPSNODE_TYPE_OP_POSE,
		                               DEPSOP_TYPE_POST, BKE_pose_eval_flush,
		                               "Flush Pose Eval");
		RNA_pointer_create(id, &RNA_Pose, ob->pose, &cleanup_op->ptr);
		
		
		/* attach links between these operations */
		DEG_add_new_relation(rebuild_op, init_op,    DEPSREL_TYPE_COMPONENT_ORDER, "[Pose Rebuild -> Pose Init] DepsRel");
		DEG_add_new_relation(init_op,    cleanup_op, DEPSREL_TYPE_COMPONENT_ORDER, "[Pose Init -> Pose Cleanup] DepsRel");
		
		/* NOTE: bones will attach themselves to these endpoints */
	}
	
	/* ensure that each bone has been validated... */
	for (PoseComponentDepsNode::BoneComponentMap::const_iterator it = this->bone_hash.begin(); it != this->bone_hash.end(); ++it) {
		DepsNode *bone_comp = it->second;
		/* recursively validate the links within bone component */
		// NOTE: this ends up hooking up the IK Solver(s) here to the relevant final bone operations...
		bone_comp->validate_links(graph);
	}
}

DEG_DEPSNODE_DEFINE(PoseComponentDepsNode, DEPSNODE_TYPE_EVAL_POSE, "Pose Eval Component");
static DepsNodeFactoryImpl<PoseComponentDepsNode> DNTI_EVAL_POSE;

/* Bone Component ========================================= */

/* Initialise 'bone component' node - from pointer data given */
void BoneComponentDepsNode::init(const ID *id, const char *subdata)
{
	/* generic component-node... */
	ComponentDepsNode::init(id, subdata);
	
	/* name of component comes is bone name */
	BLI_strncpy(this->name, subdata, MAX_NAME);
	
	/* bone-specific node data */
	Object *ob = (Object *)id;
	this->pchan = BKE_pose_channel_find_name(ob->pose, subdata);
}

/* Add 'bone component' node to graph */
void BoneComponentDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	PoseComponentDepsNode *pose_node;
	
	/* find pose node that we belong to (and create it if it doesn't exist!) */
	pose_node = (PoseComponentDepsNode *)graph->get_node(id, NULL, DEPSNODE_TYPE_EVAL_POSE, NULL);
	BLI_assert(pose_node != NULL);
	
	/* add bone component to pose bone-hash */
	pose_node->bone_hash[this->name] = this;
	this->owner = pose_node;
}

/* Remove 'bone component' node from graph */
void BoneComponentDepsNode::remove_from_graph(Depsgraph *graph)
{
	/* detach from owner (i.e. pose component) */
	if (this->owner) {
		PoseComponentDepsNode *pose_node = (PoseComponentDepsNode *)this->owner;
		
		pose_node->bone_hash.erase(this->name);
		this->owner = NULL;
	}
	
	/* NOTE: don't need to do anything about relationships,
	 * as those are handled via the standard mechanism
	 */
}

/* Validate 'bone component' links... 
 * - Re-route all component-level relationships to the nodes 
 */
void BoneComponentDepsNode::validate_links(Depsgraph *graph)
{
	PoseComponentDepsNode *pcomp = (PoseComponentDepsNode *)this->owner;
	bPoseChannel *pchan = this->pchan;
	
	DepsNode *btrans_op = this->find_operation("Bone Transforms");
	DepsNode *final_op = NULL;  /* normal final-evaluation operation */
	DepsNode *ik_op = NULL;     /* IK Solver operation */
	
	BLI_assert(btrans_op != NULL);
	
	/* link bone/component to pose "sources" if it doesn't have any obvious dependencies */
	if (pchan->parent == NULL) {
		DepsNode *pinit_op = pcomp->find_operation("Init Pose Eval");
		DEG_add_new_relation(pinit_op, btrans_op, DEPSREL_TYPE_OPERATION, "PoseEval Source-Bone Link");
	}
	
	/* inlinks destination should all go to the "Bone Transforms" operation */
	DEPSNODE_RELATIONS_ITER_BEGIN(this->inlinks, rel)
	{
		/* add equivalent relation to the bone transform operation */
		DEG_add_new_relation(rel->from, btrans_op, rel->type, rel->name);
	}
	DEPSNODE_RELATIONS_ITER_END;
	
	
	/* outlink source target depends on what we might have:
	 * 1) Transform only - No constraints at all
	 * 2) Constraints node - Just plain old constraints
	 * 3) IK Solver node - If part of IK chain...
	 */
	if (pchan->constraints.first) {
		/* find constraint stack operation */
		final_op = this->find_operation("Constraint Stack");
	}
	else {
		/* just normal transforms */
		final_op = btrans_op;
	}
	
	DEPSNODE_RELATIONS_ITER_BEGIN(this->outlinks, rel)
	{
		/* Technically, the last evaluation operation on these
		 * should be IK if present. Since, this link is actually
		 * present in the form of one or more of the ops, we'll
		 * take the first one that comes (during a first pass)
		 * (XXX: there's potential here for problems with forked trees) 
		 */
		if (strcmp(rel->name, "IK Solver Update") == 0) {
			ik_op = rel->to;
			break;
		}
	}
	DEPSNODE_RELATIONS_ITER_END;
	
	/* fix up outlink refs */
	DEPSNODE_RELATIONS_ITER_BEGIN(this->outlinks, rel)
	{
		if (ik_op) {
			/* bone is part of IK Chain... */
			if (rel->to == ik_op) {
				/* can't have ik to ik, so use final "normal" bone transform 
				 * as indicator to IK Solver that it is ready to run 
				 */
				DEG_add_new_relation(final_op, rel->to, rel->type, rel->name);
			}
			else {
				/* everything which depends on result of this bone needs to know 
				 * about the IK result too!
				 */
				DEG_add_new_relation(ik_op, rel->to, rel->type, rel->name);
			}
		}
		else {
			/* bone is not part of IK Chains... */
			DEG_add_new_relation(final_op, rel->to, rel->type, rel->name);
		}
	}
	DEPSNODE_RELATIONS_ITER_END;
	
	/* link bone/component to pose "sinks" as final link, unless it has obvious quirks */
	{
		DepsNode *ppost_op = this->find_operation("Cleanup Pose Eval");
		DEG_add_new_relation(final_op, ppost_op, DEPSREL_TYPE_OPERATION, "PoseEval Sink-Bone Link");
	}
}

DEG_DEPSNODE_DEFINE(BoneComponentDepsNode, DEPSNODE_TYPE_BONE, "Bone Component");
static DepsNodeFactoryImpl<BoneComponentDepsNode> DNTI_BONE;


/* ******************************************************** */
/* Inner Nodes */

/* Standard Operation Callbacks =========================== */
/* NOTE: some of these are just templates used by the others */

/* Helper to add 'operation' node to graph */
void OperationDepsNode::add_to_component_node(Depsgraph *graph, const ID *id, eDepsNode_Type component_type)
{
	/* get component node to add operation to */
	ComponentDepsNode *component = (ComponentDepsNode *)graph->get_node(id, NULL, component_type, NULL);
	
	/* add to hash table */
	component->operations[this->name] = this;
	
	/* add backlink to component */
	this->owner = component;
}

/* Callback to remove 'operation' node from graph */
void OperationDepsNode::remove_from_graph(Depsgraph *UNUSED(graph))
{
	if (this->owner) {
		ComponentDepsNode *component = (ComponentDepsNode *)this->owner;
		
		/* remove node from hash table */
		component->operations.erase(this->name);
		
		/* remove backlink */
		this->owner = NULL;
	}
}

/* Parameter Operation ==================================== */

/* Add 'parameter operation' node to graph */
void ParametersOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_PARAMETERS);
}

DEG_DEPSNODE_DEFINE(ParametersOperationDepsNode, DEPSNODE_TYPE_OP_PARAMETER, "Parameters Operation");
static DepsNodeFactoryImpl<ParametersOperationDepsNode> DNTI_OP_PARAMETERS;

/* Proxy Operation ======================================== */

/* Add 'proxy operation' node to graph */
void ProxyOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_PROXY);
}

DEG_DEPSNODE_DEFINE(ProxyOperationDepsNode, DEPSNODE_TYPE_OP_PROXY, "Proxy Operation");
static DepsNodeFactoryImpl<ProxyOperationDepsNode> DNTI_OP_PROXY;

/* Animation Operation ==================================== */

/* Add 'animation operation' node to graph */
void AnimationOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_ANIMATION);
}

DEG_DEPSNODE_DEFINE(AnimationOperationDepsNode, DEPSNODE_TYPE_OP_ANIMATION, "Animation Operation");
static DepsNodeFactoryImpl<AnimationOperationDepsNode> DNTI_OP_ANIMATION;

/* Transform Operation ==================================== */

/* Add 'transform operation' node to graph */
void TransformOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_TRANSFORM);
}

DEG_DEPSNODE_DEFINE(TransformOperationDepsNode, DEPSNODE_TYPE_OP_TRANSFORM, "Transform Operation");
static DepsNodeFactoryImpl<TransformOperationDepsNode> DNTI_OP_TRANSFORM;

/* Geometry Operation ===================================== */

/* Add 'geometry operation' node to graph */
void GeometryOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_GEOMETRY);
}

DEG_DEPSNODE_DEFINE(GeometryOperationDepsNode, DEPSNODE_TYPE_OP_GEOMETRY, "Geometry Operation");
static DepsNodeFactoryImpl<GeometryOperationDepsNode> DNTI_OP_GEOMETRY;

/* Sequencer Operation ==================================== */

/* Add 'sequencer operation' node to graph */
void SequencerOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_SEQUENCER);
}

DEG_DEPSNODE_DEFINE(SequencerOperationDepsNode, DEPSNODE_TYPE_OP_SEQUENCER, "Sequencer Operation");
static DepsNodeFactoryImpl<SequencerOperationDepsNode> DNTI_OP_SEQUENCER;

/* Update Operation ======================================= */

/* Add 'update operation' node to graph */
void UpdateOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_PARAMETERS);
}

DEG_DEPSNODE_DEFINE(UpdateOperationDepsNode, DEPSNODE_TYPE_OP_UPDATE, "RNA Update Operation");
static DepsNodeFactoryImpl<UpdateOperationDepsNode> DNTI_OP_UPDATE;

/* Driver Operation ===================================== */
// XXX: some special tweaks may be needed for this one...

/* Add 'driver operation' node to graph */
void DriverOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_PARAMETERS);
}

DEG_DEPSNODE_DEFINE(DriverOperationDepsNode, DEPSNODE_TYPE_OP_DRIVER, "Driver Operation");
static DepsNodeFactoryImpl<DriverOperationDepsNode> DNTI_OP_DRIVER;

/* Pose Operation ========================================= */

/* Add 'pose operation' node to graph */
void PoseOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_EVAL_POSE);
}

DEG_DEPSNODE_DEFINE(PoseOperationDepsNode, DEPSNODE_TYPE_OP_POSE, "Pose Operation");
static DepsNodeFactoryImpl<PoseOperationDepsNode> DNTI_OP_POSE;

/* Bone Operation ========================================= */

/* Init local data for bone operation */
void BoneOperationDepsNode::init(const ID *id, const char *subdata)
{
	Object *ob;
	bPoseChannel *pchan;
	
	/* set up RNA Pointer to affected bone */
	ob = (Object *)id;
	pchan = BKE_pose_channel_find_name(ob->pose, subdata);
	
	RNA_pointer_create((ID *)id, &RNA_PoseBone, pchan, &this->ptr);
}

/* Add 'bone operation' node to graph */
void BoneOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	BoneComponentDepsNode *bone_comp;
	bPoseChannel *pchan;
	
	/* get bone component that owns this bone operation */
	BLI_assert(this->ptr.type == &RNA_PoseBone);
	pchan = (bPoseChannel *)this->ptr.data;
	
	bone_comp = (BoneComponentDepsNode *)graph->get_node(id, pchan->name, DEPSNODE_TYPE_BONE, NULL);
	
	/* add to hash table */
	bone_comp->operations[pchan->name] = this;
	
	/* add backlink to component */
	this->owner = bone_comp;
}

DEG_DEPSNODE_DEFINE(BoneOperationDepsNode, DEPSNODE_TYPE_OP_BONE, "Bone Operation");
static DepsNodeFactoryImpl<BoneOperationDepsNode> DNTI_OP_BONE;

/* Particle Operation ===================================== */

/* Add 'particle operation' node to graph */
void ParticlesOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_EVAL_PARTICLES);
}

/* Remove 'particle operation' node from graph */
void ParticlesOperationDepsNode::remove_from_graph(Depsgraph *graph)
{
	// XXX...
	OperationDepsNode::remove_from_graph(graph);
}

DEG_DEPSNODE_DEFINE(ParticlesOperationDepsNode, DEPSNODE_TYPE_OP_PARTICLE, "Particles Operation");
static DepsNodeFactoryImpl<ParticlesOperationDepsNode> DNTI_OP_PARTICLES;

/* RigidBody Operation ==================================== */
/* Note: RigidBody Operations are reserved for scene-level rigidbody sim steps */

/* Add 'rigidbody operation' node to graph */
void RigidBodyOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_TRANSFORM); // XXX
}

DEG_DEPSNODE_DEFINE(RigidBodyOperationDepsNode, DEPSNODE_TYPE_OP_RIGIDBODY, "RigidBody Operation");
static DepsNodeFactoryImpl<RigidBodyOperationDepsNode> DNTI_OP_RIGIDBODY;

/* ******************************************************** */
/* External API */

/* Global type registry */

/* NOTE: For now, this is a hashtable not array, since the core node types
 * currently do not have contiguous ID values. Using a hash here gives us
 * more flexibility, albeit using more memory and also sacrificing a little
 * speed. Later on, when things stabilise we may turn this back to an array
 * since there are only just a few node types that an array would cope fine...
 */
static GHash *_depsnode_typeinfo_registry = NULL;

/* Registration ------------------------------------------- */

/* Register node type */
static void DEG_register_node_typeinfo(DepsNodeFactory *factory)
{
	BLI_assert(factory != NULL);
	BLI_ghash_insert(_depsnode_typeinfo_registry, SET_INT_IN_POINTER(factory->type()), factory);
}

/* Register all node types */
void DEG_register_node_types(void)
{
	/* initialise registry */
	_depsnode_typeinfo_registry = BLI_ghash_int_new("Depsgraph Node Type Registry");
	
	/* register node types */
	/* GENERIC */
	DEG_register_node_typeinfo(&DNTI_ROOT);
	DEG_register_node_typeinfo(&DNTI_TIMESOURCE);
	
	DEG_register_node_typeinfo(&DNTI_ID_REF);
	DEG_register_node_typeinfo(&DNTI_SUBGRAPH);
	
	/* OUTER */
	DEG_register_node_typeinfo(&DNTI_PARAMETERS);
	DEG_register_node_typeinfo(&DNTI_PROXY);
	DEG_register_node_typeinfo(&DNTI_ANIMATION);
	DEG_register_node_typeinfo(&DNTI_TRANSFORM);
	DEG_register_node_typeinfo(&DNTI_GEOMETRY);
	DEG_register_node_typeinfo(&DNTI_SEQUENCER);
	
	DEG_register_node_typeinfo(&DNTI_EVAL_POSE);
	DEG_register_node_typeinfo(&DNTI_BONE);
	
	//DEG_register_node_typeinfo(&DNTI_EVAL_PARTICLES);
	
	/* INNER */
	DEG_register_node_typeinfo(&DNTI_OP_PARAMETERS);
	DEG_register_node_typeinfo(&DNTI_OP_PROXY);
	DEG_register_node_typeinfo(&DNTI_OP_ANIMATION);
	DEG_register_node_typeinfo(&DNTI_OP_TRANSFORM);
	DEG_register_node_typeinfo(&DNTI_OP_GEOMETRY);
	DEG_register_node_typeinfo(&DNTI_OP_SEQUENCER);
	
	DEG_register_node_typeinfo(&DNTI_OP_UPDATE);
	DEG_register_node_typeinfo(&DNTI_OP_DRIVER);
	
	DEG_register_node_typeinfo(&DNTI_OP_POSE);
	DEG_register_node_typeinfo(&DNTI_OP_BONE);
	
	DEG_register_node_typeinfo(&DNTI_OP_PARTICLES);
	DEG_register_node_typeinfo(&DNTI_OP_RIGIDBODY);
}

/* Free registry on exit */
void DEG_free_node_types(void)
{
	BLI_ghash_free(_depsnode_typeinfo_registry, NULL, NULL);
}

/* Getters ------------------------------------------------- */

/* Get typeinfo for specified type */
DepsNodeFactory *DEG_get_node_factory(const eDepsNode_Type type)
{
	/* look up type - at worst, it doesn't exist in table yet, and we fail */
	return (DepsNodeFactory *)BLI_ghash_lookup(_depsnode_typeinfo_registry, SET_INT_IN_POINTER(type));
}

/* Get typeinfo for provided node */
DepsNodeFactory *DEG_node_get_factory(const DepsNode *node)
{
	if (!node)
		return NULL;
	
	return DEG_get_node_factory(node->type);
}

/* ******************************************************** */
