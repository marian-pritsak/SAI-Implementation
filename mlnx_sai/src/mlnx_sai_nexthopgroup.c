/*
 *  Copyright (C) 2017. Mellanox Technologies, Ltd. ALL RIGHTS RESERVED.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License"); you may
 *    not use this file except in compliance with the License. You may obtain
 *    a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 *    THIS CODE IS PROVIDED ON AN  *AS IS* BASIS, WITHOUT WARRANTIES OR
 *    CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
 *    LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS
 *    FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.
 *
 *    See the Apache Version 2.0 License for specific language governing
 *    permissions and limitations under the License.
 *
 */

#include "sai_windows.h"
#include "sai.h"
#include "mlnx_sai.h"
#include "assert.h"

#undef  __MODULE__
#define __MODULE__ SAI_NEXT_HOP_GROUP

typedef struct _mlnx_nh_data_t {
    sx_next_hop_t nh_id;
    uint32_t      object_index;
} mlnx_nh_bulk_data_t;
typedef struct _mlnx_nh_bulk_pair_t {
    sx_ecmp_id_t        group_id;
    mlnx_nh_bulk_data_t nh;
} mlnx_nh_bulk_pair_t;
typedef struct _mlnx_nh_bulk_data_t {
    mlnx_nh_bulk_pair_t *pairs;
    uint32_t             count;
} mlnx_nh_bulk_pair_list_t;

static sx_verbosity_level_t LOG_VAR_NAME(__MODULE__) = SX_VERBOSITY_LEVEL_WARNING;
static sai_status_t mlnx_next_hop_group_count_get(_In_ const sai_object_key_t   *key,
                                                  _Inout_ sai_attribute_value_t *value,
                                                  _In_ uint32_t                  attr_index,
                                                  _Inout_ vendor_cache_t        *cache,
                                                  void                          *arg);
static sai_status_t mlnx_next_hop_group_type_get(_In_ const sai_object_key_t   *key,
                                                 _Inout_ sai_attribute_value_t *value,
                                                 _In_ uint32_t                  attr_index,
                                                 _Inout_ vendor_cache_t        *cache,
                                                 void                          *arg);
static const sai_vendor_attribute_entry_t next_hop_group_vendor_attribs[] = {
    { SAI_NEXT_HOP_GROUP_ATTR_NEXT_HOP_COUNT,
      { false, false, false, true },
      { false, false, false, true },
      mlnx_next_hop_group_count_get, NULL,
      NULL, NULL },
    { SAI_NEXT_HOP_GROUP_ATTR_TYPE,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_next_hop_group_type_get, NULL,
      NULL, NULL },
    { SAI_NEXT_HOP_GROUP_ATTR_NEXT_HOP_MEMBER_LIST,
      { false, false, false, false },
      { false, false, false, false },
      NULL, NULL,
      NULL, NULL },
    { END_FUNCTIONALITY_ATTRIBS_ID,
      { false, false, false, false },
      { false, false, false, false },
      NULL, NULL,
      NULL, NULL }
};
static const mlnx_attr_enum_info_t        nh_group_enum_info[] = {
    [SAI_NEXT_HOP_GROUP_ATTR_TYPE] = ATTR_ENUM_VALUES_LIST(SAI_NEXT_HOP_GROUP_TYPE_ECMP)
};
const mlnx_obj_type_attrs_info_t          mlnx_next_hop_group_obj_type_info =
{ next_hop_group_vendor_attribs, OBJ_ATTRS_ENUMS_INFO(nh_group_enum_info) };
static sai_status_t mlnx_next_hop_group_member_group_id_get(_In_ const sai_object_key_t   *key,
                                                            _Inout_ sai_attribute_value_t *value,
                                                            _In_ uint32_t                  attr_index,
                                                            _Inout_ vendor_cache_t        *cache,
                                                            void                          *arg);
static sai_status_t mlnx_next_hop_group_member_hop_id_get(_In_ const sai_object_key_t   *key,
                                                          _Inout_ sai_attribute_value_t *value,
                                                          _In_ uint32_t                  attr_index,
                                                          _Inout_ vendor_cache_t        *cache,
                                                          void                          *arg);
static sai_status_t mlnx_next_hop_group_member_hop_weight_get(_In_ const sai_object_key_t   *key,
                                                              _Inout_ sai_attribute_value_t *value,
                                                              _In_ uint32_t                  attr_index,
                                                              _Inout_ vendor_cache_t        *cache,
                                                              void                          *arg);
static sai_status_t mlnx_next_hop_group_member_hop_weight_set(_In_ const sai_object_key_t      *key,
                                                              _In_ const sai_attribute_value_t *value,
                                                              void                             *arg);
static const sai_vendor_attribute_entry_t next_hop_group_member_vendor_attribs[] = {
    { SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_next_hop_group_member_group_id_get, NULL,
      NULL, NULL },
    { SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID,
      { true, false, false, true },
      { true, false, false, true },
      mlnx_next_hop_group_member_hop_id_get, NULL,
      NULL, NULL },
    { SAI_NEXT_HOP_GROUP_MEMBER_ATTR_WEIGHT,
      { true, false, true, true },
      { true, false, true, true },
      mlnx_next_hop_group_member_hop_weight_get, NULL,
      mlnx_next_hop_group_member_hop_weight_set, NULL },
    { END_FUNCTIONALITY_ATTRIBS_ID,
      { false, false, false, false },
      { false, false, false, false },
      NULL, NULL,
      NULL, NULL }
};
const mlnx_obj_type_attrs_info_t          mlnx_nh_group_member_obj_type_info =
{ next_hop_group_member_vendor_attribs, OBJ_ATTRS_ENUMS_INFO_EMPTY()};
static void next_hop_group_member_key_to_str(_In_ sai_object_id_t group_member_id, _Out_ char *key_str);
static void next_hop_group_key_to_str(_In_ sai_object_id_t next_hop_group_id, _Out_ char *key_str)
{
    uint32_t groupid;

    if (SAI_STATUS_SUCCESS != mlnx_object_to_type(next_hop_group_id, SAI_OBJECT_TYPE_NEXT_HOP_GROUP, &groupid, NULL)) {
        snprintf(key_str, MAX_KEY_STR_LEN, "invalid next hop group id");
    } else {
        snprintf(key_str, MAX_KEY_STR_LEN, "next hop group id %u", groupid);
    }
}

