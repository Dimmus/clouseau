#include "clouseau_private.h"
#include "clouseau_data_legacy.h"

static int clouseau_init_count = 0;

static Eet_Data_Descriptor *clouseau_union_edd = NULL;
static Eet_Data_Descriptor *clouseau_connect_edd = NULL;
static Eet_Data_Descriptor *clouseau_app_add_edd = NULL;
static Eet_Data_Descriptor *clouseau_data_req_edd = NULL;
static Eet_Data_Descriptor *clouseau_bmp_info_edd = NULL;
static Eet_Data_Descriptor *clouseau_shot_list_edd = NULL;
static Eet_Data_Descriptor *clouseau_tree_data_edd = NULL;
static Eet_Data_Descriptor *clouseau_tree_edd = NULL;
static Eet_Data_Descriptor *clouseau_app_closed_edd = NULL;
static Eet_Data_Descriptor *clouseau_highlight_edd = NULL;
static Eet_Data_Descriptor *clouseau_bmp_req_edd = NULL;
static Eet_Data_Descriptor *clouseau_protocol_edd = NULL;

static Eet_Data_Descriptor *eo_string_edd = NULL;
static Eet_Data_Descriptor *eo_int_edd = NULL;
static Eet_Data_Descriptor *eo_bool_edd = NULL;
static Eet_Data_Descriptor *eo_ptr_edd = NULL;
static Eet_Data_Descriptor *eo_double_edd = NULL;
static Eet_Data_Descriptor *eo_list_edd = NULL;
static Eet_Data_Descriptor *eo_dbg_info_edd = NULL;

EAPI void
clouseau_eo_info_free(Clouseau_Eo_Dbg_Info *parent)
{
   Clouseau_Eo_Dbg_Info *eo;

   if (parent->type == CLOUSEAU_DBG_INFO_TYPE_LIST)
     EINA_LIST_FREE(parent->un_dbg_info.dbg.list, eo)
        clouseau_eo_info_free(eo);
   else if (parent->type == CLOUSEAU_DBG_INFO_TYPE_STRING)
      eina_stringshare_del(parent->un_dbg_info.text.s);

   eina_stringshare_del(parent->name);
   free(parent);
}

static void
_clouseau_tree_item_free(Clouseau_Tree_Item *parent)
{
   Clouseau_Tree_Item *treeit;
   Clouseau_Eo_Dbg_Info *eo;

   if (parent->new_eo_info)
      eo_dbg_info_free(parent->new_eo_info);

   EINA_LIST_FREE(parent->eo_info, eo)
      clouseau_eo_info_free(eo);

   EINA_LIST_FREE(parent->children, treeit)
     _clouseau_tree_item_free(treeit);

   clouseau_object_information_free(parent->info);
   eina_stringshare_del(parent->name);
   free(parent);
}

EAPI void
clouseau_data_tree_free(Eina_List *tree)
{
   Clouseau_Tree_Item *treeit;

   EINA_LIST_FREE(tree, treeit)
     _clouseau_tree_item_free(treeit);
}

static void
_clouseau_connect_desc_make(void)
{
   Eet_Data_Descriptor_Class eddc;

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, connect_st);
   clouseau_connect_edd = eet_data_descriptor_stream_new(&eddc);

   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_connect_edd, connect_st,
                                 "pid", pid, EET_T_UINT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_connect_edd, connect_st,
                                 "name", name, EET_T_STRING);
}

static void
_clouseau_app_add_desc_make(void)
{  /* view field not transferred, will be loaded on user request */
   Eet_Data_Descriptor_Class eddc;

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, app_info_st);
   clouseau_app_add_edd = eet_data_descriptor_stream_new(&eddc);

   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_app_add_edd, app_info_st,
                                 "pid", pid, EET_T_UINT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_app_add_edd, app_info_st,
                                 "name", name, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_app_add_edd, app_info_st,
                                 "ptr", ptr, EET_T_ULONG_LONG);
}

