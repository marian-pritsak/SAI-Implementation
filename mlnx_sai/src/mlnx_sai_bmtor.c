//#include <sai_ext.h>
#include <mlnx_sai.h>
#include <fx_base_api.h>
#include <flextrum_types.h>

#undef  __MODULE__
#define __MODULE__ BMTOR

static sx_verbosity_level_t LOG_VAR_NAME(__MODULE__) = SX_VERBOSITY_LEVEL_WARNING;

//service_method_table_t g_mlnx_services;
static bool g_initialized = false;
fx_handle_t fx_handle;

sai_status_t sai_ext_api_initialize();

typedef enum _sai_ext_object_type_t {
    SAI_EXT_OBJECT_TYPE_NULL = SAI_OBJECT_TYPE_MAX,
    __SAI_EXT_OBJECT_TYPE_ENUM__
} sai_ext_object_type_t;

typedef struct _mlnx_sai_ext_object_id_t {
    sai_ext_object_type_t type;
    uint32_t offset;
} mlnx_sai_ext_object_id_t;

void mlnx_to_sai_ext_object_id(sai_object_id_t *entry_id, uint32_t offset, sai_ext_object_type_t type)
{
    mlnx_sai_ext_object_id_t mlnx_object_id = {.type = type, .offset = offset};
    memcpy(entry_id, &mlnx_object_id, sizeof(mlnx_sai_ext_object_id_t));
}