static sai_status_t mlnx_sdk_nhop_by_ecmp_id_get(sx_ecmp_id_t sdk_ecmp_id, sx_next_hop_t *sx_next_hop)
{
    uint32_t    sdk_next_hop_cnt = 1;
    sx_status_t status;

    status = sx_api_router_ecmp_get(gh_sdk, sdk_ecmp_id, sx_next_hop, &sdk_next_hop_cnt);
    if (SX_ERR(status)) {
        SX_LOG_ERR("Failed to get ecmp - %s id %u\n", SX_STATUS_MSG(status), sdk_ecmp_id);
        return sdk_to_sai(status);
    }

    if (1 != sdk_next_hop_cnt) {
        SX_LOG_ERR("Invalid next hosts count %u for ecmp id %u\n", sdk_next_hop_cnt, sdk_ecmp_id);
        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

static int sx_nhop_equal(const sx_next_hop_t *nhop1, const sx_next_hop_t *nhop2)
{
    return !memcmp(&nhop1->next_hop_key, &nhop2->next_hop_key, sizeof(nhop2->next_hop_key));
}

static sai_status_t mlnx_sdk_nhop_find_in_list(const sx_next_hop_t *next_hops,
                                               uint32_t             count,
                                               const sx_next_hop_t *match,
                                               uint32_t            *index)
{
    uint32_t ii;

    for (ii = 0; ii < count; ii++) {
        if (sx_nhop_equal(match, &next_hops[ii])) {
            *index = ii;
            return SAI_STATUS_SUCCESS;
        }
    }

    SX_LOG_ERR("Next group member does not exist in group\n");
    return SAI_STATUS_INVALID_OBJECT_ID;
}

/* static sai_status_t mlnx_translate_sai_next_hop_object(_In_ uint32_t              index, */
/*                                                        _In_ const sai_object_id_t next_hop_id, */
/*                                                        _Out_ sx_next_hop_t       *sx_next_hop) */
/* { */
/*     sx_ecmp_id_t sdk_ecmp_id; */
/*     sai_status_t status; */

/*     status = mlnx_object_to_type(next_hop_id, SAI_OBJECT_TYPE_NEXT_HOP, &sdk_ecmp_id, NULL); */
/*     if (SAI_ERR(status)) { */
/*         return status; */
/*     } */

/*     status = mlnx_sdk_nhop_by_ecmp_id_get(sdk_ecmp_id, sx_next_hop); */
/*     if (SAI_ERR(status)) { */
/*         SX_LOG_ERR("Failed to get nhop ecmp at index %u\n", index); */
/*         return status; */
/*     } */

/*     return SAI_STATUS_SUCCESS; */
/* } */

/* _Success_(return == SAI_STATUS_SUCCESS) */
/* static sai_status_t mlnx_translate_sai_next_hop_objects(_In_ uint32_t                      count, */
/*                                                         _In_ const sai_object_id_t        *next_hop_id, */
/*                                                         _Out_writes_(count) sx_next_hop_t *sx_next_hop) */
/* { */
/*     sai_status_t status; */
/*     uint32_t     ii; */

/*     for (ii = 0; ii < count; ii++) { */
/*         if (SAI_STATUS_SUCCESS != */
/*             (status = mlnx_translate_sai_next_hop_object(ii, next_hop_id[ii], &sx_next_hop[ii]))) { */
/*             return status; */
/*         } */
/*     } */

/*     return SAI_STATUS_SUCCESS; */
/* } */

static sai_status_t mlnx_nexthop_group_db_entry_alloc(_Out_ mlnx_nexthop_group_db_entry_t **nexthop_group_db_entry,
                                                      _Out_ mlnx_shm_rm_array_idx_t        *idx)
{
    sai_status_t status;
    void        *ptr;

    assert(nexthop_group_db_entry);
    assert(idx);

    status = mlnx_shm_rm_array_alloc(MLNX_SHM_RM_ARRAY_TYPE_NEXTHOP_GROUP, idx, &ptr);
    if (SAI_ERR(status)) {
        return status;
    }

    *nexthop_group_db_entry = ptr;

    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_nexthop_group_members_db_entry_alloc(_Out_ mlnx_nhg_block_db_entry_t **nexthop_group_members_db_entry,
                                                              _Out_ mlnx_shm_rm_array_idx_t    *idx)
{
    sai_status_t status;
    void        *ptr;

    assert(nexthop_group_members_db_entry);
    assert(idx);

    status = mlnx_shm_rm_array_alloc(MLNX_SHM_RM_ARRAY_TYPE_NEXTHOP_GROUP_MEMBER, idx, &ptr);
    if (SAI_ERR(status)) {
        return status;
    }

    *nexthop_group_members_db_entry = ptr;

    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_nexthop_group_db_entry_idx_to_data(_In_ mlnx_shm_rm_array_idx_t          idx,
                                                            _Out_ mlnx_nexthop_group_db_entry_t **nexthop_group_db_entry)
{
    sai_status_t status;
    void        *data;

    status = mlnx_shm_rm_array_idx_to_ptr(idx, &data);
    if (SAI_ERR(status)) {
        return status;
    }

    *nexthop_group_db_entry = (mlnx_nexthop_group_db_entry_t*)data;

    if (!(*nexthop_group_db_entry)->array_hdr.is_used) {
        SX_LOG_ERR("Encap Nexthop Group DB entry at index %u is removed or not created yet.\n", idx.idx);
        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_nexthop_group_db_entry_free(_In_ mlnx_shm_rm_array_idx_t idx)
{
    sai_status_t                   status;
    mlnx_nexthop_group_db_entry_t *nexthop_group_db_entry;

    if (MLNX_SHM_RM_ARRAY_IDX_IS_UNINITIALIZED(idx)) {
        return SAI_STATUS_SUCCESS;
    }

    status = mlnx_nexthop_group_db_entry_idx_to_data(idx, &nexthop_group_db_entry);
    if (SAI_ERR(status)) {
        return status;
    }

    memset(&nexthop_group_db_entry->data, 0, sizeof(nexthop_group_db_entry->data));

    return mlnx_shm_rm_array_free(idx);
}

static sai_status_t mlnx_nexthop_group_members_db_entry_idx_to_data(_In_ mlnx_shm_rm_array_idx_t          idx,
                                                                    _Out_ mlnx_nhg_block_db_entry_t     **nexthop_group_members_db_entry)
{
    sai_status_t status;
    void        *data;

    status = mlnx_shm_rm_array_idx_to_ptr(idx, &data);
    if (SAI_ERR(status)) {
        return status;
    }

    *nexthop_group_members_db_entry = (mlnx_nhg_block_db_entry_t*)data;

    if (!(*nexthop_group_members_db_entry)->array_hdr.is_used) {
        SX_LOG_ERR("Encap Nexthop Group Members DB entry at index %u is removed or not created yet.\n", idx.idx);
        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_nexthop_group_members_db_entry_free(_In_ mlnx_shm_rm_array_idx_t idx)
{
    sai_status_t                   status;
    mlnx_nhg_block_db_entry_t     *nexthop_group_members_db_entry;

    if (MLNX_SHM_RM_ARRAY_IDX_IS_UNINITIALIZED(idx)) {
        return SAI_STATUS_SUCCESS;
    }

    status = mlnx_nexthop_group_members_db_entry_idx_to_data(idx, &nexthop_group_members_db_entry);
    if (SAI_ERR(status)) {
        return status;
    }

    memset(&nexthop_group_members_db_entry->data, 0, sizeof(nexthop_group_members_db_entry->data));

    return mlnx_shm_rm_array_free(idx);
}

static sai_status_t mlnx_nexthop_group_oid_create(_In_ mlnx_shm_rm_array_idx_t idx, _Out_ sai_object_id_t *oid)
{
    sai_status_t      status;
    mlnx_object_id_t *mlnx_oid = (mlnx_object_id_t*)oid;

    assert(oid);

    status = mlnx_shm_rm_idx_validate(idx);
    if (SAI_ERR(status)) {
        return status;
    }

    if (idx.type != MLNX_SHM_RM_ARRAY_TYPE_NEXTHOP_GROUP) {
        return SAI_STATUS_FAILURE;
    }

    memset(oid, 0, sizeof(*oid));

    mlnx_oid->object_type             = SAI_OBJECT_TYPE_NEXT_HOP_GROUP;
    mlnx_oid->id.nexthop_group_db_idx = idx;

    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_nexthop_group_oid_to_data(_In_ sai_object_id_t                  oid,
                                            _Out_ mlnx_nexthop_group_db_entry_t **nexthop_group_db_entry,
                                            _Out_ mlnx_shm_rm_array_idx_t        *idx)
{
    sai_status_t     status;
    mlnx_object_id_t mlnx_oid;

    assert(nexthop_group_db_entry);

    status = sai_to_mlnx_object_id(SAI_OBJECT_TYPE_NEXT_HOP_GROUP, oid, &mlnx_oid);
    if (SAI_ERR(status)) {
        return status;
    }

    status = mlnx_nexthop_group_db_entry_idx_to_data(mlnx_oid.id.nexthop_group_db_idx, nexthop_group_db_entry);
    if (SAI_ERR(status)) {
        return status;
    }

    if (idx) {
        *idx = mlnx_oid.id.nexthop_group_db_idx;
    }

    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_update_group_sx_ecmp_contents(_Inout_ mlnx_nexthop_group_db_entry_t *group_db_entry)
{
    sai_status_t status;
    sx_status_t sx_status;
    mlnx_nhg_block_db_entry_t *members_db_entry;
    sx_next_hop_t                  ecmp_next_hops[NEXTHOP_GROUP_MEMBER_BLOCK_SIZE];
    uint32_t ii, jj;

    /* TODO: support more than one block */
    status = mlnx_nexthop_group_members_db_entry_idx_to_data(group_db_entry->data.members_idx, &members_db_entry);
    if (SAI_ERR(status)) {
        return status;
    }

    for (ii = 0; ii < NUMBER_OF_LOCAL_VNETS; ii++) {
        if (group_db_entry->data.vrf_data[ii].associated_vrf != SAI_NULL_OBJECT_ID) {
            for (jj = 0; jj < members_db_entry->data.count; jj++) {
                sx_ecmp_id_t nhop_ecmp_id;

                if (members_db_entry->data.members[jj].is_db_entry) {
                    sai_object_id_t nhop_id;

                    status = mlnx_encap_nexthop_oid_create(members_db_entry->data.members[jj].entry.nexthop_db_idx,
                                                           &nhop_id);
                    if (SAI_ERR(status)) {
                        return status;
                    }

                    status = mlnx_encap_nexthop_get_ecmp(nhop_id,
                                                         group_db_entry->data.vrf_data[ii].associated_vrf,
                                                         &nhop_ecmp_id);
                    if (SAI_ERR(status)) {
                        return status;
                    }
                } else {
                    nhop_ecmp_id = members_db_entry->data.members[jj].entry.sx_nexthop_id;
                }

                status = mlnx_sdk_nhop_by_ecmp_id_get(nhop_ecmp_id, &ecmp_next_hops[jj]);
                if (SAI_ERR(status)) {
                    return status;
                }
            }

            sx_status = sx_api_router_ecmp_set(gh_sdk,
                                              SX_ACCESS_CMD_SET,
                                              &group_db_entry->data.vrf_data[ii].sx_ecmp_id,
                                              ecmp_next_hops,
                                              &members_db_entry->data.count);
            if (SAI_ERR(sx_status)) {
                SX_LOG_ERR("Failed to set ecmp - %s.\n", SX_STATUS_MSG(status));
                return sdk_to_sai(sx_status);
            }
        }
    }

    return status;
}

static sai_status_t mlnx_add_encap_nhop_to_group(_In_ mlnx_shm_rm_array_idx_t    nhop_db_idx,
                                                 _In_ mlnx_nexthop_group_db_entry_t *group_db_entry,
                                                 _Out_ uint32_t *member_index)
{
    sai_status_t status;
    mlnx_nhg_block_db_entry_t *members_db_entry;

    assert(group_db_entry);
    assert(member_index);

    status = mlnx_nexthop_group_members_db_entry_idx_to_data(group_db_entry->data.members_idx, &members_db_entry);
    if (SAI_ERR(status)) {
        return status;
    }

    /* TODO: support more than one block */
    if (members_db_entry->data.count == NEXTHOP_GROUP_MEMBER_BLOCK_SIZE) {
        status = SAI_STATUS_INSUFFICIENT_RESOURCES;
        return status;
    }

    members_db_entry->data.members[members_db_entry->data.count].is_db_entry = true;
    members_db_entry->data.members[members_db_entry->data.count].entry.nexthop_db_idx = nhop_db_idx;
    members_db_entry->data.count++;

    status = mlnx_update_group_sx_ecmp_contents(group_db_entry);
    if (SAI_ERR(status)) {
        return status;
    }

    *member_index = members_db_entry->data.count;
    return status;
}

static sai_status_t mlnx_add_sdk_nhop_to_group(_In_ sx_ecmp_id_t nhop_ecmp_id,
                                               _In_ mlnx_nexthop_group_db_entry_t *group_db_entry,
                                               _Out_ uint32_t *member_index)
{
    sai_status_t status;
    mlnx_nhg_block_db_entry_t *members_db_entry;

    status = mlnx_nexthop_group_members_db_entry_idx_to_data(group_db_entry->data.members_idx, &members_db_entry);
    if (SAI_ERR(status)) {
        return status;
    }

    /* TODO: support more than one block */
    if (members_db_entry->data.count == NEXTHOP_GROUP_MEMBER_BLOCK_SIZE) {
        status = SAI_STATUS_INSUFFICIENT_RESOURCES;
        return status;
    }

    members_db_entry->data.members[members_db_entry->data.count].is_db_entry = false;
    members_db_entry->data.members[members_db_entry->data.count].entry.sx_nexthop_id = nhop_ecmp_id;
    members_db_entry->data.count++;

    status = mlnx_update_group_sx_ecmp_contents(group_db_entry);
    if (SAI_ERR(status)) {
        return status;
    }

    *member_index = members_db_entry->data.count;
    return status;
}

/*
 * Routine Description:
 *    Create next hop group
 *
 * Arguments:
 *    [out] next_hop_group_id - next hop group id
 *    [in] attr_count - number of attributes
 *    [in] attr_list - array of attributes
 *
 * Return Values:
 *    SAI_STATUS_SUCCESS on success
 *    Failure status code on error
 */
static sai_status_t mlnx_create_next_hop_group(_Out_ sai_object_id_t     * next_hop_group_id,
                                               _In_ sai_object_id_t        switch_id,
                                               _In_ uint32_t               attr_count,
                                               _In_ const sai_attribute_t *attr_list)
{
    char                                 list_str[MAX_LIST_VALUE_STR_LEN];
    char                                 key_str[MAX_KEY_STR_LEN];
    uint32_t                             type_index;
    sai_status_t                         status;
    const sai_attribute_value_t         *type;
    mlnx_nexthop_group_db_entry_t       *group_db_entry;
    mlnx_nhg_block_db_entry_t           *members_db_entry;
    mlnx_shm_rm_array_idx_t              group_idx;
    mlnx_shm_rm_array_idx_t              members_idx;

    SX_LOG_ENTER();

    if (NULL == next_hop_group_id) {
        SX_LOG_ERR("NULL next hop group id param\n");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (SAI_STATUS_SUCCESS !=
        (status =
             check_attribs_metadata(attr_count, attr_list, SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
                                    next_hop_group_vendor_attribs,
                                    SAI_COMMON_API_CREATE))) {
        SX_LOG_ERR("Failed attribs check\n");
        return status;
    }

    sai_attr_list_to_str(attr_count, attr_list, SAI_OBJECT_TYPE_NEXT_HOP_GROUP, MAX_LIST_VALUE_STR_LEN, list_str);
    SX_LOG_NTC("Create next hop group, %s\n", list_str);

    status = find_attrib_in_list(attr_count, attr_list, SAI_NEXT_HOP_GROUP_ATTR_TYPE, &type, &type_index);
    assert(SAI_STATUS_SUCCESS == status);

    if (SAI_NEXT_HOP_GROUP_TYPE_ECMP != type->s32) {
        SX_LOG_ERR("Invalid next hop group type %d on create\n", type->s32);
        return SAI_STATUS_INVALID_ATTR_VALUE_0 + type_index;
    }

    sai_db_write_lock();

    /* Allocate group db entry */
    status = mlnx_nexthop_group_db_entry_alloc(&group_db_entry, &group_idx);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failure allocating Nexthop Group DB entry.\n");
        sai_db_unlock();
        return status;
    }

    memset(&group_db_entry->data, 0, sizeof(group_db_entry->data));

    /* Allocate members db entry */
    status = mlnx_nexthop_group_members_db_entry_alloc(&members_db_entry, &members_idx);
    if (SAI_ERR(status)) {
        mlnx_nexthop_group_db_entry_free(group_idx); /* don't care about status */
        SX_LOG_ERR("Failure allocating Nexthop Group Members DB entry.\n");
        sai_db_unlock();
        return status;
    }

    memset(&members_db_entry->data, 0, sizeof(members_db_entry->data));
    group_db_entry->data.members_idx = members_idx;

    status = mlnx_nexthop_group_oid_create(group_idx, next_hop_group_id);
    if (SAI_ERR(status)) {
        mlnx_nexthop_group_members_db_entry_free(members_idx); /* don't care about status */
        mlnx_nexthop_group_db_entry_free(group_idx); /* don't care about status */
        SX_LOG_ERR("Failure creating Nexthop Group OID.\n");
        sai_db_unlock();
        return status;
    }

    sai_db_unlock();

    next_hop_group_key_to_str(*next_hop_group_id, key_str);
    SX_LOG_NTC("Created next hop group %s\n", key_str);

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *    Remove next hop group
 *
 * Arguments:
 *    [in] next_hop_group_id - next hop group id
 *
 * Return Values:
 *    SAI_STATUS_SUCCESS on success
 *    Failure status code on error
 */
static sai_status_t mlnx_remove_next_hop_group(_In_ sai_object_id_t next_hop_group_id)
{
    char                           key_str[MAX_KEY_STR_LEN];
    sai_status_t                   status;
    mlnx_shm_rm_array_idx_t        group_idx;
    mlnx_nexthop_group_db_entry_t *group_db_entry;

    SX_LOG_ENTER();

    next_hop_group_key_to_str(next_hop_group_id, key_str);
    SX_LOG_NTC("Remove next hop group %s\n", key_str);

    sai_db_write_lock();

    if (SAI_STATUS_SUCCESS !=
        (status = mlnx_object_to_type(next_hop_group_id, SAI_OBJECT_TYPE_NEXT_HOP_GROUP, (uint32_t *)&group_idx, NULL))) {
        goto out;
    }

    status = mlnx_nexthop_group_oid_to_data(next_hop_group_id, &group_db_entry, &group_idx);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failure getting data from DB.\n");
        goto out;
    }

    for (int32_t i = 0; i < NUMBER_OF_LOCAL_VNETS; ++i) {
        if (group_db_entry->data.vrf_data[i].associated_vrf != SAI_NULL_OBJECT_ID) {
            SX_LOG_ERR("Internal entites weren't unitialized, memory leak.\n");
            status = SAI_STATUS_OBJECT_IN_USE;
            goto out;
        }
    }

    /* TODO: free all blocks if more than one used */
    status = mlnx_nexthop_group_members_db_entry_free(group_db_entry->data.members_idx);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failure freeing members DB entry.\n");
        goto out;
    }

    status = mlnx_nexthop_group_db_entry_free(group_idx);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failure freeing group DB entry.\n");
        goto out;
    }
    /* if (SX_STATUS_SUCCESS != */
    /*     (status = sx_api_router_ecmp_set(gh_sdk, SX_ACCESS_CMD_DESTROY, &sdk_ecmp_id, NULL, &next_hop_cnt))) { */
    /*     SX_LOG_ERR("Failed to destroy ecmp - %s.\n", SX_STATUS_MSG(status)); */
    /*     return sdk_to_sai(status); */
    /* } */

out:
    sai_db_unlock();
    SX_LOG_EXIT();
    return status;
}

/*
 * Routine Description:
 *    Set Next Hop Group attribute
 *
 * Arguments:
 *    [in] sai_next_hop_group_id_t - next_hop_group_id
 *    [in] attr - attribute
 *
 * Return Values:
 *    SAI_STATUS_SUCCESS on success
 *    Failure status code on error
 */
static sai_status_t mlnx_set_next_hop_group_attribute(_In_ sai_object_id_t        next_hop_group_id,
                                                      _In_ const sai_attribute_t *attr)
{
    const sai_object_key_t key = { .key.object_id = next_hop_group_id };
    char                   key_str[MAX_KEY_STR_LEN];

    SX_LOG_ENTER();

    next_hop_group_key_to_str(next_hop_group_id, key_str);
    return sai_set_attribute(&key, key_str, SAI_OBJECT_TYPE_NEXT_HOP_GROUP, next_hop_group_vendor_attribs, attr);
}

/*
 * Routine Description:
 *    Get Next Hop Group attribute
 *
 * Arguments:
 *    [in] sai_next_hop_group_id_t - next_hop_group_id
 *    [in] attr_count - number of attributes
 *    [inout] attr_list - array of attributes
 *
 * Return Values:
 *    SAI_STATUS_SUCCESS on success
 *    Failure status code on error
 */
static sai_status_t mlnx_get_next_hop_group_attribute(_In_ sai_object_id_t     next_hop_group_id,
                                                      _In_ uint32_t            attr_count,
                                                      _Inout_ sai_attribute_t *attr_list)
{
    const sai_object_key_t key = { .key.object_id = next_hop_group_id };
    char                   key_str[MAX_KEY_STR_LEN];

    SX_LOG_ENTER();

    next_hop_group_key_to_str(next_hop_group_id, key_str);
    return sai_get_attributes(&key,
                              key_str,
                              SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
                              next_hop_group_vendor_attribs,
                              attr_count,
                              attr_list);
}

/* Number of next hops in the group [uint32_t] */
static sai_status_t mlnx_next_hop_group_count_get(_In_ const sai_object_key_t   *key,
                                                  _Inout_ sai_attribute_value_t *value,
                                                  _In_ uint32_t                  attr_index,
                                                  _Inout_ vendor_cache_t        *cache,
                                                  void                          *arg)
{
    uint32_t     sdk_next_hop_cnt;
    sx_ecmp_id_t sdk_ecmp_id;
    sai_status_t status;

    SX_LOG_ENTER();

    if (SAI_STATUS_SUCCESS !=
        (status = mlnx_object_to_type(key->key.object_id, SAI_OBJECT_TYPE_NEXT_HOP_GROUP, &sdk_ecmp_id, NULL))) {
        return status;
    }

    sdk_next_hop_cnt = 0;
    if (SX_STATUS_SUCCESS != (status = sx_api_router_ecmp_get(gh_sdk, sdk_ecmp_id, NULL, &sdk_next_hop_cnt))) {
        SX_LOG_ERR("Failed to get ecmp - %s.\n", SX_STATUS_MSG(status));
        return sdk_to_sai(status);
    }
    value->u32 = sdk_next_hop_cnt;

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

/* Next hop group type [sai_next_hop_group_type_t] */
static sai_status_t mlnx_next_hop_group_type_get(_In_ const sai_object_key_t   *key,
                                                 _Inout_ sai_attribute_value_t *value,
                                                 _In_ uint32_t                  attr_index,
                                                 _Inout_ vendor_cache_t        *cache,
                                                 void                          *arg)
{
    SX_LOG_ENTER();

    value->s32 = SAI_NEXT_HOP_GROUP_TYPE_ECMP;

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_nexthop_group_log_set(sx_verbosity_level_t level)
{
    LOG_VAR_NAME(__MODULE__) = level;

    return SAI_STATUS_SUCCESS;
}

static sai_status_t nhop_group_member_to_oid(mlnx_shm_rm_array_idx_t group_idx, uint16_t member_idx, sai_object_id_t *member_id)
{
    mlnx_object_id_t member_obj_id;
    sai_status_t     status;

    member_obj_id.ext.nhop_group_member.member_idx   = member_idx;
    member_obj_id.id.nexthop_group_db_idx = group_idx;

    status = mlnx_object_id_to_sai(SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER, &member_obj_id, member_id);
    if (SAI_ERR(status)) {
        return status;
    }

    return SAI_STATUS_SUCCESS;
}

static sai_status_t nhop_group_member_parse_oid(sai_object_id_t member_id,
                                                sx_ecmp_id_t   *sx_group_id,
                                                sx_ecmp_id_t   *sx_nhop_id)
{
    /* mlnx_object_id_t member_obj_id; */
    /* uint32_t         group_id; */
    /* uint32_t         nhop_id; */
    /* sai_status_t     status; */

    /* status = sai_to_mlnx_object_id(SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER, member_id, &member_obj_id); */
    /* if (SAI_ERR(status)) { */
    /*     return status; */
    /* } */

    /* group_id     = member_obj_id.id.nhop_group_member_low.group_id; */
    /* group_id    |= member_obj_id.ext.nhop_group_member_high.group_id << 24; */
    /* *sx_group_id = group_id; */
    *sx_group_id = 0;

    /* nhop_id     = member_obj_id.id.nhop_group_member_low.nhop_id; */
    /* nhop_id    |= member_obj_id.ext.nhop_group_member_high.nhop_id << 24; */
    /* *sx_nhop_id = nhop_id; */
    *sx_nhop_id = 0;

    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_next_hop_group_member_group_id_get(_In_ const sai_object_key_t   *key,
                                                            _Inout_ sai_attribute_value_t *value,
                                                            _In_ uint32_t                  attr_index,
                                                            _Inout_ vendor_cache_t        *cache,
                                                            void                          *arg)
{
    sx_ecmp_id_t sx_group_id;
    sx_ecmp_id_t sx_nhop_id;
    sai_status_t status;

    SX_LOG_ENTER();

    status = nhop_group_member_parse_oid(key->key.object_id, &sx_group_id, &sx_nhop_id);
    if (SAI_ERR(status)) {
        goto out;
    }

    status = mlnx_create_object(SAI_OBJECT_TYPE_NEXT_HOP_GROUP, sx_group_id, NULL, &value->oid);
    if (SAI_ERR(status)) {
        goto out;
    }

out:
    SX_LOG_EXIT();
    return status;
}

static sai_status_t mlnx_next_hop_group_member_hop_id_get(_In_ const sai_object_key_t   *key,
                                                          _Inout_ sai_attribute_value_t *value,
                                                          _In_ uint32_t                  attr_index,
                                                          _Inout_ vendor_cache_t        *cache,
                                                          void                          *arg)
{
    sx_ecmp_id_t sx_group_id;
    sx_ecmp_id_t sx_nhop_id;
    sai_status_t status;

    SX_LOG_ENTER();

    status = nhop_group_member_parse_oid(key->key.object_id, &sx_group_id, &sx_nhop_id);
    if (SAI_ERR(status)) {
        goto out;
    }

    status = mlnx_create_object(SAI_OBJECT_TYPE_NEXT_HOP, sx_nhop_id, NULL, &value->oid);
    if (SAI_ERR(status)) {
        goto out;
    }

out:
    SX_LOG_EXIT();
    return status;
}

static sai_status_t mlnx_next_hop_group_member_hop_weight_get(_In_ const sai_object_key_t   *key,
                                                              _Inout_ sai_attribute_value_t *value,
                                                              _In_ uint32_t                  attr_index,
                                                              _Inout_ vendor_cache_t        *cache,
                                                              void                          *arg)
{
    uint32_t      next_hop_count = ECMP_MAX_PATHS;
    sx_next_hop_t ecmp_next_hops[ECMP_MAX_PATHS];
    sx_ecmp_id_t  sx_group_id;
    sx_ecmp_id_t  sx_nhop_id;
    sx_next_hop_t next_hop;
    sai_status_t  status;
    uint32_t      ii;

    SX_LOG_ENTER();

    status = nhop_group_member_parse_oid(key->key.object_id, &sx_group_id, &sx_nhop_id);
    if (SAI_ERR(status)) {
        goto out;
    }

    status = sx_api_router_ecmp_get(gh_sdk, sx_group_id, ecmp_next_hops, &next_hop_count);
    if (SX_ERR(status)) {
        SX_LOG_ERR("Failed to get ecmp - %s.\n", SX_STATUS_MSG(status));
        status = sdk_to_sai(status);
        goto out;
    }

    status = mlnx_sdk_nhop_by_ecmp_id_get(sx_nhop_id, &next_hop);
    if (SAI_ERR(status)) {
        goto out;
    }

    status = mlnx_sdk_nhop_find_in_list(ecmp_next_hops, next_hop_count, &next_hop, &ii);
    if (SAI_ERR(status)) {
        goto out;
    }

    value->u32 = ecmp_next_hops[ii].next_hop_data.weight;

out:
    SX_LOG_EXIT();
    return status;
}

static sai_status_t mlnx_next_hop_group_member_hop_weight_set(_In_ const sai_object_key_t      *key,
                                                              _In_ const sai_attribute_value_t *value,
                                                              void                             *arg)
{
    uint32_t      next_hop_count = ECMP_MAX_PATHS;
    sx_next_hop_t ecmp_next_hops[ECMP_MAX_PATHS];
    sx_ecmp_id_t  sx_group_id;
    sx_ecmp_id_t  sx_nhop_id;
    sx_next_hop_t next_hop;
    sai_status_t  status;
    uint32_t      ii;

    SX_LOG_ENTER();

    status = nhop_group_member_parse_oid(key->key.object_id, &sx_group_id, &sx_nhop_id);
    if (SAI_ERR(status)) {
        goto out;
    }

    status = sx_api_router_ecmp_get(gh_sdk, sx_group_id, ecmp_next_hops, &next_hop_count);
    if (SX_ERR(status)) {
        SX_LOG_ERR("Failed to get ecmp - %s.\n", SX_STATUS_MSG(status));
        status = sdk_to_sai(status);
        goto out;
    }

    status = mlnx_sdk_nhop_by_ecmp_id_get(sx_nhop_id, &next_hop);
    if (SAI_ERR(status)) {
        goto out;
    }

    status = mlnx_sdk_nhop_find_in_list(ecmp_next_hops, next_hop_count, &next_hop, &ii);
    if (SAI_ERR(status)) {
        goto out;
    }

    ecmp_next_hops[ii].next_hop_data.weight = value->u32;

    status = sx_api_router_ecmp_set(gh_sdk, SX_ACCESS_CMD_SET, &sx_group_id, ecmp_next_hops, &next_hop_count);
    if (SX_ERR(status)) {
        SX_LOG_ERR("Failed to set ecmp - %s.\n", SX_STATUS_MSG(status));
        status = sdk_to_sai(status);
        goto out;
    }

out:
    SX_LOG_EXIT();
    return status;
}

/**
 * @brief Create next hop group member
 *
 * @param[out] next_hop_group_member_id - next hop group member id
 * @param[in] attr_count - number of attributes
 * @param[in] attr_list - array of attributes
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
static sai_status_t mlnx_create_next_hop_group_member(_Out_ sai_object_id_t     * next_hop_group_member_id,
                                                      _In_ sai_object_id_t        switch_id,
                                                      _In_ uint32_t               attr_count,
                                                      _In_ const sai_attribute_t *attr_list)
{
    const sai_attribute_value_t   *next_hop_group_id = NULL, *next_hop_id = NULL, *weight = NULL;
    uint32_t                       group_index, next_hop_index, weight_index;
    uint32_t                       member_index;
    char                           list_str[MAX_LIST_VALUE_STR_LEN];
    char                           key_str[MAX_KEY_STR_LEN];
    /* sx_ecmp_id_t                   group_ecmp_id; */
    sx_ecmp_id_t                   nhop_ecmp_id;
    sai_status_t                   status;
    mlnx_shm_rm_array_idx_t        group_idx;
    mlnx_nexthop_group_db_entry_t *group_db_entry;
    uint32_t     data;
    uint16_t     use_db;
    mlnx_encap_nexthop_db_entry_t *nhop_db_entry;
    mlnx_shm_rm_array_idx_t        nhop_idx;

    SX_LOG_ENTER();

    if (NULL == next_hop_group_member_id) {
        SX_LOG_ERR("NULL next hop group member id param\n");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    status = check_attribs_metadata(attr_count,
                                    attr_list,
                                    SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER,
                                    next_hop_group_member_vendor_attribs,
                                    SAI_COMMON_API_CREATE);
    if (SAI_ERR(status)) {
        return status;
    }

    sai_attr_list_to_str(attr_count, attr_list, SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER, MAX_LIST_VALUE_STR_LEN,
                         list_str);
    SX_LOG_NTC("Create next hop group member, %s\n", list_str);

    status = find_attrib_in_list(attr_count,
                                 attr_list,
                                 SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID,
                                 &next_hop_group_id,
                                 &group_index);
    assert(SAI_STATUS_SUCCESS == status);

    status = find_attrib_in_list(attr_count,
                                 attr_list,
                                 SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID,
                                 &next_hop_id,
                                 &next_hop_index);
    assert(SAI_STATUS_SUCCESS == status);

    /* ignore weight for now */
    find_attrib_in_list(attr_count, attr_list, SAI_NEXT_HOP_GROUP_MEMBER_ATTR_WEIGHT, &weight, &weight_index);

    sai_db_write_lock();

    if (SAI_STATUS_SUCCESS !=
        (status = mlnx_object_to_type(next_hop_group_id->oid, SAI_OBJECT_TYPE_NEXT_HOP_GROUP, (uint32_t *)&group_idx, NULL))) {
        goto out;
    }

    status = mlnx_nexthop_group_db_entry_idx_to_data(group_idx, &group_db_entry);
    if (SAI_ERR(status)) {
        goto out;
    }

    status = mlnx_object_to_type(next_hop_id->oid, SAI_OBJECT_TYPE_NEXT_HOP, &data, (uint8_t*)&use_db);
    if (SAI_ERR(status)) {
        goto out;
    }

    if (use_db) {
        nhop_idx = *(mlnx_shm_rm_array_idx_t*)&data;

        status = mlnx_encap_nexthop_oid_to_data(next_hop_id->oid, &nhop_db_entry, &nhop_idx);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Failure getting data from DB.\n");
            goto out;
        }

        status = mlnx_add_encap_nhop_to_group(nhop_idx, group_db_entry, &member_index);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Failure adding encap next hop to group.\n");
            goto out;
        }
    } else {
        nhop_ecmp_id = (sx_ecmp_id_t)data;

        status = mlnx_add_sdk_nhop_to_group(nhop_ecmp_id, group_db_entry, &member_index);
        if (SAI_ERR(status)) {
            SX_LOG_ERR("Failure adding next hop to group.\n");
            goto out;
        }
    }

    status = nhop_group_member_to_oid(group_idx, member_index, next_hop_group_member_id);

    next_hop_group_member_key_to_str(*next_hop_group_member_id, key_str);
    SX_LOG_NTC("Created next hop group member %s\n", key_str);

out:
    sai_db_unlock();
    SX_LOG_EXIT();
    return status;
}

/**
 * @brief Remove next hop group member
 *
 * @param[in] next_hop_group_member_id - next hop group member id
 *
 * @return SAI_STATUS_SUCCESS on success Failure status code on error
 */
static sai_status_t mlnx_remove_next_hop_group_member(_In_ sai_object_id_t next_hop_group_member_id)
{
    sx_next_hop_t next_hops[ECMP_MAX_PATHS];
    sx_next_hop_t next_hop_remove;
    uint32_t      next_hop_count = ECMP_MAX_PATHS;
    sx_ecmp_id_t  sx_group_id;
    sx_ecmp_id_t  sx_nhop_id;
    sai_status_t  status;
    uint32_t      ii;

    SX_LOG_ENTER();

    status = nhop_group_member_parse_oid(next_hop_group_member_id, &sx_group_id, &sx_nhop_id);
    if (SAI_ERR(status)) {
        return status;
    }

    SX_LOG_NTC("Remove next hop %u from next hop group %u\n", sx_nhop_id, sx_group_id);

    status = sx_api_router_ecmp_get(gh_sdk, sx_group_id, next_hops, &next_hop_count);
    if (SX_ERR(status)) {
        SX_LOG_ERR("Failed to get ecmp - %s.\n", SX_STATUS_MSG(status));
        return sdk_to_sai(status);
    }

    status = mlnx_sdk_nhop_by_ecmp_id_get(sx_nhop_id, &next_hop_remove);
    if (SAI_ERR(status)) {
        return status;
    }

    status = mlnx_sdk_nhop_find_in_list(next_hops, next_hop_count, &next_hop_remove, &ii);
    if (SAI_ERR(status)) {
        return status;
    }
    next_hops[ii] = next_hops[--next_hop_count];

    status = sx_api_router_ecmp_set(gh_sdk, SX_ACCESS_CMD_SET, &sx_group_id, next_hops, &next_hop_count);
    if (SX_ERR(status)) {
        SX_LOG_ERR("Failed to set ecmp - %s.\n", SX_STATUS_MSG(status));
        return sdk_to_sai(status);
    }

    SX_LOG_EXIT();
    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_next_hop_bulk_init(_In_ mlnx_nh_bulk_pair_list_t *nh_bulk_data, _In_ uint32_t object_count)
{
    assert(nh_bulk_data);
    assert(!nh_bulk_data->pairs);

    nh_bulk_data->pairs = calloc(object_count, sizeof(mlnx_nh_bulk_pair_t));
    if (!nh_bulk_data->pairs) {
        return SAI_STATUS_NO_MEMORY;
    }

    nh_bulk_data->count = 0;

    return SAI_STATUS_SUCCESS;
}

static void mlnx_next_hop_bulk_deinit(_In_ mlnx_nh_bulk_pair_list_t *nh_bulk_data)
{
    assert(nh_bulk_data);

    free(nh_bulk_data->pairs);
}

static sai_status_t mlnx_next_hop_bulk_map_add(_In_ mlnx_nh_bulk_pair_list_t  *nh_bulk_data,
                                               _In_ const mlnx_nh_bulk_pair_t *bulk_pair)
{
    assert(nh_bulk_data);
    assert(bulk_pair);

    memcpy(&nh_bulk_data->pairs[nh_bulk_data->count], bulk_pair, sizeof(mlnx_nh_bulk_pair_t));
    nh_bulk_data->count++;

    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_next_hop_bulk_create_member_add(_In_ mlnx_nh_bulk_pair_list_t *nh_bulk_data,
                                                         _In_ uint32_t                  attr_count,
                                                         _In_ const sai_attribute_t    *attr_list,
                                                         _In_ uint32_t                  object_index,
                                                         _Out_ sai_object_id_t         *group_member_oid)
{
    /* sai_status_t                 status; */
    /* const sai_attribute_value_t *group_attr = NULL, *next_hop_attr = NULL, *weight_attr = NULL; */
    /* uint32_t                     group_index, next_hop_index, weight_index; */
    /* sx_ecmp_id_t                 group_ecmp_id, nhop_ecmp_id; */
    /* mlnx_nh_bulk_pair_t          bulk_pair; */

    /* assert(nh_bulk_data); */
    /* assert(attr_list); */
    /* assert(group_member_oid); */

    /* status = check_attribs_metadata(attr_count, */
    /*                                 attr_list, */
    /*                                 SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER, */
    /*                                 next_hop_group_member_vendor_attribs, */
    /*                                 SAI_COMMON_API_CREATE); */
    /* if (SAI_ERR(status)) { */
    /*     return status; */
    /* } */

    /* status = find_attrib_in_list(attr_count, */
    /*                              attr_list, */
    /*                              SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID, */
    /*                              &group_attr, */
    /*                              &group_index); */
    /* assert(SAI_STATUS_SUCCESS == status); */

    /* status = find_attrib_in_list(attr_count, */
    /*                              attr_list, */
    /*                              SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID, */
    /*                              &next_hop_attr, */
    /*                              &next_hop_index); */
    /* assert(SAI_STATUS_SUCCESS == status); */

    /* find_attrib_in_list(attr_count, attr_list, SAI_NEXT_HOP_GROUP_MEMBER_ATTR_WEIGHT, &weight_attr, &weight_index); */

    /* status = mlnx_object_to_type(group_attr->oid, SAI_OBJECT_TYPE_NEXT_HOP_GROUP, &group_ecmp_id, NULL); */
    /* if (SAI_ERR(status)) { */
    /*     return status; */
    /* } */

    /* bulk_pair.group_id = group_ecmp_id; */

    /* status = mlnx_object_to_type(next_hop_attr->oid, SAI_OBJECT_TYPE_NEXT_HOP, &nhop_ecmp_id, NULL); */
    /* if (SAI_ERR(status)) { */
    /*     return status; */
    /* } */

    /* status = mlnx_translate_sai_next_hop_object(object_index, next_hop_attr->oid, &bulk_pair.nh.nh_id); */
    /* if (SAI_ERR(status)) { */
    /*     return status; */
    /* } */

    /* if (weight_attr) { */
    /*     bulk_pair.nh.nh_id.next_hop_data.weight = weight_attr->u32; */
    /* } else { */
    /*     bulk_pair.nh.nh_id.next_hop_data.weight = 1; */
    /* } */

    /* bulk_pair.nh.object_index = object_index; */

    /* /1* status = nhop_group_member_to_oid(group_ecmp_id, nhop_ecmp_id, group_member_oid); *1/ */
    /* if (SAI_ERR(status)) { */
    /*     return status; */
    /* } */

    /* status = mlnx_next_hop_bulk_map_add(nh_bulk_data, &bulk_pair); */
    /* if (SAI_ERR(status)) { */
    /*     SX_LOG_ERR("Failed to add next hop %lx to next hop group %lx\n", next_hop_attr->oid, group_attr->oid); */
    /*     return status; */
    /* } */

    return SAI_STATUS_NOT_SUPPORTED;
}

static sai_status_t mlnx_next_hop_bulk_sx_nh_add(_In_ sx_ecmp_id_t               sx_ecmp_id,
                                                 _In_ const mlnx_nh_bulk_data_t *nh_list,
                                                 _Out_ sai_status_t             *object_statuses)
{
    sx_status_t   sx_status;
    sx_next_hop_t sx_next_hops[ECMP_MAX_PATHS];
    uint32_t      nh_added, next_hop_count = ECMP_MAX_PATHS;
    bool          failure = false;

    assert(nh_list);
    assert(object_statuses);

    memset(sx_next_hops, 0, sizeof(sx_next_hops));

    nh_added = 0;

    sx_status = sx_api_router_ecmp_get(gh_sdk, sx_ecmp_id, sx_next_hops, &next_hop_count);
    if (SX_ERR(sx_status)) {
        SX_LOG_ERR("Failed to get ecmp group %d - %s.\n", sx_ecmp_id, SX_STATUS_MSG(sx_status));
        failure = true;
    }

    if (!failure) {
        for (nh_added = 0; nh_added < ECMP_MAX_PATHS; nh_added++) {
            if (nh_list[nh_added].nh_id.next_hop_data.weight == 0) {
                break;
            }

            if (next_hop_count + 1 > ECMP_MAX_PATHS) {
                SX_LOG_ERR("Cannot add sx next hop to sx ecmp id %x - current next hop count is maximum (%u)\n",
                           sx_ecmp_id, ECMP_MAX_PATHS);
                failure = true;
                break;
            }

            object_statuses[nh_list[nh_added].object_index] = SAI_STATUS_SUCCESS;
            sx_next_hops[next_hop_count]                    = nh_list[nh_added].nh_id;
            next_hop_count++;
        }
    }

    if (nh_added > 0) {
        sx_status = sx_api_router_ecmp_set(gh_sdk, SX_ACCESS_CMD_SET, &sx_ecmp_id, sx_next_hops, &next_hop_count);
        if (SX_ERR(sx_status)) {
            SX_LOG_ERR("Failed to update sx ecmp id (%x) - %s.\n", sx_ecmp_id, SX_STATUS_MSG(sx_status));
            failure  = true;
            nh_added = 0;
        }
    }

    if (failure) {
        for (; nh_added < ECMP_MAX_PATHS; nh_added++) {
            if (nh_list[nh_added].nh_id.next_hop_data.weight == 0) {
                break;
            }

            object_statuses[nh_list[nh_added].object_index] = SAI_STATUS_FAILURE;
        }
    }

    return failure ? SAI_STATUS_FAILURE : SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_next_hop_bulk_sx_nh_del(_In_ sx_ecmp_id_t               sx_ecmp_id,
                                                 _In_ const mlnx_nh_bulk_data_t *nh_list,
                                                 _In_ bool                       stop_on_error,
                                                 _Out_ sai_status_t             *object_statuses)
{
    sai_status_t  status;
    sx_status_t   sx_status;
    sx_next_hop_t sx_next_hops[ECMP_MAX_PATHS];
    uint32_t      next_hop_count = ECMP_MAX_PATHS;
    uint32_t      ii, nh_removed, nh_index_to_remove;
    bool          failure = false, update_sdk = false;

    assert(nh_list);
    assert(object_statuses);

    memset(sx_next_hops, 0, sizeof(sx_next_hops));

    nh_removed = 0;

    sx_status = sx_api_router_ecmp_get(gh_sdk, sx_ecmp_id, sx_next_hops, &next_hop_count);
    if (SX_ERR(sx_status)) {
        SX_LOG_ERR("Failed to get ecmp group %d - %s.\n", sx_ecmp_id, SX_STATUS_MSG(sx_status));
        failure = true;
    }

    if (!failure) {
        for (nh_removed = 0; nh_removed < ECMP_MAX_PATHS; nh_removed++) {
            if (nh_list[nh_removed].nh_id.next_hop_data.weight == 0) {
                break;
            }

            status = mlnx_sdk_nhop_find_in_list(sx_next_hops,
                                                next_hop_count,
                                                &nh_list[nh_removed].nh_id,
                                                &nh_index_to_remove);
            object_statuses[nh_list[nh_removed].object_index] = status;
            if (SAI_ERR(status)) {
                SX_LOG_ERR("Failed to remove next hop group member at index %d\n", nh_list[nh_removed].object_index);
                failure = true;
                if (stop_on_error) {
                    break;
                } else {
                    continue;
                }
            }

            if (next_hop_count == 0) {
                SX_LOG_ERR("Failed to update next_hop_count - underflow\n");
                return SAI_STATUS_FAILURE;
            }

            sx_next_hops[nh_index_to_remove] = sx_next_hops[next_hop_count - 1];
            next_hop_count--;

            update_sdk = true;
        }
    }

    if (update_sdk) {
        sx_status = sx_api_router_ecmp_set(gh_sdk, SX_ACCESS_CMD_SET, &sx_ecmp_id, sx_next_hops, &next_hop_count);
        if (SX_ERR(sx_status)) {
            SX_LOG_ERR("Failed to update sx ecmp id (%x) - %s.\n", sx_ecmp_id, SX_STATUS_MSG(sx_status));
            failure = true;
            for (ii = 0; ii < nh_removed; ii++) {
                object_statuses[nh_list[ii].object_index] = sdk_to_sai(sx_status);
            }
        }
    }

    return failure ? SAI_STATUS_FAILURE : SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_next_hop_bulk_sx_update(_In_ sai_common_api_t           api,
                                                 _In_ sx_ecmp_id_t               sx_ecmp_id,
                                                 _In_ const mlnx_nh_bulk_data_t *nh_list,
                                                 _In_ bool                       stop_on_error,
                                                 _Out_ sai_status_t             *object_statuses)
{
    assert((api == SAI_COMMON_API_BULK_CREATE) || (api == SAI_COMMON_API_BULK_REMOVE));

    if (api == SAI_COMMON_API_BULK_CREATE) {
        return mlnx_next_hop_bulk_sx_nh_add(sx_ecmp_id, nh_list, object_statuses);
    } else { /* SAI_COMMON_API_BULK_REMOVE */
        return mlnx_next_hop_bulk_sx_nh_del(sx_ecmp_id, nh_list, stop_on_error, object_statuses);
    }
}

static int mlnx_next_hop_bulk_data_sort_fn(_In_ const void *nha, _In_ const void  *nhb)
{
    const mlnx_nh_bulk_pair_t *pair_a, *pair_b;

    assert(nha && nhb);

    pair_a = nha;
    pair_b = nhb;

    return pair_b->group_id - pair_a->group_id;
}

static sai_status_t mlnx_next_hop_bulk_apply(_In_ mlnx_nh_bulk_pair_list_t *nh_bulk_data,
                                             _Out_ sai_status_t            *object_statuses,
                                             _In_ bool                      stop_on_error,
                                             _In_ sai_common_api_t          api)
{
    sai_status_t         status;
    sx_ecmp_id_t         sx_ecmp_id;
    mlnx_nh_bulk_data_t *nh_list = NULL;
    uint32_t             nh_to_update, ii;
    bool                 failure = false;

    assert((api == SAI_COMMON_API_BULK_CREATE) || (api == SAI_COMMON_API_BULK_REMOVE));

    qsort(nh_bulk_data->pairs, nh_bulk_data->count, sizeof(mlnx_nh_bulk_pair_t), mlnx_next_hop_bulk_data_sort_fn);

    nh_list = calloc(ECMP_MAX_PATHS, sizeof(mlnx_nh_bulk_data_t));
    if (!nh_list) {
        return SAI_STATUS_NO_MEMORY;
    }

    sx_ecmp_id = nh_bulk_data->pairs[0].group_id;

    nh_list[0]   = nh_bulk_data->pairs[0].nh;
    nh_to_update = 1;

    for (ii = 1; ii < nh_bulk_data->count; ii++) {
        if (nh_bulk_data->pairs[ii].group_id != sx_ecmp_id) {
            status = mlnx_next_hop_bulk_sx_update(api, sx_ecmp_id, nh_list, stop_on_error, object_statuses);
            if (SAI_ERR(status)) {
                failure = true;
                if (stop_on_error) {
                    goto out;
                }
            }

            memset(nh_list, 0, sizeof(mlnx_nh_bulk_data_t) * ECMP_MAX_PATHS);
            nh_to_update = 0;
        }

        nh_list[nh_to_update] = nh_bulk_data->pairs[ii].nh;
        nh_to_update++;

        sx_ecmp_id = nh_bulk_data->pairs[ii].group_id;
    }

    status = mlnx_next_hop_bulk_sx_update(api, sx_ecmp_id, nh_list, stop_on_error, object_statuses);
    if (SAI_ERR(status)) {
        failure = true;
    }

out:
    free(nh_list);
    return failure ? SAI_STATUS_FAILURE : SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_next_hop_bulk_remove_member_add(_In_ mlnx_nh_bulk_pair_list_t *nh_bulk_data,
                                                         _In_ sai_object_id_t           group_member_oid,
                                                         _In_ uint32_t                  object_index)
{
    sai_status_t        status;
    sx_ecmp_id_t        group_ecmp_id, nh_ecmp_id;
    mlnx_nh_bulk_pair_t nh_pair;

    assert(nh_bulk_data);

    status = nhop_group_member_parse_oid(group_member_oid, &group_ecmp_id, &nh_ecmp_id);
    if (SAI_ERR(status)) {
        return status;
    }

    status = mlnx_sdk_nhop_by_ecmp_id_get(nh_ecmp_id, &nh_pair.nh.nh_id);
    if (SAI_ERR(status)) {
        return status;
    }

    nh_pair.group_id        = group_ecmp_id;
    nh_pair.nh.object_index = object_index;

    status = mlnx_next_hop_bulk_map_add(nh_bulk_data, &nh_pair);
    if (SAI_ERR(status)) {
        SX_LOG_ERR("Failed to remove next hop group member %lx\n", group_member_oid);
        return status;
    }

    return SAI_STATUS_SUCCESS;
}

static sai_status_t mlnx_next_hop_bulk_impl(_In_ sai_object_id_t          switch_id,
                                            _In_ uint32_t                 object_count,
                                            _In_ const uint32_t          *attr_count,
                                            _In_ const sai_attribute_t  **attr_list,
                                            _In_ sai_bulk_op_error_mode_t mode,
                                            _Inout_ sai_object_id_t      *object_id,
                                            _Out_ sai_status_t           *object_statuses,
                                            _In_ sai_common_api_t         api)
{
    sai_status_t             status;
    mlnx_nh_bulk_pair_list_t nh_bulk_data = (mlnx_nh_bulk_pair_list_t) {.pairs = NULL, .count = 0};
    uint32_t                 ii;
    bool                     stop_on_error, failure = false;

    assert((api == SAI_COMMON_API_BULK_CREATE) || (api == SAI_COMMON_API_BULK_REMOVE));
    assert((api != SAI_COMMON_API_BULK_REMOVE) || (!attr_count && !attr_list));

    SX_LOG_ENTER();

    status = mlnx_bulk_attrs_validate(object_count, attr_count, attr_list, NULL,
                                      NULL, mode, object_statuses, api, &stop_on_error);
    if (SAI_ERR(status)) {
        return status;
    }

    if (!object_id) {
        SX_LOG_ERR("object_id is NULL");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (stop_on_error) {
        for (ii = 0; ii < object_count; ii++) {
            object_statuses[ii] = SAI_STATUS_NOT_EXECUTED;
        }
    }

    status = mlnx_next_hop_bulk_init(&nh_bulk_data, object_count);
    if (SAI_ERR(status)) {
        return SAI_STATUS_FAILURE;
    }

    sai_db_write_lock();

    for (ii = 0; ii < object_count; ii++) {
        if (api == SAI_COMMON_API_BULK_CREATE) {
            status = mlnx_next_hop_bulk_create_member_add(&nh_bulk_data, attr_count[ii],
                                                          attr_list[ii], ii, &object_id[ii]);
        } else { /* SAI_COMMON_API_BULK_REMOVE */
            status = mlnx_next_hop_bulk_remove_member_add(&nh_bulk_data, object_id[ii], ii);
        }

        if (SAI_ERR(status)) {
            object_statuses[ii] = status;
            failure             = true;
            if (stop_on_error) {
                break;
            } else {
                continue;
            }
        }
    }

    status = mlnx_next_hop_bulk_apply(&nh_bulk_data, object_statuses, stop_on_error, api);
    if (SAI_ERR(status)) {
        failure = true;
        goto out;
    }

out:
    sai_db_unlock();
    mlnx_next_hop_bulk_deinit(&nh_bulk_data);
    mlnx_bulk_statuses_print("Next hop group membes", object_statuses, object_count, api);
    SX_LOG_EXIT();
    return failure ? SAI_STATUS_FAILURE : SAI_STATUS_SUCCESS;
}

/**
 * @brief Bulk next hop group members creation.
 *
 * @param[in] switch_id SAI Switch object id
 * @param[in] object_count Number of objects to create
 * @param[in] attr_count List of attr_count. Caller passes the number
 *    of attribute for each object to create.
 * @param[in] attr_list List of attributes for every object.
 * @param[in] mode Bulk operation error handling mode.
 *
 * @param[out] object_id List of object ids returned
 * @param[out] object_statuses List of status for every object. Caller needs to allocate the buffer.
 *
 * @return #SAI_STATUS_SUCCESS on success when all objects are created or #SAI_STATUS_FAILURE when
 * any of the objects fails to create. When there is failure, Caller is expected to go through the
 * list of returned statuses to find out which fails and which succeeds.
 */
sai_status_t mlnx_create_next_hop_group_members(_In_ sai_object_id_t          switch_id,
                                                _In_ uint32_t                 object_count,
                                                _In_ const uint32_t          *attr_count,
                                                _In_ const sai_attribute_t  **attr_list,
                                                _In_ sai_bulk_op_error_mode_t mode,
                                                _Out_ sai_object_id_t        *object_id,
                                                _Out_ sai_status_t           *object_statuses)
{
    return mlnx_next_hop_bulk_impl(switch_id, object_count, attr_count, attr_list, mode,
                                   object_id, object_statuses, SAI_COMMON_API_BULK_CREATE);
}

/**
 * @brief Bulk next hop group members removal.
 *
 * @param[in] object_count Number of objects to create
 * @param[in] object_id List of object ids
 * @param[in] mode Bulk operation error handling mode.
 * @param[out] object_statuses List of status for every object. Caller needs to allocate the buffer.
 *
 * @return #SAI_STATUS_SUCCESS on success when all objects are removed or #SAI_STATUS_FAILURE when
 * any of the objects fails to remove. When there is failure, Caller is expected to go through the
 * list of returned statuses to find out which fails and which succeeds.
 */
sai_status_t mlnx_remove_next_hop_group_members(_In_ uint32_t                 object_count,
                                                _In_ const sai_object_id_t   *object_id,
                                                _In_ sai_bulk_op_error_mode_t mode,
                                                _Out_ sai_status_t           *object_statuses)
{
    return mlnx_next_hop_bulk_impl(SAI_NULL_OBJECT_ID, object_count, NULL, NULL, mode, (sai_object_id_t*)object_id,
                                   object_statuses, SAI_COMMON_API_BULK_REMOVE);
}

static void next_hop_group_member_key_to_str(_In_ sai_object_id_t group_member_id, _Out_ char *key_str)
{
    sx_ecmp_id_t sx_group_id;
    sx_ecmp_id_t sx_nhop_id;
    sai_status_t status;

    status = nhop_group_member_parse_oid(group_member_id, &sx_group_id, &sx_nhop_id);
    if (SAI_ERR(status)) {
        snprintf(key_str, MAX_KEY_STR_LEN, "invalid next hop group id");
    } else {
        snprintf(key_str, MAX_KEY_STR_LEN, "next hop group member id %u:%u", sx_group_id, sx_nhop_id);
    }
}

/**
 * @brief Set Next Hop Group attribute
 *
 * @param[in] sai_object_id_t - next_hop_group_member_id
 * @param[in] attr - attribute
 *
 * @return #SAI_STATUS_SUCCESS on success Failure status code on error
 */
static sai_status_t mlnx_set_next_hop_group_member_attribute(_In_ sai_object_id_t        next_hop_group_member_id,
                                                             _In_ const sai_attribute_t *attr)
{
    const sai_object_key_t key = { .key.object_id = next_hop_group_member_id };
    char                   key_str[MAX_KEY_STR_LEN];

    SX_LOG_ENTER();

    next_hop_group_member_key_to_str(next_hop_group_member_id, key_str);
    return sai_set_attribute(&key,
                             key_str,
                             SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER,
                             next_hop_group_member_vendor_attribs,
                             attr);
}

/**
 * @brief Get Next Hop Group attribute
 *
 * @param[in] sai_object_id_t - next_hop_group_member_id
 * @param[in] attr_count - number of attributes
 * @param[inout] attr_list - array of attributes
 *
 * @return SAI_STATUS_SUCCESS on success Failure status code on error
 */
static sai_status_t mlnx_get_next_hop_group_member_attribute(_In_ sai_object_id_t     next_hop_group_member_id,
                                                             _In_ uint32_t            attr_count,
                                                             _Inout_ sai_attribute_t *attr_list)
{
    const sai_object_key_t key = { .key.object_id = next_hop_group_member_id };
    char                   key_str[MAX_KEY_STR_LEN];

    SX_LOG_ENTER();

    next_hop_group_member_key_to_str(next_hop_group_member_id, key_str);
    return sai_get_attributes(&key,
                              key_str,
                              SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER,
                              next_hop_group_member_vendor_attribs,
                              attr_count,
                              attr_list);
}

const sai_next_hop_group_api_t mlnx_next_hop_group_api = {
    mlnx_create_next_hop_group,
    mlnx_remove_next_hop_group,
    mlnx_set_next_hop_group_attribute,
    mlnx_get_next_hop_group_attribute,
    mlnx_create_next_hop_group_member,
    mlnx_remove_next_hop_group_member,
    mlnx_set_next_hop_group_member_attribute,
    mlnx_get_next_hop_group_member_attribute,
    mlnx_create_next_hop_group_members,
    mlnx_remove_next_hop_group_members,
};