static void
_clouseau_req_desc_make(void)
{
   Eet_Data_Descriptor_Class eddc;

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, data_req_st);
   clouseau_data_req_edd = eet_data_descriptor_stream_new(&eddc);

   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_data_req_edd, data_req_st,
                                 "gui", gui, EET_T_ULONG_LONG);
   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_data_req_edd, data_req_st,
                                 "app", app, EET_T_ULONG_LONG);
}

static void
_clouseau_bmp_info_desc_make(void)
{
   Eet_Data_Descriptor_Class eddc;

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, bmp_info_st);
   clouseau_bmp_info_edd = eet_data_descriptor_stream_new(&eddc);

   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_bmp_info_edd, bmp_info_st,
                                 "gui", gui, EET_T_ULONG_LONG);
   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_bmp_info_edd, bmp_info_st,
                                 "app", app, EET_T_ULONG_LONG);
   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_bmp_info_edd, bmp_info_st,
                                 "object", object, EET_T_ULONG_LONG);
   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_bmp_info_edd, bmp_info_st,
                                 "ctr", ctr, EET_T_ULONG_LONG);
   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_bmp_info_edd, bmp_info_st,
                                 "w", w, EET_T_ULONG_LONG);
   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_bmp_info_edd, bmp_info_st,
                                 "h", h, EET_T_ULONG_LONG);
}

static void
_clouseau_shot_list_desc_make(void)
{
   Eet_Data_Descriptor_Class eddc;

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, shot_list_st);
   clouseau_shot_list_edd = eet_data_descriptor_stream_new(&eddc);

   EET_DATA_DESCRIPTOR_ADD_LIST(clouseau_shot_list_edd, shot_list_st,
                                "view", view, clouseau_bmp_info_edd);
}

static void
_clouseau_tree_item_desc_make(void)
{
   Eet_Data_Descriptor_Class eddc;

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, Clouseau_Tree_Item);
   clouseau_tree_edd = eet_data_descriptor_stream_new(&eddc);

   EET_DATA_DESCRIPTOR_ADD_LIST(clouseau_tree_edd, Clouseau_Tree_Item,
                                "children", children, clouseau_tree_edd);
   EET_DATA_DESCRIPTOR_ADD_LIST(clouseau_tree_edd, Clouseau_Tree_Item,
                                "eo_info", eo_info, eo_dbg_info_edd);
   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_tree_edd, Clouseau_Tree_Item,
                                 "name", name, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_tree_edd, Clouseau_Tree_Item,
                                 "ptr", ptr, EET_T_ULONG_LONG);
   EET_DATA_DESCRIPTOR_ADD_SUB(clouseau_tree_edd, Clouseau_Tree_Item,
                               "info", info, clouseau_object_edd);
   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_tree_edd, Clouseau_Tree_Item,
                                 "is_obj", is_obj, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_tree_edd, Clouseau_Tree_Item,
                                 "is_clipper", is_clipper, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_tree_edd, Clouseau_Tree_Item,
                                 "is_visible", is_visible, EET_T_UCHAR);
}

static void
_clouseau_tree_data_desc_make(void)
{
   Eet_Data_Descriptor_Class eddc;

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, tree_data_st);
   clouseau_tree_data_edd = eet_data_descriptor_stream_new(&eddc);

   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_tree_data_edd, tree_data_st,
                                 "gui", gui, EET_T_ULONG_LONG);
   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_tree_data_edd, tree_data_st,
                                 "app", app, EET_T_ULONG_LONG);
   EET_DATA_DESCRIPTOR_ADD_LIST(clouseau_tree_data_edd, tree_data_st,
                                "tree", tree, clouseau_tree_edd);
}

static void
_clouseau_app_closed_desc_make(void)
{
   Eet_Data_Descriptor_Class eddc;

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, app_closed_st);
   clouseau_app_closed_edd = eet_data_descriptor_stream_new(&eddc);

   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_app_closed_edd, app_closed_st,
                                 "ptr", ptr, EET_T_ULONG_LONG);
}

static void
_clouseau_highlight_desc_make(void)
{
   Eet_Data_Descriptor_Class eddc;

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, highlight_st);
   clouseau_highlight_edd = eet_data_descriptor_stream_new(&eddc);

   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_highlight_edd, highlight_st,
                                 "app", app, EET_T_ULONG_LONG);
   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_highlight_edd, highlight_st,
                                 "object", object, EET_T_ULONG_LONG);
}