sai_status_t sai_ext_oid_to_mlnx_offset(sai_object_id_t object_id, uint32_t *offset, sai_ext_object_type_t expected_type)
{
    mlnx_sai_ext_object_id_t *mlnx_object_id = (mlnx_sai_ext_object_id_t *)&object_id;

    if (NULL == offset)
    {
        MLNX_SAI_LOG_ERR("NULL offset value\n");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (expected_type != mlnx_object_id->type)
    {
        MLNX_SAI_LOG_ERR("Expected object %d got %d\n", expected_type, mlnx_object_id->type);
        return SAI_STATUS_INVALID_PARAMETER;
    }
    *offset = mlnx_object_id->offset;
    return SAI_STATUS_SUCCESS;
}

fx_action_id_t get_bitmap_classification_fx_action(
    _In_ sai_table_bitmap_classification_entry_action_t action)
{
    switch (action)
    {
        case SAI_TABLE_BITMAP_CLASSIFICATION_ENTRY_ACTION_SET_METADATA:
            return CONTROL_IN_RIF_SET_METADATA_ID;
        case SAI_TABLE_BITMAP_CLASSIFICATION_ENTRY_ACTION_NOACTION:
            return NOACTION_ID;
        default:
            return -1;
    }

    return -1;
}

sai_status_t mlnx_create_table_bitmap_classification_entry(
    _Out_ sai_object_id_t *entry_id,
    _In_ sai_object_id_t switch_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    printf("mlnx_create_table_bitmap_classification_entry\n");
    sai_status_t sai_status;
    uint32_t attr_idx;
    const sai_attribute_value_t *attr;

    // Lazy initialization
    if (SAI_STATUS_SUCCESS != (sai_status = sai_ext_api_initialize()))
    {
        MLNX_SAI_LOG_ERR("Failure in call to sai_ext_api_initialize\n");
        return sai_status;
    }

    fx_key_t bitmap_classification_keys[1];
    fx_param_t bitmap_classification_params[1];
    fx_key_list_t bitmap_classification_key_list;
    bitmap_classification_key_list.keys = bitmap_classification_keys;
    fx_param_list_t bitmap_classification_param_list;
    bitmap_classification_param_list.params = bitmap_classification_params;
    int keys_idx = 0;
    int params_idx = 0;
    fx_action_id_t flextrum_action;
        sai_table_bitmap_classification_entry_action_t bitmap_classification_action;
        if (SAI_STATUS_SUCCESS ==
            (sai_status =
                 find_attrib_in_list(attr_count, attr_list, SAI_TABLE_BITMAP_CLASSIFICATION_ENTRY_ATTR_ACTION, &attr, &attr_idx)))
        {
            memcpy(&bitmap_classification_action, &attr->s32, sizeof(sai_table_bitmap_classification_entry_action_t));
        }
        else
        {
            MLNX_SAI_LOG_ERR("Did not receive mandatory SAI_TABLE_BITMAP_CLASSIFICATION_ENTRY_ATTR_ACTION attribute\n");
            return SAI_STATUS_INVALID_PARAMETER;
        }

    flextrum_action = get_bitmap_classification_fx_action(bitmap_classification_action);

    sx_acl_rule_offset_t bitmap_classification_priority = 0;
        sx_router_interface_t bitmap_classification_router_interface_key;
        if (SAI_STATUS_SUCCESS ==
            (sai_status =
                 find_attrib_in_list(attr_count, attr_list, SAI_TABLE_BITMAP_CLASSIFICATION_ENTRY_ATTR_ROUTER_INTERFACE_KEY, &attr, &attr_idx)))
        {
            if (SAI_STATUS_SUCCESS !=
                (sai_status = mlnx_rif_oid_to_sdk_rif_id(attr->oid, &bitmap_classification_router_interface_key)))
            {
                MLNX_SAI_LOG_ERR("Failed to get sx handle for router_interface_key\n");
                return SAI_STATUS_INVALID_ATTR_VALUE_0 + attr_idx;
            }
        bitmap_classification_keys[keys_idx].key.data = (uint8_t *)&bitmap_classification_router_interface_key;
        bitmap_classification_keys[keys_idx].key.len  = sizeof(bitmap_classification_router_interface_key);
        keys_idx++;
        }
        else
        {
            MLNX_SAI_LOG_ERR("Did not receive mandatory SAI_TABLE_BITMAP_CLASSIFICATION_ENTRY_ATTR_ROUTER_INTERFACE_KEY attribute\n");
            return SAI_STATUS_INVALID_PARAMETER;
        }

        uint32_t bitmap_classification_in_rif_metadata;
        if (flextrum_action == CONTROL_IN_RIF_SET_METADATA_ID)
        {
        if (SAI_STATUS_SUCCESS ==
            (sai_status =
                 find_attrib_in_list(attr_count, attr_list, SAI_TABLE_BITMAP_CLASSIFICATION_ENTRY_ATTR_IN_RIF_METADATA, &attr, &attr_idx)))
        {
            memcpy(&bitmap_classification_in_rif_metadata, &attr->u32, sizeof(uint32_t));
        bitmap_classification_params[params_idx].data = (uint8_t *)&bitmap_classification_in_rif_metadata;
        bitmap_classification_params[params_idx].len  = sizeof(bitmap_classification_in_rif_metadata);
        params_idx++;
        }
        else
        {
            MLNX_SAI_LOG_ERR("Did not receive mandatory SAI_TABLE_BITMAP_CLASSIFICATION_ENTRY_ATTR_IN_RIF_METADATA attribute\n");
            return SAI_STATUS_INVALID_PARAMETER;
        }
        }

    bitmap_classification_key_list.len = keys_idx;
    bitmap_classification_param_list.len = params_idx;
    if (fx_table_entry_add(fx_handle, CONTROL_IN_RIF_TABLE_BITMAP_CLASSIFICATION_ID, flextrum_action, bitmap_classification_key_list, bitmap_classification_param_list, &bitmap_classification_priority))
    {
        MLNX_SAI_LOG_ERR("Failure in insertion of bitmap_classification entry\n");
        return SAI_STATUS_FAILURE;
    }
    mlnx_to_sai_ext_object_id(entry_id, bitmap_classification_priority, SAI_OBJECT_TYPE_TABLE_BITMAP_CLASSIFICATION_ENTRY);
    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_remove_table_bitmap_classification_entry(
    _In_ sai_object_id_t entry_id)
{
    printf("mlnx_remove_table_bitmap_classification_entry\n");
    sai_status_t status;
    uint32_t bitmap_classification_offset;
    if (SAI_STATUS_SUCCESS != (status = sai_ext_oid_to_mlnx_offset(entry_id, &bitmap_classification_offset, SAI_OBJECT_TYPE_TABLE_BITMAP_CLASSIFICATION_ENTRY)))
    {
        MLNX_SAI_LOG_ERR("Failure in extracting offest from bitmap_classification entry object id 0x%lx " PRIx64 "\n", entry_id);
        return status;
    }

    if (fx_table_entry_remove(fx_handle, CONTROL_IN_RIF_TABLE_BITMAP_CLASSIFICATION_ID, bitmap_classification_offset))
    {
        MLNX_SAI_LOG_ERR("Failure in removal of table_bitmap_classification entry at offset %d\n", bitmap_classification_offset);
        return SAI_STATUS_FAILURE;
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_set_table_bitmap_classification_entry_attribute(
    _In_ sai_object_id_t entry_id,
    _In_ const sai_attribute_t *attr)
{
    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t mlnx_get_table_bitmap_classification_entry_attribute(
    _In_ sai_object_id_t entry_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    return SAI_STATUS_NOT_IMPLEMENTED;
}
fx_action_id_t get_bitmap_router_fx_action(
    _In_ sai_table_bitmap_router_entry_action_t action)
{
    switch (action)
    {
        case SAI_TABLE_BITMAP_ROUTER_ENTRY_ACTION_TO_NEXTHOP:
            return CONTROL_IN_RIF_TO_NEXTHOP_ID;
        case SAI_TABLE_BITMAP_ROUTER_ENTRY_ACTION_TO_LOCAL:
            return CONTROL_IN_RIF_TO_LOCAL_ID;
        case SAI_TABLE_BITMAP_ROUTER_ENTRY_ACTION_TO_CPU:
            return CONTROL_IN_RIF_TO_CPU_ID;
        case SAI_TABLE_BITMAP_ROUTER_ENTRY_ACTION_DROP:
            return CONTROL_IN_RIF_DROP_ID;
        case SAI_TABLE_BITMAP_ROUTER_ENTRY_ACTION_NOACTION:
            return NOACTION_ID;
        default:
            return -1;
    }

    return -1;
}

sai_status_t mlnx_create_table_bitmap_router_entry(
    _Out_ sai_object_id_t *entry_id,
    _In_ sai_object_id_t switch_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    printf("mlnx_create_table_bitmap_router_entry\n");
    sai_status_t sai_status;
    uint32_t attr_idx;
    const sai_attribute_value_t *attr;

    // Lazy initialization
    if (SAI_STATUS_SUCCESS != (sai_status = sai_ext_api_initialize()))
    {
        MLNX_SAI_LOG_ERR("Failure in call to sai_ext_api_initialize\n");
        return sai_status;
    }

    fx_key_t bitmap_router_keys[2];
    fx_param_t bitmap_router_params[3];
    fx_key_list_t bitmap_router_key_list;
    bitmap_router_key_list.keys = bitmap_router_keys;
    fx_param_list_t bitmap_router_param_list;
    bitmap_router_param_list.params = bitmap_router_params;
    int keys_idx = 0;
    int params_idx = 0;
    fx_action_id_t flextrum_action;
        sai_table_bitmap_router_entry_action_t bitmap_router_action;
        if (SAI_STATUS_SUCCESS ==
            (sai_status =
                 find_attrib_in_list(attr_count, attr_list, SAI_TABLE_BITMAP_ROUTER_ENTRY_ATTR_ACTION, &attr, &attr_idx)))
        {
            memcpy(&bitmap_router_action, &attr->s32, sizeof(sai_table_bitmap_router_entry_action_t));
        }
        else
        {
            MLNX_SAI_LOG_ERR("Did not receive mandatory SAI_TABLE_BITMAP_ROUTER_ENTRY_ATTR_ACTION attribute\n");
            return SAI_STATUS_INVALID_PARAMETER;
        }

    flextrum_action = get_bitmap_router_fx_action(bitmap_router_action);

        sx_acl_rule_offset_t bitmap_router_priority;
        if (SAI_STATUS_SUCCESS ==
            (sai_status =
                 find_attrib_in_list(attr_count, attr_list, SAI_TABLE_BITMAP_ROUTER_ENTRY_ATTR_PRIORITY, &attr, &attr_idx)))
        {
            memcpy(&bitmap_router_priority, &attr->u32, sizeof(sx_acl_rule_offset_t));
        }
        else
        {
            MLNX_SAI_LOG_ERR("Did not receive mandatory SAI_TABLE_BITMAP_ROUTER_ENTRY_ATTR_PRIORITY attribute\n");
            return SAI_STATUS_INVALID_PARAMETER;
        }

        uint32_t bitmap_router_in_rif_metadata_key;
        if (SAI_STATUS_SUCCESS ==
            (sai_status =
                 find_attrib_in_list(attr_count, attr_list, SAI_TABLE_BITMAP_ROUTER_ENTRY_ATTR_IN_RIF_METADATA_KEY, &attr, &attr_idx)))
        {
            memcpy(&bitmap_router_in_rif_metadata_key, &attr->u32, sizeof(uint32_t));
        bitmap_router_keys[keys_idx].key.data = (uint8_t *)&bitmap_router_in_rif_metadata_key;
        bitmap_router_keys[keys_idx].key.len  = sizeof(bitmap_router_in_rif_metadata_key);
        }
        else
        {
            MLNX_SAI_LOG_ERR("Did not receive mandatory SAI_TABLE_BITMAP_ROUTER_ENTRY_ATTR_IN_RIF_METADATA_KEY attribute\n");
            return SAI_STATUS_INVALID_PARAMETER;
        }

        uint32_t bitmap_router_in_rif_metadata_mask;
        if (SAI_STATUS_SUCCESS ==
            (sai_status =
                 find_attrib_in_list(attr_count, attr_list, SAI_TABLE_BITMAP_ROUTER_ENTRY_ATTR_IN_RIF_METADATA_MASK, &attr, &attr_idx)))
        {
            memcpy(&bitmap_router_in_rif_metadata_mask, &attr->u32, sizeof(uint32_t));
        bitmap_router_keys[keys_idx].mask.data = (uint8_t *)&bitmap_router_in_rif_metadata_mask;
        bitmap_router_keys[keys_idx].mask.len  = sizeof(bitmap_router_in_rif_metadata_mask);
        keys_idx++;
        }
        else
        {
            MLNX_SAI_LOG_ERR("Did not receive mandatory SAI_TABLE_BITMAP_ROUTER_ENTRY_ATTR_IN_RIF_METADATA_MASK attribute\n");
            return SAI_STATUS_INVALID_PARAMETER;
        }

        uint32_t bitmap_router_dst_ip_key;
        uint32_t bitmap_router_dst_ip_key_mask;
        if (SAI_STATUS_SUCCESS ==
            (sai_status =
                 find_attrib_in_list(attr_count, attr_list, SAI_TABLE_BITMAP_ROUTER_ENTRY_ATTR_DST_IP_KEY, &attr, &attr_idx)))
        {
            memcpy(&bitmap_router_dst_ip_key, &attr->ipprefix.addr.ip4, sizeof(uint32_t));
            bitmap_router_dst_ip_key = htonl(bitmap_router_dst_ip_key);
            memcpy(&bitmap_router_dst_ip_key_mask, &attr->ipprefix.mask.ip4, sizeof(uint32_t));
            bitmap_router_dst_ip_key_mask = htonl(bitmap_router_dst_ip_key_mask);
        bitmap_router_keys[keys_idx].key.data = (uint8_t *)&bitmap_router_dst_ip_key;
        bitmap_router_keys[keys_idx].key.len  = sizeof(bitmap_router_dst_ip_key);
        bitmap_router_keys[keys_idx].mask.data = (uint8_t *)&bitmap_router_dst_ip_key_mask;
        bitmap_router_keys[keys_idx].mask.len  = sizeof(bitmap_router_dst_ip_key_mask);
        keys_idx++;
        }
        else
        {
            MLNX_SAI_LOG_ERR("Did not receive mandatory SAI_TABLE_BITMAP_ROUTER_ENTRY_ATTR_DST_IP_KEY attribute\n");
            return SAI_STATUS_INVALID_PARAMETER;
        }

        sx_ecmp_id_t bitmap_router_next_hop;
        if (flextrum_action == CONTROL_IN_RIF_TO_NEXTHOP_ID)
        {
        if (SAI_STATUS_SUCCESS ==
            (sai_status =
                 find_attrib_in_list(attr_count, attr_list, SAI_TABLE_BITMAP_ROUTER_ENTRY_ATTR_NEXT_HOP, &attr, &attr_idx)))
        {
            if (SAI_STATUS_SUCCESS !=
                (sai_status = mlnx_object_to_type(attr->oid, SAI_OBJECT_TYPE_NEXT_HOP, (uint32_t *)&bitmap_router_next_hop, NULL)))
            {
                MLNX_SAI_LOG_ERR("Failed to get sx handle for next_hop\n");
                return SAI_STATUS_INVALID_ATTR_VALUE_0 + attr_idx;
            }
        bitmap_router_params[params_idx].data = (uint8_t *)&bitmap_router_next_hop;
        bitmap_router_params[params_idx].len  = sizeof(bitmap_router_next_hop);
        params_idx++;
        }
        else
        {
            MLNX_SAI_LOG_ERR("Did not receive mandatory SAI_TABLE_BITMAP_ROUTER_ENTRY_ATTR_NEXT_HOP attribute\n");
            return SAI_STATUS_INVALID_PARAMETER;
        }
        }

        sx_router_interface_t bitmap_router_router_interface;
        if (flextrum_action == CONTROL_IN_RIF_TO_LOCAL_ID)
        {
        if (SAI_STATUS_SUCCESS ==
            (sai_status =
                 find_attrib_in_list(attr_count, attr_list, SAI_TABLE_BITMAP_ROUTER_ENTRY_ATTR_ROUTER_INTERFACE, &attr, &attr_idx)))
        {
            if (SAI_STATUS_SUCCESS !=
                (sai_status = mlnx_rif_oid_to_sdk_rif_id(attr->oid, &bitmap_router_router_interface)))
            {
                MLNX_SAI_LOG_ERR("Failed to get sx handle for router_interface\n");
                return SAI_STATUS_INVALID_ATTR_VALUE_0 + attr_idx;
            }
        bitmap_router_params[params_idx].data = (uint8_t *)&bitmap_router_router_interface;
        bitmap_router_params[params_idx].len  = sizeof(bitmap_router_router_interface);
        params_idx++;
        }
        else
        {
            MLNX_SAI_LOG_ERR("Did not receive mandatory SAI_TABLE_BITMAP_ROUTER_ENTRY_ATTR_ROUTER_INTERFACE attribute\n");
            return SAI_STATUS_INVALID_PARAMETER;
        }
        }

        sx_flex_acl_trap_action_t bitmap_router_trap_id;
        if (flextrum_action == CONTROL_IN_RIF_TO_CPU_ID)
        {
        if (SAI_STATUS_SUCCESS ==
            (sai_status =
                 find_attrib_in_list(attr_count, attr_list, SAI_TABLE_BITMAP_ROUTER_ENTRY_ATTR_TRAP_ID, &attr, &attr_idx)))
        {
            if (SAI_STATUS_SUCCESS !=
                (sai_status = mlnx_object_to_type(attr->oid, SAI_OBJECT_TYPE_HOSTIF_TRAP, (uint32_t *)&bitmap_router_trap_id, NULL)))
            {
                MLNX_SAI_LOG_ERR("Failed to get sx handle for trap_id\n");
                return SAI_STATUS_INVALID_ATTR_VALUE_0 + attr_idx;
            }
        bitmap_router_params[params_idx].data = (uint8_t *)&bitmap_router_trap_id;
        bitmap_router_params[params_idx].len  = sizeof(bitmap_router_trap_id);
        params_idx++;
        }
        else
        {
            MLNX_SAI_LOG_ERR("Did not receive mandatory SAI_TABLE_BITMAP_ROUTER_ENTRY_ATTR_TRAP_ID attribute\n");
            return SAI_STATUS_INVALID_PARAMETER;
        }
        }

    bitmap_router_key_list.len = keys_idx;
    bitmap_router_param_list.len = params_idx;
    if (fx_table_entry_add(fx_handle, CONTROL_IN_RIF_TABLE_BITMAP_ROUTER_ID, flextrum_action, bitmap_router_key_list, bitmap_router_param_list, &bitmap_router_priority))
    {
        MLNX_SAI_LOG_ERR("Failure in insertion of bitmap_router entry\n");
        return SAI_STATUS_FAILURE;
    }
    mlnx_to_sai_ext_object_id(entry_id, bitmap_router_priority, SAI_OBJECT_TYPE_TABLE_BITMAP_ROUTER_ENTRY);
    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_remove_table_bitmap_router_entry(
    _In_ sai_object_id_t entry_id)
{
    printf("mlnx_remove_table_bitmap_router_entry\n");
    sai_status_t status;
    uint32_t bitmap_router_offset;
    if (SAI_STATUS_SUCCESS != (status = sai_ext_oid_to_mlnx_offset(entry_id, &bitmap_router_offset, SAI_OBJECT_TYPE_TABLE_BITMAP_ROUTER_ENTRY)))
    {
        MLNX_SAI_LOG_ERR("Failure in extracting offest from bitmap_router entry object id 0x%lx " PRIx64 "\n", entry_id);
        return status;
    }

    if (fx_table_entry_remove(fx_handle, CONTROL_IN_RIF_TABLE_BITMAP_ROUTER_ID, bitmap_router_offset))
    {
        MLNX_SAI_LOG_ERR("Failure in removal of table_bitmap_router entry at offset %d\n", bitmap_router_offset);
        return SAI_STATUS_FAILURE;
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t mlnx_set_table_bitmap_router_entry_attribute(
    _In_ sai_object_id_t entry_id,
    _In_ const sai_attribute_t *attr)
{
    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t mlnx_get_table_bitmap_router_entry_attribute(
    _In_ sai_object_id_t entry_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    return SAI_STATUS_NOT_IMPLEMENTED;
}


const sai_bmtor_api_t mlnx_bmtor_api = {
    mlnx_create_table_bitmap_classification_entry,
    mlnx_remove_table_bitmap_classification_entry,
    mlnx_set_table_bitmap_classification_entry_attribute,
    mlnx_get_table_bitmap_classification_entry_attribute,
    NULL,
    NULL,
    NULL,
    mlnx_create_table_bitmap_router_entry,
    mlnx_remove_table_bitmap_router_entry,
    mlnx_set_table_bitmap_router_entry_attribute,
    mlnx_get_table_bitmap_router_entry_attribute,
    NULL,
    NULL,
    NULL,
};

sai_status_t sai_ext_api_rebind()
{
    sx_port_log_id_t port_list[PORT_NUM];
    sx_router_interface_t rif_list[RIF_NUM];
    uint32_t num_of_ports = 0;
    uint32_t num_of_rifs = 0;

    sx_status_t rc = fx_get_bindable_port_list(fx_handle, port_list, &num_of_ports);
    if (rc)
    {
        printf("Error - rc:%d\n", rc);
        return rc;
    }
    rc = fx_get_bindable_rif_list(fx_handle, rif_list, &num_of_rifs);
    if (rc)
    {
        printf("Error - rc:%d\n", rc);
        return rc;
    }

        rc = fx_pipe_rebind(fx_handle, FX_CONTROL_IN_RIF, (void *)rif_list, num_of_rifs);
        if (rc) {
            printf("Error - rc:%d\n", rc);
            return rc;
        }

    return SAI_STATUS_SUCCESS;
}
#define BMFLOOD

#ifdef BMFLOOD
#include <stdio.h>
#include <sx/sdk/sx_api_rm.h>
#include <sx/sxd/sxd_access_register_init.h>
#include <sx/sxd/sxd_access_register.h>
#include <sx/sxd/sxd_dpt.h>
#include <errno.h>

#define BRIDGE_START 4097
#define NUM_BRIDGES 900

#define SX_PORT_PHY_ID_MASK  (0x0000FF00)
#define SX_PORT_PHY_ID_ISO(id)  ((id) & (SX_PORT_PHY_ID_MASK))
#define SX_PORT_PHY_ID_OFFS  (8)
#define SX_PORT_PHY_ID_GET(id)  (SX_PORT_PHY_ID_ISO(id) >> SX_PORT_PHY_ID_OFFS)
#define SPECTRUM_PORT_EXT_NUM_MAX (64)
#define SX_ROUTER_PHY_PORT (SPECTRUM_PORT_EXT_NUM_MAX + 2)
#define SX_FDB_ROUTER_PORT(dev_id) ((sx_port_log_id_t)((dev_id << 16) | (SX_ROUTER_PHY_PORT << 8)))


static void log_cb(sx_log_severity_t severity, const char *module_name, char *msg)
{
}

int bmflood() {
    sxd_status_t              sxd_ret = SXD_STATUS_SUCCESS;
    sxd_handle                sxd_handle   = 0;
    uint32_t                  dev_num      = 1;
    char                      dev_name[MAX_NAME_LEN];
    char                     *dev_names[1] = { dev_name };
    struct ku_sftr_reg        sftr_reg_data;
    sxd_reg_meta_t     sftr_reg_meta;
    sx_port_phy_id_t          port_phy_id = 0;

    memset(&sftr_reg_meta, 0, sizeof(sftr_reg_meta));
    memset(&sftr_reg_data, 0, sizeof(sftr_reg_data));

    sxd_ret = sxd_access_reg_init(0, log_cb, 0);
    if (SXD_CHECK_FAIL(sxd_ret)) {
        printf("Failed to init access reg - %s.\n", SXD_STATUS_MSG(sxd_ret));
        return 1;
    }

    /* get device list from the devices directory */
    sxd_ret = sxd_get_dev_list(dev_names, &dev_num);
    if (SXD_CHECK_FAIL(sxd_ret)) {
        printf("sxd_get_dev_list error %s.\n", SXD_STATUS_MSG(sxd_ret));
        return 1;
    }

    /* open the first device */
    sxd_ret = sxd_open_device(dev_name, &sxd_handle);
    if (SXD_CHECK_FAIL(sxd_ret)) {
        printf("sxd_open_device error %s.\n", SXD_STATUS_MSG(sxd_ret));
        return 1;
    }

    printf("Init done.\n");

    sftr_reg_meta.swid = 0;
    sftr_reg_meta.dev_id = 1;
    sftr_reg_meta.access_cmd = SXD_ACCESS_CMD_ADD;

    int i;
    for (i = BRIDGE_START; i < BRIDGE_START + NUM_BRIDGES; i++) {

        sftr_reg_data.swid = 0;
        sftr_reg_data.index = i - 4096;
        sftr_reg_data.range = 0;
        sftr_reg_data.flood_table = 1;
        sftr_reg_data.table_type = SFGC_TABLE_TYPE_FID;
        port_phy_id = SX_PORT_PHY_ID_GET(SX_FDB_ROUTER_PORT(0));
        sftr_reg_data.mask_bitmap[port_phy_id] = 1;
        sftr_reg_data.ports_bitmap[port_phy_id] = 1;

        sxd_ret = sxd_access_reg_sftr(&sftr_reg_data, &sftr_reg_meta, 1, NULL, NULL);
        if (SXD_CHECK_FAIL(sxd_ret)) {
            MLNX_SAI_LOG_ERR("sxd_access_reg_sftr error %s.\n", SXD_STATUS_MSG(sxd_ret));
            return 1;
        }
    }

    sxd_ret = sxd_close_device(sxd_handle);
    if (SXD_CHECK_FAIL(sxd_ret)) {
        MLNX_SAI_LOG_ERR("sxd_close_device error: %s\n", SXD_STATUS_MSG(sxd_ret));
        return 1;
    }


    return 0;
}
#endif

sai_status_t sai_ext_api_initialize()
{
#ifdef BMFLOOD
bmflood();
#endif
#ifdef CONFIG_SYSLOG
    if (!g_initialized)
    {
        openlog("SAI", 0, LOG_USER);
    }
#endif
    if (g_initialized)
    {
        return sai_ext_api_rebind();
    }

    sx_port_log_id_t port_list[PORT_NUM];
    sx_router_interface_t rif_list[RIF_NUM];
    uint32_t num_of_ports = 0;
    uint32_t num_of_rifs = 0;
    fx_init(&fx_handle);
    fx_extern_init(fx_handle);
    sx_status_t rc = fx_get_bindable_port_list(fx_handle, port_list, &num_of_ports);
    if (rc)
    {
        printf("Error - rc:%d\n", rc);
        return rc;
    }
    rc = fx_get_bindable_rif_list(fx_handle, rif_list, &num_of_rifs);
    if (rc)
    {
        printf("Error - rc:%d\n", rc);
        return rc;
    }
        rc = fx_pipe_create(fx_handle, FX_CONTROL_IN_RIF, (void *)rif_list, num_of_rifs);
        if (rc) {
            printf("Error - rc:%d\n", rc);
            return rc;
        }
    g_initialized = true;
    return SAI_STATUS_SUCCESS;
}

sai_status_t sai_ext_api_uninitialize()
{
    g_initialized = false;
    sx_port_log_id_t port_list[PORT_NUM];
    sx_router_interface_t rif_list[RIF_NUM];
    uint32_t num_of_ports = 0;
    uint32_t num_of_rifs = 0;
    sx_status_t rc = fx_get_bindable_port_list(fx_handle, port_list, &num_of_ports);
    if (rc)
    {
        printf("Error - rc:%d\n", rc);
        return rc;
    }
    rc = fx_get_bindable_rif_list(fx_handle, rif_list, &num_of_rifs);
    if (rc)
    {
        printf("Error - rc:%d\n", rc);
        return rc;
    }
        rc = fx_pipe_destroy(fx_handle, FX_CONTROL_IN_RIF, (void *)rif_list, num_of_rifs);
        if (rc) {
            printf("Error - rc:%d\n", rc);
            return rc;
        }
    fx_extern_deinit(fx_handle);
    fx_deinit(fx_handle);
    return SAI_STATUS_SUCCESS;
}