static void
_clouseau_bmp_req_desc_make(void)
{
   Eet_Data_Descriptor_Class eddc;

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, bmp_req_st);
   clouseau_bmp_req_edd = eet_data_descriptor_stream_new(&eddc);

   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_bmp_req_edd, bmp_req_st,
                                 "gui", gui, EET_T_ULONG_LONG);
   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_bmp_req_edd, bmp_req_st,
                                  "app", app, EET_T_ULONG_LONG);
   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_bmp_req_edd, bmp_req_st,
                                 "object", object, EET_T_ULONG_LONG);
   EET_DATA_DESCRIPTOR_ADD_BASIC(clouseau_bmp_req_edd, bmp_req_st,
                                 "ctr", ctr, EET_T_UINT);
}

/* START EO descs */
struct _Clouseau_Eo_Dbg_Info_Mapping
{
   Clouseau_Dbg_Info_Type u;
   const char *name;
};
typedef struct _Clouseau_Eo_Dbg_Info_Mapping Clouseau_Eo_Dbg_Info_Mapping;

/* It's init later. */
static Clouseau_Eo_Dbg_Info_Mapping eet_dbg_info_mapping[] =
{
     { CLOUSEAU_DBG_INFO_TYPE_STRING, EO_DBG_INFO_TYPE_STRING_STR },
     { CLOUSEAU_DBG_INFO_TYPE_INT, EO_DBG_INFO_TYPE_INT_STR },
     { CLOUSEAU_DBG_INFO_TYPE_BOOL, EO_DBG_INFO_TYPE_BOOL_STR },
     { CLOUSEAU_DBG_INFO_TYPE_PTR, EO_DBG_INFO_TYPE_PTR_STR },
     { CLOUSEAU_DBG_INFO_TYPE_DOUBLE, EO_DBG_INFO_TYPE_DOUBLE_STR },
     { CLOUSEAU_DBG_INFO_TYPE_LIST, EO_DBG_INFO_TYPE_LIST_STR },
     { CLOUSEAU_DBG_INFO_TYPE_UNKNOWN, NULL }
};

static const char *
_dbg_info_union_type_get(const void *data, Eina_Bool  *unknow)
{  /* _union_type_get */
   const Clouseau_Dbg_Info_Type *u = data;
   int i;

   if (unknow)
     *unknow = EINA_FALSE;

   for (i = 0; eet_dbg_info_mapping[i].name != NULL; ++i)
     if (*u == eet_dbg_info_mapping[i].u)
       return eet_dbg_info_mapping[i].name;

   if (unknow)
     *unknow = EINA_TRUE;

   return NULL;
}

static Eina_Bool
_dbg_info_union_type_set(const char *type, void *data, Eina_Bool unknow)
{  /* same as _union_type_set */
   Clouseau_Dbg_Info_Type *u = data;
   int i;

   if (unknow)
     return EINA_FALSE;

   for (i = 0; eet_dbg_info_mapping[i].name != NULL; ++i)
     if (strcmp(eet_dbg_info_mapping[i].name, type) == 0)
       {
          *u = eet_dbg_info_mapping[i].u;
          return EINA_TRUE;
       }

   return EINA_FALSE;
}

static Eet_Data_Descriptor *
clouseau_string_desc_make(void)
{
   Eet_Data_Descriptor *d;

   Eet_Data_Descriptor_Class eddc;
   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, Clouseau_st_string);
   d = eet_data_descriptor_stream_new(&eddc);

   EET_DATA_DESCRIPTOR_ADD_BASIC (d, Clouseau_st_string, "s",
         s, EET_T_STRING);

   return d;
}

static Eet_Data_Descriptor *
clouseau_int_desc_make(void)
{
   Eet_Data_Descriptor *d;

   Eet_Data_Descriptor_Class eddc;
   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, Clouseau_st_int);
   d = eet_data_descriptor_stream_new(&eddc);

   EET_DATA_DESCRIPTOR_ADD_BASIC (d, Clouseau_st_int, "i",
         i, EET_T_INT);

   return d;
}

static Eet_Data_Descriptor *
clouseau_bool_desc_make(void)
{
   Eet_Data_Descriptor *d;

   Eet_Data_Descriptor_Class eddc;
   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, Clouseau_st_bool);
   d = eet_data_descriptor_stream_new(&eddc);

   EET_DATA_DESCRIPTOR_ADD_BASIC (d, Clouseau_st_bool, "b",
         b, EET_T_UCHAR);

   return d;
}

static Eet_Data_Descriptor *
clouseau_ptr_desc_make(void)
{
   Eet_Data_Descriptor *d;

   Eet_Data_Descriptor_Class eddc;
   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, Clouseau_st_ptr);
   d = eet_data_descriptor_stream_new(&eddc);

   EET_DATA_DESCRIPTOR_ADD_BASIC (d, Clouseau_st_ptr, "p",
         p, EET_T_ULONG_LONG);

   return d;
}

static Eet_Data_Descriptor *
clouseau_double_desc_make(void)
{
   Eet_Data_Descriptor *d;

   Eet_Data_Descriptor_Class eddc;
   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, Clouseau_st_double);
   d = eet_data_descriptor_stream_new(&eddc);

   EET_DATA_DESCRIPTOR_ADD_BASIC (d, Clouseau_st_double, "d",
         d, EET_T_DOUBLE);

   return d;
}

static Eet_Data_Descriptor *
clouseau_list_desc_make(void)
{
   Eet_Data_Descriptor *d;

   Eet_Data_Descriptor_Class eddc;
   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, Clouseau_st_dbg_list);
   d = eet_data_descriptor_stream_new(&eddc);

   EET_DATA_DESCRIPTOR_ADD_LIST (d, Clouseau_st_dbg_list,
         "list", list, eo_dbg_info_edd); /* Carefull, has to be initiated */

   return d;
}

static void
_clouseau_eo_descs_make(void)
{
   Eet_Data_Descriptor_Class eddc;

   eo_string_edd = clouseau_string_desc_make();
   eo_int_edd = clouseau_int_desc_make();
   eo_bool_edd = clouseau_bool_desc_make();
   eo_ptr_edd = clouseau_ptr_desc_make();
   eo_double_edd = clouseau_double_desc_make();

   EET_EINA_FILE_DATA_DESCRIPTOR_CLASS_SET(&eddc, Clouseau_Eo_Dbg_Info);
   eo_dbg_info_edd = eet_data_descriptor_file_new(&eddc);
   EET_DATA_DESCRIPTOR_ADD_BASIC (eo_dbg_info_edd, Clouseau_Eo_Dbg_Info,
         "name", name, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC (eo_dbg_info_edd, Clouseau_Eo_Dbg_Info,
         "type", type, EET_T_UINT);

   /* Here because clouseau_list_desc_make() uses eo_dbg_info_edd */
   eo_list_edd = clouseau_list_desc_make();

   /* for union */
   eddc.version = EET_DATA_DESCRIPTOR_CLASS_VERSION;
   eddc.func.type_get = _dbg_info_union_type_get;
   eddc.func.type_set = _dbg_info_union_type_set;
   clouseau_union_edd = eet_data_descriptor_file_new(&eddc);

   EET_DATA_DESCRIPTOR_ADD_MAPPING(
         clouseau_union_edd, EO_DBG_INFO_TYPE_STRING_STR
         ,eo_string_edd);

   EET_DATA_DESCRIPTOR_ADD_MAPPING(
         clouseau_union_edd, EO_DBG_INFO_TYPE_INT_STR
         ,eo_int_edd);

   EET_DATA_DESCRIPTOR_ADD_MAPPING(
         clouseau_union_edd, EO_DBG_INFO_TYPE_BOOL_STR
         ,eo_bool_edd);

   EET_DATA_DESCRIPTOR_ADD_MAPPING(
         clouseau_union_edd, EO_DBG_INFO_TYPE_PTR_STR
         ,eo_ptr_edd);

   EET_DATA_DESCRIPTOR_ADD_MAPPING(
         clouseau_union_edd, EO_DBG_INFO_TYPE_DOUBLE_STR
         ,eo_double_edd);

   EET_DATA_DESCRIPTOR_ADD_MAPPING(
         clouseau_union_edd, EO_DBG_INFO_TYPE_LIST_STR
         ,eo_list_edd);

   EET_DATA_DESCRIPTOR_ADD_UNION(eo_dbg_info_edd,
         Clouseau_Eo_Dbg_Info,
         "un_dbg_info", un_dbg_info,
         type, clouseau_union_edd);
}
/* END EO descs */



static void
clouseau_data_descriptors_init(void)
{
   clouseau_data_descriptors_legacy_init();
   _clouseau_eo_descs_make();
   _clouseau_bmp_req_desc_make();
   _clouseau_bmp_info_desc_make();
   _clouseau_shot_list_desc_make();
   _clouseau_tree_item_desc_make();
   _clouseau_connect_desc_make();
   _clouseau_app_add_desc_make();
   _clouseau_req_desc_make();
   _clouseau_tree_data_desc_make();
   _clouseau_app_closed_desc_make();
   _clouseau_highlight_desc_make();
}

static void
clouseau_data_descriptors_shutdown(void)
{
   eet_data_descriptor_free(eo_string_edd);
   eet_data_descriptor_free(eo_int_edd);
   eet_data_descriptor_free(eo_bool_edd);
   eet_data_descriptor_free(eo_ptr_edd);
   eet_data_descriptor_free(eo_double_edd);
   eet_data_descriptor_free(eo_list_edd);
   eet_data_descriptor_free(eo_dbg_info_edd);
   eet_data_descriptor_free(clouseau_union_edd);

   eet_data_descriptor_free(clouseau_connect_edd);
   eet_data_descriptor_free(clouseau_app_add_edd);
   eet_data_descriptor_free(clouseau_data_req_edd);
   eet_data_descriptor_free(clouseau_tree_edd);
   eet_data_descriptor_free(clouseau_app_closed_edd);
   eet_data_descriptor_free(clouseau_highlight_edd);
   eet_data_descriptor_free(clouseau_bmp_req_edd);
   eet_data_descriptor_free(clouseau_bmp_info_edd);
   eet_data_descriptor_free(clouseau_shot_list_edd);
   eet_data_descriptor_free(clouseau_protocol_edd);
   clouseau_data_descriptors_legacy_shutdown();
}

static void *
_host_to_net_blob_get(void *blob, int *blob_size)
{
   if (!blob)
     return blob;

   /* Complete blob_size to sizeof(uint32_t) */
   int mod = (*blob_size) % sizeof(uint32_t);
   if (mod)
     *blob_size += (sizeof(uint32_t) - mod);

   void *n_blob = malloc(*blob_size);
   uint32_t *src = blob;
   uint32_t *dst = n_blob;
   int cnt = (*blob_size) / sizeof(uint32_t);
   while (cnt)
     {
        *dst = htonl(*src);
        src++;
        dst++;
        cnt--;
     }

   return n_blob;
}

static void *
_net_to_host_blob_get(void *blob, int blob_size)
{
   if (!blob)
     return blob;

   void *h_blob = malloc(blob_size);

   uint32_t *src = blob;
   uint32_t *dst = h_blob;
   int cnt = blob_size / sizeof(uint32_t);
   while (cnt)
     {
        *dst = ntohl(*src);
        src++;
        dst++;
        cnt--;
     }

   return h_blob;
}

EAPI void *
clouseau_data_packet_compose(const char *p_type, void *data,
      unsigned int *size, void *blob, int blob_size)
{  /* Returns packet BLOB and size in size param, NULL on failure */
   /* User has to free returned buffer                            */
   /* Packet is composed of Message Type + packet data.           */
   void *net_blob = NULL;

   if (!strcmp(p_type, CLOUSEAU_BMP_DATA_STR))
     {  /* Builed Bitmap data as follows:
           First uint32_t is encoding size of bmp_info_st
           The next to come will be the encoded bmp_info_st itself
           Then we have blob_size param (specifiying bmp-blob-size)
           folloed by the Bitmap raw data.                          */

        int t_size; /* total size */
        int e_size;
        uint32_t e_size32;
        uint32_t tmp;
        void *p;
        char *b;
        char *ptr;

        /* First, we like to encode bmp_info_st from data  */
        p = eet_data_descriptor_encode(clouseau_bmp_info_edd, data, &e_size);
        e_size32 = (uint32_t) e_size;

        /* Allocate buffer to hold whole packet data */
        t_size = sizeof(e_size32) + /* encoding size of bmp_info_st */
           + e_size                 /* Encoded bmp_info_st */
           + sizeof(e_size32)       /* bmp-blob-size       */
           + blob_size;             /* The BMP blob data   */

        ptr = b = malloc(t_size);

        /* START - Build BMP_RAW_DATA packet data */
        /* Size of encoded bmp_info_st comes next in uint32 format */
        memcpy(ptr, &e_size32, sizeof(e_size32));
        ptr += sizeof(e_size32);

        /* Encoded bmp_info_st comes next */
        memcpy(ptr, p, e_size);
        ptr += e_size;

        /* Size of BMP blob comes next */
        tmp = (uint32_t) blob_size;
        memcpy(ptr, &tmp, sizeof(uint32_t));
        ptr += sizeof(uint32_t);

        if (blob && blob_size)
          {  /* BMP blob info comes right after BMP blob_size */
             memcpy(ptr, blob, blob_size);
          }

        /* Save encoded size in network format */
        net_blob = _host_to_net_blob_get(b, &t_size);
        *size = t_size;  /* Update packet size */

        /*  All info now in net_blob, free allocated mem */
        free(b);
        free(p);
        /* END   - Build BMP_RAW_DATA packet data */
     }

   return net_blob;
}

EAPI void *
clouseau_data_packet_info_get(const char *p_type, void *data, size_t size)
{
   bmp_info_st *st = NULL;
   void *host_blob = NULL;
   char *ptr = NULL;

   if (size <= 0)
      return NULL;

   host_blob = _net_to_host_blob_get(data, size);
   ptr = host_blob;

   if (!strcmp(p_type, CLOUSEAU_BMP_DATA_STR))
     {
        uint32_t *e_size32 = (uint32_t *) ptr;
        int e_size = (int) (*e_size32); /* First Encoded bmp_info_st size */
        ptr += sizeof(uint32_t);

        /* Get the encoded bmp_info_st */
        st = eet_data_descriptor_decode(clouseau_bmp_info_edd
              ,ptr, e_size);
        ptr += e_size;

        st->bmp = NULL;

        /* Next Get bmp-blob-size */
        e_size32 = (uint32_t *) ptr;
        e_size = (int) (*e_size32); /* Get bmp-blob size */
        ptr += sizeof(uint32_t);

        /* Now we need to get the bmp data */
        if (e_size)
          {  /* BMP data available, allocate and copy    */
             st->bmp = malloc(e_size);  /* Freed by user */
             memcpy(st->bmp, ptr, e_size);
          }
     }  /* User has to free st, st->bmp */

   free(host_blob);
   return st;
}

EAPI Eina_Bool
clouseau_data_eet_info_save(const char *filename,
              app_info_st *a, tree_data_st *ftd, Eina_List *ck_list)
{
   Eina_List *shots = NULL;
   Eina_List *l;
   Eet_File *fp;
   Evas_Object *ck;

   fp = eet_open(filename, EET_FILE_MODE_WRITE);
   if (!fp) return EINA_FALSE;

   eet_data_write(fp, clouseau_app_add_edd, CLOUSEAU_APP_ADD_ENTRY,
                  a, EINA_TRUE);
   eet_data_write(fp, clouseau_tree_data_edd, CLOUSEAU_TREE_DATA_ENTRY,
                  ftd, EINA_TRUE);

   /* Build list of (bmp_info_st *) according to user selection    */
   EINA_LIST_FOREACH(ck_list, l , ck)
     if (elm_check_state_get(ck))
       {
          void *data;

          data = evas_object_data_get(ck, BMP_FIELD);
          if (data)
            shots = eina_list_append(shots, data);
       }

   if (shots)
     {
        /* Write list and bitmaps */
        char buf[1024];
        shot_list_st t;
        bmp_info_st *st;

        t.view = shots;
        eet_data_write(fp, clouseau_shot_list_edd, CLOUSEAU_BMP_LIST_ENTRY,
                       &t, EINA_TRUE);
        EINA_LIST_FREE(shots, st)
          {
             sprintf(buf, CLOUSEAU_BMP_DATA_ENTRY"/%llx", st->object);
             eet_data_image_write(fp, buf, st->bmp,
                                  st->w, st->h, 1, 0, 100, 0);
          }
     }

   eet_close(fp);

   return EINA_TRUE;
}

EAPI Eina_Bool
clouseau_data_eet_info_read(const char *filename,
              app_info_st **a, tree_data_st **ftd)
{
   bmp_info_st *st;
   shot_list_st *t;
   Eet_File *fp;

   fp = eet_open(filename, EET_FILE_MODE_READ);
   if (!fp) return EINA_FALSE;

   *a = eet_data_read(fp, clouseau_app_add_edd, CLOUSEAU_APP_ADD_ENTRY);
   *ftd = eet_data_read(fp, clouseau_tree_data_edd, CLOUSEAU_TREE_DATA_ENTRY);
   t = eet_data_read(fp, clouseau_shot_list_edd, CLOUSEAU_BMP_LIST_ENTRY);

   if (t)
     {
        EINA_LIST_FREE(t->view, st)
          {
             char buf[1024];
             int alpha;
             int compress;
             int quality;
             int lossy;

             sprintf(buf, CLOUSEAU_BMP_DATA_ENTRY"/%llx", st->object);
             st->bmp = eet_data_image_read(fp, buf,
                   (unsigned int *) &st->w,
                   (unsigned int *) &st->h,
                   &alpha, &compress, &quality, &lossy);

             /* Add the bitmaps to the actuall app data struct */
             (*a)->view = eina_list_append((*a)->view, st);
          }
        free(t);
     }

   eet_close(fp);

   return EINA_TRUE;
}

EAPI int
clouseau_data_init(void)
{
   if (clouseau_init_count++ != 0)
     return clouseau_init_count;

   eina_init();
   eet_init();
   ecore_init();

   clouseau_data_descriptors_init();

   return clouseau_init_count;
}

EAPI int
clouseau_register_descs(Ecore_Con_Eet *eet_svr)
{  /* Register descriptors for ecore_con_eet */
   if (clouseau_init_count)
     {  /* MUST be called after clouseau_data_init */
        ecore_con_eet_register(eet_svr, CLOUSEAU_GUI_CLIENT_CONNECT_STR,
              clouseau_connect_edd);
        ecore_con_eet_register(eet_svr, CLOUSEAU_APP_CLIENT_CONNECT_STR,
              clouseau_connect_edd);
        ecore_con_eet_register(eet_svr, CLOUSEAU_APP_ADD_STR,
              clouseau_app_add_edd);
        ecore_con_eet_register(eet_svr, CLOUSEAU_DATA_REQ_STR,
              clouseau_data_req_edd);
        ecore_con_eet_register(eet_svr, CLOUSEAU_TREE_DATA_STR,
              clouseau_tree_data_edd);
        ecore_con_eet_register(eet_svr, CLOUSEAU_APP_CLOSED_STR,
              clouseau_app_closed_edd);
        ecore_con_eet_register(eet_svr, CLOUSEAU_HIGHLIGHT_STR,
              clouseau_highlight_edd);
        ecore_con_eet_register(eet_svr, CLOUSEAU_BMP_REQ_STR,
              clouseau_bmp_req_edd);
        ecore_con_eet_register(eet_svr, CLOUSEAU_BMP_DATA_STR,
              clouseau_bmp_info_edd);
     }

   return clouseau_init_count;
}

EAPI int
clouseau_data_shutdown(void)
{
   if (--clouseau_init_count != 0)
     return clouseau_init_count;

   clouseau_data_descriptors_shutdown();

   ecore_shutdown();
   eet_shutdown();
   eina_shutdown();

   return clouseau_init_count;
}
