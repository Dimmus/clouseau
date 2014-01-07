#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <Elementary_Cursor.h>
#include <Ecore_Con_Eet.h>

#include "Clouseau.h"
#include "client/cfg.h"
#include "client/config_dialog.h"

#define CLIENT_NAME         "Clouseau Client"
#define SELECT_APP_TEXT     "Select App"

#define SHOW_SCREENSHOT     "/images/show-screenshot.png"
#define TAKE_SCREENSHOT     "/images/take-screenshot.png"
#define SCREENSHOT_MISSING  "/images/screenshot-missing.png"

static int _clouseau_client_log_dom = -1;

#ifdef CRITICAL
#undef CRITICAL
#endif
#define CRITICAL(...) EINA_LOG_DOM_CRIT(_clouseau_client_log_dom, __VA_ARGS__)

#ifdef ERR
#undef ERR
#endif
#define ERR(...) EINA_LOG_DOM_ERR(_clouseau_client_log_dom, __VA_ARGS__)

#ifdef WRN
#undef WRN
#endif
#define WRN(...) EINA_LOG_DOM_WARN(_clouseau_client_log_dom, __VA_ARGS__)

#ifdef INF
#undef INF
#endif
#define INF(...) EINA_LOG_DOM_INFO(_clouseau_client_log_dom, __VA_ARGS__)

#ifdef DBG
#undef DBG
#endif
#define DBG(...) EINA_LOG_DOM_DBG(_clouseau_client_log_dom, __VA_ARGS__)

static Evas_Object *prop_list = NULL;
static Elm_Genlist_Item_Class _obj_info_itc;
// Item class for objects classnames
static Elm_Genlist_Item_Class _class_info_itc;

/* FIXME: Most hackish thing even seen. Needed because of the lack of a
 * way to list genlist item's children. */
static Elm_Object_Item *_tree_item_show_last_expanded_item = NULL;

struct _App_Data_St
{
   app_info_st *app;
   tree_data_st *td;
};
typedef struct _App_Data_St App_Data_St;

struct _Bmp_Node
{
   unsigned int ctr;           /* Current refresh_ctr */
   unsigned long long object;  /* Evas ptr        */
   Evas_Object *bt;            /* Button of BMP_REQ */
};
typedef struct _Bmp_Node Bmp_Node;

struct _Gui_Elementns
{
   Evas_Object *win;
   Evas_Object *bx;     /* The main box */
   Evas_Object *hbx;    /* The top menu box */
   Evas_Object *bt_load;
   Evas_Object *bt_save;
   Evas_Object *dd_list;
   Evas_Object *gl;
   Evas_Object *prop_list;
   Evas_Object *connect_inwin;
   Evas_Object *save_inwin;
   Evas_Object *en;
   Evas_Object *pb; /* Progress wheel shown when waiting for TREE_DATA */
   char *address;
   App_Data_St *sel_app; /* Currently selected app data */
   Elm_Object_Item *gl_it; /* Currently selected genlist item */
   uintptr_t jump_to_ptr;
};
typedef struct _Gui_Elementns Gui_Elements;

static void _load_list(Gui_Elements *g);
static void _bt_load_file(void *data, Evas_Object *obj EINA_UNUSED, void *event_info);
static void _show_gui(Gui_Elements *g, Eina_Bool work_offline);

/* Globals */
static Gui_Elements *gui = NULL;
static Eina_List *apps = NULL;    /* List of (App_Data_St *)  */
static Eina_List *bmp_req = NULL; /* List of (Bmp_Node *)     */

static Elm_Genlist_Item_Class itc;
static Eina_Bool do_highlight = EINA_TRUE;
static Ecore_Con_Reply *eet_svr = NULL;
static Eina_Bool _add_callback_called = EINA_FALSE;
static void _cancel_bt_clicked(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED);
static void _ofl_bt_clicked(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED);
static void _jump_to_ptr(Gui_Elements *g, uintptr_t ptr);

static void
_titlebar_string_set(Gui_Elements *g, Eina_Bool online)
{
   if (online)
     {
        char *str = malloc(strlen(CLIENT_NAME) + (g->address ? strlen(g->address) : 0) + 32);
        sprintf(str, "%s - %s", CLIENT_NAME, g->address);
        elm_win_title_set(g->win, str);
        free(str);
     }
   else
     {
        char *str = malloc(strlen(CLIENT_NAME) + strlen(" - Offline") + 32);
        sprintf(str, "%s - Offline", CLIENT_NAME);
        elm_win_title_set(g->win, str);
        free(str);
     }
}

Eina_Bool
_add(EINA_UNUSED void *data, Ecore_Con_Reply *reply,
      EINA_UNUSED Ecore_Con_Server *conn)
{
   _add_callback_called = EINA_TRUE;

   eet_svr = reply;
   connect_st t = { getpid(), __FILE__ };
   ecore_con_eet_send(reply, CLOUSEAU_GUI_CLIENT_CONNECT_STR, &t);
   _titlebar_string_set(gui, EINA_TRUE);

   return ECORE_CALLBACK_RENEW;
}

static void
_set_button(Evas_Object *w, Evas_Object *bt,
      char *ic_name, char *tip, Eina_Bool en)
{  /* Update button icon and tooltip */
   char buf[1024];
   Evas_Object *ic = elm_icon_add(w);
   snprintf(buf, sizeof(buf), "%s%s", PACKAGE_DATA_DIR, ic_name);
   elm_image_file_set(ic, buf, NULL);
   elm_object_part_content_set(bt, "icon", ic);
   elm_object_tooltip_text_set(bt, tip);
   elm_object_disabled_set(bt, en);
   evas_object_show(ic);
}

static void
_work_offline_popup(void)
{
   Evas_Object *bxx, *lb, *bt_bx, *bt_ofl, *bt_exit;
   /* START - Popup asking user to close client or work offline */
   gui->connect_inwin = elm_win_inwin_add(gui->win);
   evas_object_show(gui->connect_inwin);

   bxx = elm_box_add(gui->connect_inwin);
   elm_object_style_set(gui->connect_inwin, "minimal_vertical");
   evas_object_size_hint_weight_set(bxx, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(bxx);

   lb = elm_label_add(bxx);
   evas_object_size_hint_weight_set(lb, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(lb, EVAS_HINT_FILL, 0.0);
   elm_object_text_set(lb, "Connection to server failed.");
   elm_box_pack_end(bxx, lb);
   evas_object_show(lb);

   bt_bx = elm_box_add(bxx);
   elm_box_horizontal_set(bt_bx, EINA_TRUE);
   elm_box_homogeneous_set(bt_bx, EINA_TRUE);
   evas_object_size_hint_align_set(bt_bx, 0.5, 0.5);
   evas_object_size_hint_weight_set(bt_bx, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(bt_bx);
   elm_box_pack_end(bxx, bt_bx);

   /* Add the exit button */
   bt_exit = elm_button_add(bt_bx);
   elm_object_text_set(bt_exit, "Exit");
   evas_object_smart_callback_add(bt_exit, "clicked",
         _cancel_bt_clicked, (void *) gui);

   elm_box_pack_end(bt_bx, bt_exit);
   evas_object_show(bt_exit);

   bt_ofl = elm_button_add(bt_bx);
   elm_object_text_set(bt_ofl, "Work Offline");
   evas_object_smart_callback_add(bt_ofl, "clicked",
         _ofl_bt_clicked, (void *) gui);

   elm_box_pack_end(bt_bx, bt_ofl);
   evas_object_show(bt_ofl);

   elm_win_inwin_content_set(gui->connect_inwin, bxx);
   /* END   - Popup asking user to close client or work offline */
}

Eina_Bool
_del(EINA_UNUSED void *data, EINA_UNUSED Ecore_Con_Reply *reply,
      Ecore_Con_Server *conn)
{
   if ((!_add_callback_called) || (!eet_svr))
     {  /* if initial connection with daemon failed - exit */
        ecore_con_server_del(conn);
        eet_svr = NULL; /* Global svr var */
        _work_offline_popup();
        return ECORE_CALLBACK_RENEW;
     }

   ERR("Lost server with ip <%s>!\n", ecore_con_server_ip_get(conn));

   ecore_con_server_del(conn);
   eet_svr = NULL; /* Global svr var */
   _show_gui(gui, EINA_TRUE);

   return ECORE_CALLBACK_RENEW;
}

static void
clouseau_lines_free(bmp_info_st *st)
{  /* Free lines asociated with a bmp */
   if (st->lx)
     evas_object_del(st->lx);

   if (st->ly)
     evas_object_del(st->ly);

   st->lx = st->ly = NULL;
}

static void
clouseau_bmp_blob_free(bmp_info_st *st)
{  /* We also free all lines drawn in this bmp canvas */
   clouseau_lines_free(st);

   if (st->bmp)
     free(st->bmp);
}

static Eina_Bool
_load_gui_with_list(Gui_Elements *g, Eina_List *trees)
{
   Eina_List *l;
   Clouseau_Tree_Item *treeit;

   if (!trees)
     return EINA_TRUE;

   /* Stop progress wheel as we load tree data */
   elm_progressbar_pulse(g->pb, EINA_FALSE);
   evas_object_hide(g->pb);

   elm_genlist_clear(g->gl);

   EINA_LIST_FOREACH(trees, l, treeit)
     {  /* Insert the base ee items */
        Elm_Genlist_Item_Type glflag = (treeit->children) ?
           ELM_GENLIST_ITEM_TREE : ELM_GENLIST_ITEM_NONE;
        elm_genlist_item_append(g->gl, &itc, treeit, NULL,
              glflag, NULL, NULL);
     }

   if (g->jump_to_ptr)
     {
        _jump_to_ptr(g, g->jump_to_ptr);
        g->jump_to_ptr = 0;
     }

   return EINA_TRUE;
}

static char *
_app_name_get(app_info_st *app)
{
   char *str = NULL;
   if (app->file)
     {
        char *tmp = strdup(app->file);
        char *bname = basename(tmp);
        str = malloc(strlen(bname) + strlen(app->name) + 32);
        sprintf(str, "%s:%s [%d]", bname, app->name, app->pid);
        free(tmp);
     }
   else
     {
        str = malloc(strlen(app->name)+32);
        sprintf(str, "%s [%d]", app->name, app->pid);
     }

   return str;  /* User has to free(str) */
}

static void
_close_app_views(app_info_st *app, Eina_Bool clr)
{  /* Close any open-views if this app */
   Eina_List *l;
   bmp_info_st *view;
   EINA_LIST_FOREACH(app->view, l, view)
     {
        if (view->win)
          evas_object_del(view->win);

        if (view->bt)
          elm_object_disabled_set(view->bt, EINA_FALSE);

        view->win = view->bt = NULL;
     }

   if (clr)
     {  /* These are cleared when app data is reloaded */
        EINA_LIST_FREE(app->view, view)
          {  /* Free memory allocated to show any app screens */
             clouseau_bmp_blob_free(view);
             free(view);
          }

        app->view = NULL;
     }
}

static void
_set_selected_app(void *data, Evas_Object *pobj,
      void *event_info EINA_UNUSED)
{  /* Set hovel label */
   App_Data_St *st = data;
   elm_progressbar_pulse(gui->pb, EINA_FALSE);
   evas_object_hide(gui->pb);
   gui->gl_it = NULL;

   if (gui->sel_app)
     _close_app_views(gui->sel_app->app, EINA_FALSE);

   if (st)
     {
        if (!eet_svr)
          {  /* Got TREE_DATA from file, update this immidately */
              gui->sel_app = st;
             char *str = _app_name_get(st->app);
             elm_object_text_set(pobj, str);
             free(str);
             _load_list(gui);
             return;
          }

        if (gui->sel_app != st)
          {  /* Reload only of selected some other app */
             gui->sel_app = st;
             char *str = _app_name_get(st->app);
             elm_object_text_set(pobj, str);
             free(str);

             elm_progressbar_pulse(gui->pb, EINA_FALSE);
             evas_object_hide(gui->pb);
             _load_list(gui);
          }
     }
   else
     {  /* If we got a NULL ptr, reset lists and dd_list text */
        elm_object_text_set(pobj, SELECT_APP_TEXT);
        elm_genlist_clear(gui->gl);
        elm_genlist_clear(gui->prop_list);
        gui->sel_app = NULL;
     }

   if (eet_svr)
     {  /* Enable/Disable buttons only if we are online */
        elm_object_disabled_set(gui->bt_load, (gui->sel_app == NULL));
        elm_object_disabled_set(gui->bt_save, (gui->sel_app == NULL));
     }
}

static int
_app_ptr_cmp(const void *d1, const void *d2)
{
   const App_Data_St *info = d1;
   app_info_st *app = info->app;

   return ((app->ptr) - (unsigned long long) (uintptr_t) d2);
}

static void
_add_app_to_dd_list(Evas_Object *dd_list, App_Data_St *st)
{  /* Add app to Drop Down List */
   char *str = _app_name_get(st->app);
   elm_hoversel_item_add(dd_list, str, NULL, ELM_ICON_NONE,
         _set_selected_app, st);

   free(str);
}

static int
_bmp_object_ptr_cmp(const void *d1, const void *d2)
{  /* Comparison according to Evas ptr of BMP struct */
   const bmp_info_st *bmp = d1;
   return ((bmp->object) - (unsigned long long) (uintptr_t) d2);
}

static int
_bmp_app_ptr_cmp(const void *d1, const void *d2)
{  /* Comparison according to app ptr of BMP struct */
   const bmp_info_st *bmp = d1;
   return ((bmp->app) - (unsigned long long) (uintptr_t) d2);
}

static Eina_List *
_remove_bmp(Eina_List *view, void *ptr)
{  /* Remove app bitmap from bitmaps list */
   bmp_info_st *st = (bmp_info_st *)
      eina_list_search_unsorted(view, _bmp_app_ptr_cmp,
            (void *) (uintptr_t) ptr);

   if (st)
     {
        if (st->win)
          evas_object_del(st->win);

        if (st->bmp)
          free(st->bmp);

        free(st);
        return eina_list_remove(view, st);
     }

   return view;
}

static App_Data_St *
_add_app(Gui_Elements *g, app_info_st *app)
{
   App_Data_St *st;

   st = malloc(sizeof(App_Data_St));
   if (!st) return NULL;

   st->app = app;
   st->td = NULL; /* Will get this on TREE_DATA message */
   apps = eina_list_append(apps, st);

   _add_app_to_dd_list(g->dd_list, st);

   return st;
}

static void
_free_app_tree_data(tree_data_st *ftd)
{

   if (!ftd) return;

   clouseau_data_tree_free(ftd->tree);
   free(ftd);
}

static void
_free_app(App_Data_St *st)
{
   bmp_info_st *view;
   app_info_st *app = st->app;
   if (app->file)
     free(app->file);

   EINA_LIST_FREE(app->view, view)
     {  /* Free memory allocated to show any app screens */
        if (view->win)
          evas_object_del(view->win);

        if (view->bmp)
          free(view->bmp);

        free(view);
     }

   _free_app_tree_data(st->td);
   free(app);
   free(st);
}

static void
_remove_app(Gui_Elements *g, app_closed_st *app)
{  /* Handle the case that NO app is selected, set sel_app to NULL */
   app_info_st *sel_app = (g->sel_app) ? g->sel_app->app: NULL;
   App_Data_St *st = (App_Data_St *)
      eina_list_search_unsorted(apps, _app_ptr_cmp,
            (void *) (uintptr_t) app->ptr);

   /* if NO app selected OR closing app is the selected one, reset display */
   if ((!sel_app) || (app->ptr == sel_app->ptr))
     _set_selected_app(NULL, g->dd_list, NULL);

   if (st)
     {  /* Remove from list and free all app info */
        Eina_List *l;
        apps = eina_list_remove(apps, st);
        _free_app(st);

        if (elm_hoversel_expanded_get(g->dd_list))
          elm_hoversel_hover_end(g->dd_list);

        elm_hoversel_clear(g->dd_list);
        EINA_LIST_FOREACH(apps, l , st)
           _add_app_to_dd_list(g->dd_list, st);
     }
}

static void
_update_tree_offline(Gui_Elements *g, tree_data_st *td)
{
   elm_genlist_clear(g->gl);
   _load_gui_with_list(g, td->tree);
}

static int
_Bmp_Node_cmp(const void *d1, const void *d2)
{  /* Compare accoring to Evas ptr */
   const Bmp_Node *info = d1;

   return ((info->object) - (unsigned long long) (uintptr_t) d2);
}

static Bmp_Node *
_get_Bmp_Node(bmp_info_st *st, app_info_st *app)
{  /* Find the request of this bmp info, in the req list         */
   /* We would like to verify this bmp_info_st is still relevant */
   Eina_List *req_list = bmp_req;
   Bmp_Node *nd = NULL;

   if (!app)
     return NULL;

   do
     { /* First find according to Evas ptr, then match ctr with refresh_ctr */
        req_list = eina_list_search_unsorted_list(req_list, _Bmp_Node_cmp,
              (void *) (uintptr_t) st->object);

        if (req_list)
          nd = (Bmp_Node *) eina_list_data_get(req_list);

        if (nd)
          {  /* if found this object in list, compare ctr */
             if (nd->ctr == app->refresh_ctr)
               return nd;

             /* ctr did not match, look further in list */
             req_list = eina_list_next(req_list);
          }
     }
   while(req_list);

   return NULL;
}

static void
clouseau_make_lines(bmp_info_st *st, Evas_Coord xx, Evas_Coord yy)
{  /* and no, we are NOT talking about WHITE lines */
   Evas_Coord x_rgn, y_rgn, w_rgn, h_rgn;

   clouseau_lines_free(st);

   elm_scroller_region_get(st->scr, &x_rgn, &y_rgn, &w_rgn, &h_rgn);

   st->lx = evas_object_line_add(evas_object_evas_get(st->o));
   st->ly = evas_object_line_add(evas_object_evas_get(st->o));
   evas_object_repeat_events_set(st->lx, EINA_TRUE);
   evas_object_repeat_events_set(st->ly, EINA_TRUE);

   evas_object_line_xy_set(st->lx, 0, yy, w_rgn, yy);
   evas_object_line_xy_set(st->ly, xx, 0, xx, h_rgn);

   evas_object_color_set(st->lx, HIGHLIGHT_R, HIGHLIGHT_G, HIGHLIGHT_B,
         HIGHLIGHT_A);
   evas_object_color_set(st->ly, HIGHLIGHT_R, HIGHLIGHT_G, HIGHLIGHT_B,
         HIGHLIGHT_A);
   evas_object_show(st->lx);
   evas_object_show(st->ly);
}

static void
clouseau_lines_cb(void *data,
      Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
      void *event_info)
{
   if (((Evas_Event_Mouse_Down *) event_info)->button == 1)
     return; /* Draw line only if not left mouse button */

   clouseau_make_lines(data, 
         (((Evas_Event_Mouse_Move *) event_info)->cur.canvas.x),
         (((Evas_Event_Mouse_Move *) event_info)->cur.canvas.y));
}

static void
_mouse_out(void *data,
      Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
      EINA_UNUSED void *event_info)
{
   bmp_info_st *st = data;
   elm_object_text_set(st->lb_mouse, " ");
   elm_object_text_set(st->lb_rgba, " ");
}

static void
_mouse_move(void *data,
      Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
      void *event_info)
{  /* Event info is label getting mouse pointer cords */
   bmp_info_st *st = data;
   unsigned char *pt;
   char s_bar[64];
   float dx, dy;
   Evas_Coord mp_x, mp_y, xx, yy;
   Evas_Coord x, y, w, h;

   mp_x = (((Evas_Event_Mouse_Move *) event_info)->cur.canvas.x);
   mp_y = (((Evas_Event_Mouse_Move *) event_info)->cur.canvas.y);
   evas_object_geometry_get(st->o, &x, &y, &w, &h);

   dx = ((float) (mp_x - x)) / ((float) w);
   dy = ((float) (mp_y - y)) / ((float) h);

   xx = dx * st->w;
   yy = dy * st->h;

   sprintf(s_bar, "%dx%d", xx, yy);

   elm_object_text_set(st->lb_mouse, s_bar);

   if (((Evas_Event_Mouse_Move *) event_info)->buttons > 1)
     clouseau_make_lines(st, mp_x, mp_y);

   if (((xx >= 0) && (xx < ((Evas_Coord) st->w))) &&
         ((yy >= 0) && (yy < ((Evas_Coord) st->h))))
     { /* Need to test borders, because image may be scrolled */
        pt = ((unsigned char *) st->bmp) + (((yy * st->w) + xx) * sizeof(int));
        sprintf(s_bar, "rgba(%d,%d,%d,%d)", pt[2], pt[1], pt[0], pt[3]);
        elm_object_text_set(st->lb_rgba, s_bar);
     }
   else
     elm_object_text_set(st->lb_rgba, " ");
}

static void
_app_win_del(void *data,
      Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{  /* when closeing view, set view ptr to NULL, and enable open button */
   bmp_info_st *st = data;
   clouseau_lines_free(st);
   elm_object_disabled_set(st->bt, EINA_FALSE);
   evas_object_event_callback_del(st->o, EVAS_CALLBACK_MOUSE_MOVE,
         _mouse_move);
   evas_object_event_callback_del(st->o, EVAS_CALLBACK_MOUSE_OUT,
         _mouse_out);
   evas_object_event_callback_del(st->o, EVAS_CALLBACK_MOUSE_DOWN,
         clouseau_lines_cb);
   st->win = st->bt = st->lb_mouse = st->o = NULL;
}

/* START - Callbacks to handle zoom on app window (screenshot) */
static Evas_Event_Flags
reset_view(void *data , void *event_info EINA_UNUSED)
{  /* Cancel ZOOM and remove LINES on double tap */
   bmp_info_st *st = data;
   st->zoom_val = 1.0;
   clouseau_lines_free(st);
   evas_object_size_hint_min_set(st->o, st->w, st->h);

   return EVAS_EVENT_FLAG_ON_HOLD;
}

static void
_update_zoom(Evas_Object *img, Evas_Object *scr, Evas_Coord zx,
      Evas_Coord zy, double zoom, Evas_Coord origw, Evas_Coord origh)
{
   Evas_Coord origrelx = 0, origrely= 0;
   Evas_Coord offx = 0, offy= 0;

   Evas_Coord sx, sy, sw, sh;
   elm_scroller_region_get(scr, &sx, &sy, &sw, &sh);

   /* Get coords on pic. */
     {
        Evas_Coord x, y, w, h;
        evas_object_geometry_get(img, &x, &y, &w, &h);
        double ratio = (((double) origw) / w) * zoom;
        origrelx = ratio * (double) (zx - x);
        origrely = ratio * (double) (zy - y);

        /* Offset of the cursor from the first visible pixel of the
         * content. */
        offx = (zx - x) - sx;
        offy = (zy - y) - sy;
     }

   Evas_Coord imw, imh;
   imw = origw * zoom;
   imh = origh * zoom;
   evas_object_size_hint_min_set(img, imw, imh);
   evas_object_size_hint_max_set(img, imw, imh);

   elm_scroller_region_show(scr, origrelx - offx, origrely - offy, sw, sh);
}

static Evas_Event_Flags
zoom_start(void *data , void *event_info)
{
   bmp_info_st *st = data;
   Elm_Gesture_Zoom_Info *p = (Elm_Gesture_Zoom_Info *) event_info;
   clouseau_lines_free(st);
   _update_zoom(st->o, st->scr, p->x, p->y, st->zoom_val, st->w, st->h);

   return EVAS_EVENT_FLAG_ON_HOLD;
}

static Evas_Event_Flags
zoom_move(void *data , void *event_info)
{
   bmp_info_st *st = data;
   Elm_Gesture_Zoom_Info *p = (Elm_Gesture_Zoom_Info *) event_info;
   _update_zoom(st->o, st->scr, p->x, p->y,
         st->zoom_val * p->zoom, st->w, st->h);

   return EVAS_EVENT_FLAG_ON_HOLD;
}

static Evas_Event_Flags
zoom_end(void *data , void *event_info)
{
   Elm_Gesture_Zoom_Info *p = (Elm_Gesture_Zoom_Info *) event_info;
   bmp_info_st *st = data;
   st->zoom_val *= p->zoom;

   return EVAS_EVENT_FLAG_ON_HOLD;
}
/* END   - Callbacks to handle zoom on app window (screenshot) */

static void
_open_app_window(bmp_info_st *st, Evas_Object *bt, Clouseau_Tree_Item *treeit)
{
#define SHOT_HEADER " - Screenshot"
#define SBAR_PAD_X 4
#define SBAR_PAD_Y 2

   Evas_Object *tb, *bg, *lb_size, *hbx, *glayer;

   char s_bar[128];
   char *win_name = malloc(strlen(treeit->name) + strlen(SHOT_HEADER) + 1);
   st->zoom_val = 1.0; /* Init zoom value */
   st->bt = bt;
   st->win = elm_win_add(NULL, "win", ELM_WIN_BASIC);
   sprintf(win_name, "%s%s", treeit->name, SHOT_HEADER);
   elm_win_title_set(st->win, win_name);
   free(win_name);

   bg = elm_bg_add(st->win);
   elm_win_resize_object_add(st->win, bg);
   evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(bg);

   Evas_Object *bx = elm_box_add(st->win);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(bx);

   /* Table to holds bg and scr on top of it */
   tb = elm_table_add(bx);
   elm_box_pack_end(bx, tb);
   evas_object_size_hint_weight_set(tb, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(tb);

   /* Set background to scr in table cell */
   bg = elm_bg_add(tb);
   snprintf(s_bar, sizeof(s_bar), "%s/images/background.png",
         PACKAGE_DATA_DIR);
   elm_bg_file_set(bg, s_bar, NULL);
   elm_bg_option_set(bg, ELM_BG_OPTION_TILE);
   evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(bg, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(bg);
   elm_table_pack(tb, bg, 0, 0, 1, 1);

   /* Then add the scroller in same cell */
   st->scr = elm_scroller_add(tb);
   elm_table_pack(tb, st->scr, 0, 0, 1, 1);
   evas_object_size_hint_weight_set(st->scr,
         EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

   evas_object_size_hint_align_set(st->scr, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(st->scr);

   st->o = evas_object_image_filled_add(
         evas_object_evas_get(bx));

   evas_object_size_hint_min_set(st->o, st->w, st->h);
   elm_object_content_set(st->scr, st->o);
   elm_object_cursor_set(st->o, ELM_CURSOR_TARGET);

   elm_object_disabled_set(bt, EINA_TRUE);
   evas_object_image_colorspace_set(st->o, EVAS_COLORSPACE_ARGB8888);
   evas_object_image_alpha_set(st->o, EINA_FALSE);
   evas_object_image_size_set(st->o, st->w, st->h);
   evas_object_image_data_copy_set(st->o, st->bmp);
   evas_object_image_data_update_add(st->o, 0, 0, st->w, st->h);
   evas_object_show(st->o);
   evas_object_smart_callback_add(st->win,
         "delete,request", _app_win_del, st);

   /* Build status bar */
   hbx = elm_box_add(bx);
   elm_box_horizontal_set(hbx, EINA_TRUE);
   evas_object_show(hbx);
   elm_box_padding_set(hbx, SBAR_PAD_X, SBAR_PAD_Y);
   evas_object_size_hint_align_set(hbx, 0.0, EVAS_HINT_FILL);
   elm_box_pack_end(bx, hbx);
   lb_size = elm_label_add(hbx);
   sprintf(s_bar, "%llux%llu", st->w, st->h);
   elm_object_text_set(lb_size, s_bar);
   evas_object_show(lb_size);
   elm_box_pack_end(hbx, lb_size);

   st->lb_mouse = elm_label_add(hbx);
   elm_object_text_set(st->lb_mouse, s_bar);
   evas_object_show(st->lb_mouse);
   elm_box_pack_end(hbx, st->lb_mouse);

   st->lb_rgba = elm_label_add(hbx);
   elm_object_text_set(st->lb_rgba, s_bar);
   evas_object_show(st->lb_rgba);
   elm_box_pack_end(hbx, st->lb_rgba);

   evas_object_event_callback_add(st->o, EVAS_CALLBACK_MOUSE_MOVE,
         _mouse_move, st);

   evas_object_event_callback_add(st->o, EVAS_CALLBACK_MOUSE_OUT,
         _mouse_out, st);

   evas_object_event_callback_add(st->o, EVAS_CALLBACK_MOUSE_DOWN,
         clouseau_lines_cb, st);

   evas_object_resize(st->scr, st->w, st->h);
   elm_win_resize_object_add(st->win, bx);
   evas_object_resize(st->win, st->w, st->h);

   elm_win_autodel_set(st->win, EINA_TRUE);
   evas_object_show(st->win);

   /* Attach a gesture layer object to support ZOOM gesture */
   glayer = elm_gesture_layer_add(st->scr);
   elm_gesture_layer_attach(glayer, st->scr);

   /* Reset zoom and remove lines on double click */
   elm_gesture_layer_cb_set(glayer, ELM_GESTURE_N_DOUBLE_TAPS,
         ELM_GESTURE_STATE_END, reset_view, st);

   elm_gesture_layer_cb_set(glayer, ELM_GESTURE_ZOOM,
         ELM_GESTURE_STATE_START, zoom_start, st);
   elm_gesture_layer_cb_set(glayer, ELM_GESTURE_ZOOM,
         ELM_GESTURE_STATE_MOVE, zoom_move, st);
   elm_gesture_layer_cb_set(glayer, ELM_GESTURE_ZOOM,
         ELM_GESTURE_STATE_END, zoom_end, st);
   elm_gesture_layer_cb_set(glayer, ELM_GESTURE_ZOOM,
         ELM_GESTURE_STATE_ABORT, zoom_end, st);
}

static void
_show_app_window(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{  /* Open window with currnent bmp, or download it if missing   */
   app_info_st *st = gui->sel_app->app;
   Clouseau_Tree_Item *treeit = data;

   /* First search app->view list if already have the window bmp */
   bmp_info_st *bmp = (bmp_info_st *)
      eina_list_search_unsorted(st->view, _bmp_object_ptr_cmp,
            (void *) (uintptr_t) treeit->ptr);
   if (bmp)
     return _open_app_window(bmp, obj, data);

   /* Need to issue BMP_REQ */
   if (eet_svr)
     {
        bmp_req_st t = { (unsigned long long) (uintptr_t) NULL,
             (unsigned long long) (uintptr_t) st->ptr,
             (unsigned long long) (uintptr_t) treeit->ptr, st->refresh_ctr };

        ecore_con_eet_send(eet_svr, CLOUSEAU_BMP_REQ_STR, &t);
        elm_object_disabled_set(obj, EINA_TRUE);
        elm_progressbar_pulse(gui->pb, EINA_TRUE);
        evas_object_show(gui->pb);

        Bmp_Node *b_node = malloc(sizeof(*b_node));
        b_node->ctr = st->refresh_ctr;
        b_node->object = (unsigned long long) (uintptr_t) treeit->ptr;
        b_node->bt = obj;       /* Button of BMP_REQ */
        bmp_req = eina_list_append(bmp_req, b_node);
     }
   else  /* Disable button if we lost server */
     _set_button(gui->win, obj,
           SCREENSHOT_MISSING,
           "Screenshot not available", EINA_TRUE);
}

/* START - Callbacks to handle messages from daemon */
void
_app_closed_cb(EINA_UNUSED void *data, EINA_UNUSED Ecore_Con_Reply *reply,
      EINA_UNUSED const char *protocol_name, void *value)
{
   _remove_app(gui, value);
}

void
_app_add_cb(EINA_UNUSED void *data, EINA_UNUSED Ecore_Con_Reply *reply,
      EINA_UNUSED const char *protocol_name, void *value)
{
   _add_app(gui, value);
}

void
_tree_data_cb(EINA_UNUSED void *data, EINA_UNUSED Ecore_Con_Reply *reply,
      EINA_UNUSED const char *protocol_name, void *value)
{  /* Update Tree for app, then update GUI if its displayed */
   tree_data_st *td = value;
   app_info_st *selected = gui->sel_app->app;

   /* Update only if tree is from APP on our list */
   App_Data_St *st = (App_Data_St *)
      eina_list_search_unsorted(apps, _app_ptr_cmp,
            (void *) (uintptr_t) td->app);

   if (st)
     {  /* Free app TREE_DATA then set ptr to new data */
        _free_app_tree_data(st->td);
        st->td = value;

        if (selected->ptr == td->app)
          {  /* Update GUI only if TREE_DATA is from SELECTED app */
             elm_genlist_clear(gui->gl);
             _load_gui_with_list(gui, td->tree);
          }
     }
   else
     {  /* Happens when TREE_DATA of app that already closed has arrived */
        _free_app_tree_data(value);
     }
}

void
_bmp_data_cb(EINA_UNUSED void *data, EINA_UNUSED Ecore_Con_Reply *reply,
      const char *protocol_name, EINA_UNUSED const char *section,
      void *value, size_t length)
{  /* Remove bmp if exists (according to obj-ptr), then add the new one */
   bmp_info_st *st = clouseau_data_packet_info_get(protocol_name,
         value, length);

   st->zoom_val = 1.0; /* Init zoom value */

   App_Data_St *app = (App_Data_St *)
      eina_list_search_unsorted(apps, _app_ptr_cmp,
            (void *) (uintptr_t) st->app);

   /* Check for relevant bmp req in the bmp_req list */
   Bmp_Node *nd = _get_Bmp_Node(st, app->app);

   if (!st->bmp)
     {  /* We consider a case out request will be answered with empty bmp
           this may happen if we have a sub-window of app
           (like checks in elementary test)
           if the user closed it just as we send our BMP_REQ
           this Evas is no longer valid and we get NULL ptr for BMP.
           This code ignores this case. */
        elm_progressbar_pulse(gui->pb, EINA_FALSE);
        evas_object_hide(gui->pb);
        free(st);

        /* Make refresh button display: screenshot NOT available */
        if (nd)
          _set_button(gui->win, nd->bt,
                SCREENSHOT_MISSING,
                "Screenshot not available", EINA_TRUE);
        return;
     }

   if (app && nd)
     {  /* Remove app bmp data if exists, then update */
        elm_progressbar_pulse(gui->pb, EINA_FALSE);
        evas_object_hide(gui->pb);

        app_info_st *info = app->app;
        info->view = _remove_bmp(info->view,
              (void *) (uintptr_t) (st->object));
        info->view = eina_list_append(info->view, st);

        /* Now we need to update refresh button, make it open-window */
        _set_button(gui->win, nd->bt,
              SHOW_SCREENSHOT,
              "Show App Screenshot", EINA_FALSE);

        bmp_req = eina_list_remove(bmp_req, nd);
        free(nd);
     }
   else
     {  /* Dispose bmp info if app no longer in the list of apps */
        /* or the bmp_info is no longer relevant */
        if (st->bmp)
          free(st->bmp);

        free(st);
     }
}

static Eina_Bool
_tree_it_is_elm(Clouseau_Tree_Item *treeit)
{
   Eina_List *l;
   Eo_Dbg_Info *eo_root, *eo;
   Eina_Value_List eo_list;
   clouseau_tree_item_from_legacy_convert(treeit);
   eo_root = treeit->new_eo_info;

   eina_value_pget(&(eo_root->value), &eo_list);

   EINA_LIST_FOREACH(eo_list.list, l, eo)
     {
        if (!strcmp(eo->name, "Elm_Widget"))
           return EINA_TRUE;
     }

   return EINA_FALSE;
}

static void
_gl_exp_add_subitems(Evas_Object *gl, Elm_Object_Item *glit, Clouseau_Tree_Item *parent)
{
   Clouseau_Tree_Item *treeit;
   Eina_List *itr;

   EINA_LIST_FOREACH(parent->children, itr, treeit)
     {
        /* Skip the item if we don't want to show it. */
        if ((!_clouseau_cfg->show_hidden && !treeit->is_visible) ||
              (!_clouseau_cfg->show_clippers && treeit->is_clipper))
           continue;

        if (_clouseau_cfg->show_elm_only && !_tree_it_is_elm(treeit))
          {
             _gl_exp_add_subitems(gl, glit, treeit);
          }
        else
          {
             Elm_Genlist_Item_Type iflag = (treeit->children) ?
                ELM_GENLIST_ITEM_TREE : ELM_GENLIST_ITEM_NONE;
             _tree_item_show_last_expanded_item =
                elm_genlist_item_append(gl, &itc, treeit, glit, iflag,
                      NULL, NULL);
          }
     }
}

static void
gl_exp(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Elm_Object_Item *glit = event_info;
   Evas_Object *gl = elm_object_item_widget_get(glit);
   Clouseau_Tree_Item *parent = elm_object_item_data_get(glit);
   _gl_exp_add_subitems(gl, glit, parent);
}

static void
gl_con(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Elm_Object_Item *glit = event_info;
   elm_genlist_item_subitems_clear(glit);
}

static void
gl_exp_req(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Elm_Object_Item *glit = event_info;
   elm_genlist_item_expanded_set(glit, EINA_TRUE);
}

static void
gl_con_req(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Elm_Object_Item *glit = event_info;
   elm_genlist_item_expanded_set(glit, EINA_FALSE);
}

static Evas_Object *
item_icon_get(void *data, Evas_Object *parent, const char *part)
{
   Clouseau_Tree_Item *treeit = data;
   char buf[PATH_MAX];

   if (!treeit->is_obj)
     {  /* Add "Download" button for evas objects */
        if (!strcmp(part, "elm.swallow.end"))
          {
             Evas_Object *bt = elm_button_add(parent);
             app_info_st *app = NULL;
             if (gui->sel_app)
               app = gui->sel_app->app;

             if (app)
               {  /* match ptr with bmp->object ptr to find view */
                  bmp_info_st *bmp = (bmp_info_st *)
                     eina_list_search_unsorted(app->view, _bmp_object_ptr_cmp,
                           (void *) (uintptr_t) treeit->ptr);

                  if (bmp)
                    {  /* Set to "show view" if view exists */
                       _set_button(parent, bt,
                             SHOW_SCREENSHOT,
                             "Show App Screenshot", EINA_FALSE);
                    }
                  else
                    {  /* Set to Download or not available if offline */
                       if (eet_svr)
                         {
                            _set_button(parent, bt,
                                  TAKE_SCREENSHOT,
                                  "Download Screenshot", EINA_FALSE);
                         }
                       else
                         { /* Make button display: screenshot NOT available */
                            _set_button(parent, bt,
                                  SCREENSHOT_MISSING,
                                  "Screenshot not available", EINA_TRUE);
                         }
                    }
               }

             evas_object_smart_callback_add(bt, "clicked",
                   _show_app_window, treeit);

             evas_object_show(bt);
             return bt;
          }

        return NULL;
     }

   if (!strcmp(part, "elm.swallow.icon"))
     {
        if (treeit->is_clipper && !treeit->is_visible)
          {
             Evas_Object *ic;
             Evas_Object *bx = elm_box_add(parent);
             evas_object_size_hint_aspect_set(bx, EVAS_ASPECT_CONTROL_VERTICAL,
                   1, 1);

             ic = elm_icon_add(bx);
             snprintf(buf, sizeof(buf), "%s/images/clipper.png",
                   PACKAGE_DATA_DIR);
             elm_image_file_set(ic, buf, NULL);
             evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL,
                   1, 1);
             evas_object_size_hint_weight_set(ic, EVAS_HINT_EXPAND,
                   EVAS_HINT_EXPAND);
             evas_object_size_hint_align_set(ic, EVAS_HINT_FILL,
                   EVAS_HINT_FILL);
             elm_box_pack_end(bx, ic);

             ic = elm_icon_add(bx);
             snprintf(buf, sizeof(buf), "%s/images/hidden.png",
                   PACKAGE_DATA_DIR);
             elm_image_file_set(ic, buf, NULL);
             evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL,
                   1, 1);
             evas_object_size_hint_weight_set(ic, EVAS_HINT_EXPAND,
                   EVAS_HINT_EXPAND);
             evas_object_size_hint_align_set(ic, EVAS_HINT_FILL,
                   EVAS_HINT_FILL);
             elm_box_pack_end(bx, ic);

             return bx;

          }
        else if (treeit->is_clipper)
          {
             Evas_Object *ic;
             ic = elm_icon_add(parent);
             snprintf(buf, sizeof(buf), "%s/images/clipper.png",
                   PACKAGE_DATA_DIR);
             elm_image_file_set(ic, buf, NULL);
             evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL,
                   1, 1);
             return ic;
          }
        else if (!treeit->is_visible)
          {
             Evas_Object *ic;
             ic = elm_icon_add(parent);
             snprintf(buf, sizeof(buf), "%s/images/hidden.png",
                   PACKAGE_DATA_DIR);
             elm_image_file_set(ic, buf, NULL);
             evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL,
                   1, 1);
             return ic;
          }
     }

   return NULL;
}

static char *
item_text_get(void *data, Evas_Object *obj EINA_UNUSED,
      const char *part EINA_UNUSED)
{
   Clouseau_Tree_Item *treeit = data;
   char buf[256];
   snprintf(buf, sizeof(buf), "%s %llx", treeit->name, treeit->ptr);
   return strdup(buf);
}

static void
client_win_del(void *data EINA_UNUSED,
      Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{  /* called when client window is deleted */
   elm_exit(); /* exit the program's main loop that runs in elm_run() */
}

static Eina_Bool
_connect_to_daemon(Gui_Elements *g)
{
   if (eet_svr)
     return EINA_TRUE;

   int port = PORT;
   char *address = LOCALHOST;
   char *p_colon = NULL;
   Ecore_Con_Server *server;
   Ecore_Con_Eet *ece = NULL;

   if (g->address && strlen(g->address))
     {
        address = g->address;
        p_colon = strchr(g->address, ':');
     }

   if (p_colon)
     {
        *p_colon = '\0';
        if (isdigit(*(p_colon+1)))
          port = atoi(p_colon+1);
     }

   server = ecore_con_server_connect(ECORE_CON_REMOTE_TCP,
         address, port, NULL);

   if (p_colon)
     *p_colon = ':';

   if (!server)
     {
        ERR("could not connect to the server: %s\n", g->address);
        return EINA_FALSE;
     }

   /* TODO: ecore_con_server_data_size_max_set(server, -1); */

   ece = ecore_con_eet_client_new(server);
   if (!ece)
     {
        ERR("could not connect to the server: %s\n", g->address);
        return EINA_FALSE;
     }

   clouseau_register_descs(ece);

   /* Register callbacks for ecore_con_eet */
   ecore_con_eet_server_connect_callback_add(ece, _add, NULL);
   ecore_con_eet_server_disconnect_callback_add(ece, _del, NULL);
   ecore_con_eet_data_callback_add(ece, CLOUSEAU_APP_CLOSED_STR,
         _app_closed_cb, NULL);
   ecore_con_eet_data_callback_add(ece, CLOUSEAU_APP_ADD_STR,
         _app_add_cb, NULL);
   ecore_con_eet_data_callback_add(ece, CLOUSEAU_TREE_DATA_STR,
         _tree_data_cb, NULL);

   /* At the moment our only raw-data packet is BMP info */
   ecore_con_eet_raw_data_callback_add(ece, CLOUSEAU_BMP_DATA_STR,
         _bmp_data_cb, NULL);

   return EINA_TRUE;
}

static void
_gl_selected(void *data, Evas_Object *pobj EINA_UNUSED, void *event_info)
{
   Gui_Elements *g = data;
   Clouseau_Tree_Item *treeit = elm_object_item_data_get(event_info);
   const Elm_Object_Item *parent;
   const Elm_Object_Item *prt = elm_genlist_item_parent_get(event_info);

   if (!prt)
     {
        g->gl_it = NULL;
        return;
     }

   /* Populate object information, then do highlight */
   if (g->gl_it != event_info)
     {
        elm_genlist_clear(prop_list);
        clouseau_object_information_list_populate(treeit);
        g->gl_it = event_info;

          {
             /* Fetch properties of eo object */
             Eina_List *expand_list = NULL, *l, *l_prev;
             Elm_Object_Item *eo_it;

             /* Populate the property list. */
               {
                  Eo_Dbg_Info *eo_root, *eo;
                  Eina_Value_List eo_list;
                  /* FIXME: Do it before and save it like that. Probably at the
                   * eet conversion stage. */
                  clouseau_tree_item_from_legacy_convert(treeit);
                  eo_root = treeit->new_eo_info;

                  eina_value_pget(&(eo_root->value), &eo_list);

                  EINA_LIST_FOREACH(eo_list.list, l, eo)
                    {
                       Elm_Genlist_Item_Type iflag = (eina_value_type_get(&(eo->value)) == EINA_VALUE_TYPE_LIST) ?
                          ELM_GENLIST_ITEM_TREE : ELM_GENLIST_ITEM_NONE;
                       // We force the item to be a tree for the class layers
                       eo_it = elm_genlist_item_append(prop_list, &_class_info_itc, eo, NULL,
                             iflag, _gl_selected, NULL);
                       expand_list = eina_list_append(expand_list, eo_it);
                    }
               }
             EINA_LIST_REVERSE_FOREACH_SAFE(expand_list, l, l_prev, eo_it)
               {
                  elm_genlist_item_expanded_set(eo_it, EINA_TRUE);
                  expand_list = eina_list_remove_list(expand_list, l);
               }
          }
     }

   if (!do_highlight)
     return;

   /* START - replacing libclouseau_highlight(obj); */
   app_info_st *app = g->sel_app->app;
   highlight_st st = { (unsigned long long) (uintptr_t) app->ptr,
                       treeit->ptr };

   if (eet_svr)
     {
        ecore_con_eet_send(eet_svr, CLOUSEAU_HIGHLIGHT_STR, &st);
     }

   /* We also like to HIGHLIGHT on any app views that open (for offline) */
   do
     {
        parent = prt;
        prt = elm_genlist_item_parent_get(prt);
     }
   while (prt);

   Clouseau_Tree_Item *t = elm_object_item_data_get(parent);
   bmp_info_st *bmp = eina_list_search_unsorted(app->view,
                                             _bmp_object_ptr_cmp,
                                             (void*) (uintptr_t) t->ptr);

   if (bmp && bmp->win)
     {  /* Third param gives evas surface when running offline */
        clouseau_data_object_highlight((void*) (uintptr_t) treeit->ptr,
                                  &treeit->info->evas_props, bmp);
     }
   /* END   - replacing clouseau_object_highlight(obj); */
}

static void
_load_list(Gui_Elements *g)
{
   elm_progressbar_pulse(g->pb, EINA_FALSE);
   evas_object_hide(g->pb);

   if (g->sel_app)
     {
        elm_genlist_clear(g->gl);
        elm_genlist_clear(g->prop_list);
        app_info_st *st = g->sel_app->app;
        tree_data_st *td = (g->sel_app->td) ? g->sel_app->td : NULL;

        if (td)
          {  /* Just show currnet tree we got */
             _load_gui_with_list(g, td->tree);
          }
        else
          {  /* Ask for app info only if was not fetched */
             if (!eet_svr)
               {
                  _update_tree_offline(g, g->sel_app->td);
                  return;
               }

             if (eina_list_search_unsorted(apps, _app_ptr_cmp,
                      (void *) (uintptr_t) st->ptr))
               {  /* do it only if app selected AND found in apps list */
                  data_req_st t = { (unsigned long long) (uintptr_t) NULL,
                       (unsigned long long) (uintptr_t) st->ptr };

                  ecore_con_eet_send(eet_svr, CLOUSEAU_DATA_REQ_STR, &t);
                  elm_progressbar_pulse(g->pb, EINA_TRUE);
                  evas_object_show(g->pb);
               }
          }
     }
}

static void
_highlight_check_check_changed(EINA_UNUSED void *data, Evas_Object *obj,
      void *event_info EINA_UNUSED)
{
   do_highlight = elm_check_state_get(obj);
}

static void
_bt_clicked(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Gui_Elements *g = data;

   /* If there's a currently selected item, try to reopening it. */
   if (g->gl_it)
     {
        Clouseau_Tree_Item *treeit = elm_object_item_data_get(g->gl_it);
        g->jump_to_ptr = (treeit) ? (uintptr_t) treeit->ptr : 0;
     }

   /* Close all app-bmp-view windows here and clear mem */
   if (g->sel_app)
     {
        app_info_st *st = g->sel_app->app;
        _close_app_views(st, EINA_TRUE);
        st->refresh_ctr++;
     }

   _free_app_tree_data(g->sel_app->td);
   g->sel_app->td = NULL;
   g->gl_it = NULL;
   _load_list(data);
}

static void
_bt_load_file(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   if (event_info)
     {
        Gui_Elements *g = data;
        app_info_st *app = calloc(1, sizeof(*app));
        tree_data_st *td =  calloc(1, sizeof(*td));
        Eina_Bool s = clouseau_data_eet_info_read(event_info,
              (app_info_st **) &app, (tree_data_st **) &td);

        if (s)
          {  /* Add the app to list of apps, then set this as selected app */
             app->file = strdup(event_info);
             App_Data_St *st = _add_app(g, app);
             st->td = td;  /* This is the same as we got TREE_DATA message */
             _set_selected_app(st, g->dd_list, NULL);
          }
     }
}

static void
_dismiss_save_dialog(void *data,
      Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{  /* Just close save file save_inwin, do nothing */
   Gui_Elements *g = data;
   evas_object_del(g->save_inwin);
   g->save_inwin = NULL;
}

static void
_bt_save_file(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   app_info_st *app = gui->sel_app->app;
   tree_data_st *ftd = gui->sel_app->td;
   if (event_info)
     {
        /* FIXME: Handle failure. */
        Eina_List *bmp_ck_list  = elm_box_children_get(data);

        clouseau_data_eet_info_save(event_info, app, ftd, bmp_ck_list);
        eina_list_free(bmp_ck_list);
     }


   if (event_info)  /* Dismiss save dialog after saving */
     _dismiss_save_dialog(gui, NULL, NULL);
}

static void
_dismiss_inwin(Gui_Elements *g)
{
   g->address = (g->en) ? strdup(elm_entry_entry_get(g->en)) : NULL;
   evas_object_del(g->connect_inwin);
   g->en = NULL;
   g->connect_inwin = NULL;
}

static void
_save_all(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Evas_Object *ck_bx = data;
   Evas_Object *ck;
   Eina_List *l;
   Eina_List *ck_list = elm_box_children_get(ck_bx);
   Eina_Bool val = elm_check_state_get(obj);

   EINA_LIST_FOREACH(ck_list, l, ck)
     {  /* Run through checkoxes, set / unset marks for all */
        if (!elm_object_disabled_get(ck))
          elm_check_state_set(ck, val);
     }

   eina_list_free(ck_list);
}

static Eina_Bool
_tree_item_show_item(Elm_Object_Item *git, Eina_List *item_list)
{
   if (eina_list_data_get(item_list) == elm_object_item_data_get(git))
     {
        item_list = eina_list_next(item_list);
        if (item_list)
          {
             Elm_Object_Item *gitc;
             _tree_item_show_last_expanded_item = NULL;
             elm_genlist_item_expanded_set(git, EINA_FALSE);
             elm_genlist_item_expanded_set(git, EINA_TRUE);
             gitc = _tree_item_show_last_expanded_item;

             while (gitc && (gitc != git))
               {
                  if (_tree_item_show_item(gitc, item_list))
                     return EINA_TRUE;
                  gitc = elm_genlist_item_prev_get(gitc);
               }
          }
        else
          {
             elm_genlist_item_bring_in(git, ELM_GENLIST_ITEM_SCROLLTO_MIDDLE);
             elm_genlist_item_selected_set(git, EINA_TRUE);
             return EINA_TRUE;
          }
     }

   return EINA_FALSE;
}

static void
_tree_item_show(Evas_Object *tree_genlist, Eina_List *item_list)
{
   Elm_Object_Item *git = elm_genlist_first_item_get(tree_genlist);
   while (git)
     {
        if (_tree_item_show_item(git, item_list))
           break;
        git = elm_genlist_item_next_get(git);
     }

}

static Eina_List *
_tree_item_pointer_find(Clouseau_Tree_Item *treeit, uintptr_t ptr)
{
   Eina_List *l;

   /* Mark that we found the item, and start adding. */
   if (treeit->ptr == ptr)
      return eina_list_prepend(NULL, NULL);

   EINA_LIST_FOREACH(treeit->children, l, treeit)
     {
        Eina_List *found;
        if ((found = _tree_item_pointer_find(treeit, ptr)))
          {
             if (!eina_list_data_get(found))
               {
                  eina_list_free(found);
                  found = NULL;
               }
             return eina_list_prepend(found, treeit);
          }
     }

   return NULL;
}

static Eina_List *
_list_tree_item_pointer_find(Eina_List *tree, uintptr_t ptr)
{
   Clouseau_Tree_Item *treeit;
   Eina_List *l;
   EINA_LIST_FOREACH(tree, l, treeit)
     {
        Eina_List *found;
        if ((found = _tree_item_pointer_find(treeit, ptr)))
          {
             found = eina_list_prepend(found, treeit);
             return found;
          }
     }

   return NULL;
}

/* Load/unload modules. */

static Eina_List *_client_modules = NULL;

static void
_modules_load_from_path(const char *path)
{
   Eina_Array *modules = NULL;

   modules = eina_module_list_get(modules, path, EINA_TRUE, NULL, NULL);
   if (modules)
     {
        eina_module_list_load(modules);

        _client_modules = eina_list_append(_client_modules, modules);
     }
}

#define MODULES_POSTFIX PACKAGE "/modules/client"

static void
_modules_init(void)
{
   char *path;

   path = eina_module_environment_path_get("HOME", "/." MODULES_POSTFIX);
   _modules_load_from_path(path);
   free(path);

   path = PACKAGE_LIB_DIR "/" MODULES_POSTFIX;
   _modules_load_from_path(path);
}

static void
_modules_shutdown(void)
{
   Eina_Array *module_list;

   EINA_LIST_FREE(_client_modules, module_list)
      eina_module_list_free(module_list);
}

static void
_run_module_btn_clicked(void *data,
      Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Eina_Module *module = data;
   tree_data_st *td = NULL;
 
   if (gui && gui->sel_app)
      td = (gui->sel_app->td) ? gui->sel_app->td : NULL;

   if (td)
     {
        void (*module_run)(Eina_List *) = eina_module_symbol_get(module, "clouseau_client_module_run");

        module_run(td->tree);
     }
   else
     {
        ERR("No selected apps!");
     }
}

static Eina_Bool
_module_name_get_cb(const void *container EINA_UNUSED, void *data, void *fdata)
{
   Evas_Object *box = fdata;
   Eina_Module *module = data;
   Evas_Object *btn = NULL;

   const char **name = eina_module_symbol_get(module, "clouseau_module_name");
   if (name)
     {
        btn = elm_button_add(box);
        elm_object_text_set(btn, *name);
        evas_object_smart_callback_add(btn, "clicked", _run_module_btn_clicked, module);
        elm_box_pack_end(box, btn);
        evas_object_show(btn);
     }

   return EINA_TRUE;
}

static void
_popup_close_clicked_cb(void *data,
      Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   evas_object_del(data);
}

static void
_extensions_btn_clicked(void *data EINA_UNUSED,
      Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Eina_List *itr;
   Eina_Array *module_list;
   Evas_Object *popup, *box, *btn;

   popup = elm_popup_add(gui->win);
   elm_object_part_text_set(popup, "title,text", "Run Extensions");
   evas_object_show(popup);

   box = elm_box_add(popup);
   elm_object_content_set(popup, box);
   evas_object_show(box);

   EINA_LIST_FOREACH(_client_modules, itr, module_list)
     {
        eina_array_foreach(module_list, _module_name_get_cb, box);
     }

   btn = elm_button_add(box);
   elm_object_text_set(btn, "Close");
   evas_object_smart_callback_add(btn, "clicked", _popup_close_clicked_cb, popup);
   evas_object_size_hint_align_set(btn, 1.0, 0.5);
   elm_box_pack_end(box, btn);
   evas_object_show(btn);
}

static void
_settings_btn_clicked(void *data EINA_UNUSED,
      Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   clouseau_settings_dialog_open(gui->win,
         (Clouseau_Config_Changed_Cb) _load_list, (void *) gui);
}

static void
_save_file_dialog(void *data,
      Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{  /* START - Popup to save eet file */
   Gui_Elements *g = data;
   Evas_Object *scr, *bt_bx, *bx, *ck_bx,
               *lb, *ck, *bt_cancel, *bt_save;
   g->save_inwin = elm_win_inwin_add(g->win);
   evas_object_show(g->save_inwin);


   bx = elm_box_add(g->save_inwin);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(bx);

   lb = elm_label_add(bx);
   evas_object_size_hint_weight_set(lb, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(lb, EVAS_HINT_FILL, 0.0);
   elm_object_text_set(lb, "Select Screeenshots to save:");
   elm_box_pack_end(bx, lb);
   evas_object_show(lb);

   /* Add checkboxes to select screenshots to save */
   ck_bx = elm_box_add(g->save_inwin);

   Eina_List *l;
   app_info_st *a = g->sel_app->app;
   tree_data_st *td = g->sel_app->td;
   Clouseau_Tree_Item *treeit;
   char buf[256];
   EINA_LIST_FOREACH(td->tree, l, treeit)
     {  /* First search app->view list if already have the window bmp */
        bmp_info_st *bmp = (bmp_info_st *)
           eina_list_search_unsorted(a->view, _bmp_object_ptr_cmp,
                 (void *) (uintptr_t) treeit->ptr);

        ck = elm_check_add(ck_bx);
        evas_object_size_hint_weight_set(ck, EVAS_HINT_EXPAND, 1.0);
        evas_object_size_hint_align_set(ck, EVAS_HINT_FILL, 0.0);
        elm_box_pack_end(ck_bx, ck);
        elm_object_disabled_set(ck, !(bmp && bmp->bmp));
        evas_object_data_set(ck, BMP_FIELD, bmp); /* Associate ck with bmp */
        snprintf(buf, sizeof(buf), "%llx %s", treeit->ptr, treeit->name);
        elm_object_text_set(ck, buf);

        evas_object_show(ck);
     }

   evas_object_show(ck_bx);
   scr = elm_scroller_add(bx);
   elm_object_content_set(scr, ck_bx);
   evas_object_size_hint_align_set(scr, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(scr, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(scr);
   elm_box_pack_end(bx, scr);

   /* Add the save all checkbox */
   ck = elm_check_add(bx);
   elm_object_text_set(ck, "Save All");
   evas_object_smart_callback_add(ck, "changed", _save_all, ck_bx);
   evas_object_show(ck);
   elm_box_pack_end(bx, ck);

   bt_bx = elm_box_add(bx);
   elm_box_horizontal_set(bt_bx, EINA_TRUE);
   elm_box_homogeneous_set(bt_bx, EINA_TRUE);
   evas_object_size_hint_align_set(bt_bx, 0.5, 1.0);
   evas_object_size_hint_weight_set(bt_bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_show(bt_bx);
   elm_box_pack_end(bx, bt_bx);

   /* Add the cancel button */
   bt_cancel = elm_button_add(bt_bx);
   elm_object_text_set(bt_cancel, "Cancel");
   evas_object_smart_callback_add(bt_cancel, "clicked",
         _dismiss_save_dialog, g);

   elm_box_pack_end(bt_bx, bt_cancel);
   evas_object_show(bt_cancel);

   /* Add the Save fileselector button */
   bt_save = elm_fileselector_button_add(bt_bx);
   elm_fileselector_button_is_save_set(bt_save, EINA_TRUE);
   elm_object_text_set(bt_save, "Save File");
   elm_fileselector_button_path_set(bt_save, getenv("HOME"));
   evas_object_smart_callback_add(bt_save, "file,chosen",
         _bt_save_file, ck_bx);

   elm_box_pack_end(bt_bx, bt_save);
   evas_object_show(bt_save);

   elm_win_inwin_content_set(g->save_inwin, bx);
   /* END   - Popup to save eet file */
}

static void
_remove_apps_with_no_tree_data(Gui_Elements *g)
{  /* We need to remove apps with no tree data when losing commection
    * with daemon. We may have apps in our list that were added but
    * tree-data was NOT loaded.
    * In this case, we want to remove them if connection was lost.    */

   Eina_List *l, *l_next;
   App_Data_St *st;
   app_closed_st t;
   EINA_LIST_FOREACH_SAFE(apps, l, l_next, st)
     {
        if (!st->td)
          {  /* We actually fake APP_CLOSED message, for app NO tree */
             t.ptr = (unsigned long long) (uintptr_t)
                (((app_info_st *) st->app)->ptr);

             _remove_app(g, &t);
          }
     }
}

static void
_show_gui(Gui_Elements *g, Eina_Bool work_offline)
{
   if (work_offline)
     {  /* Replace bt_load with fileselector button */
        _titlebar_string_set(g, EINA_FALSE);
        elm_box_unpack(g->hbx, g->bt_load);
        evas_object_del(g->bt_load);

        /* We need this in case conneciton closed and no tree data */
        _remove_apps_with_no_tree_data(g);

        g->bt_load = elm_fileselector_button_add(g->hbx);
        elm_box_pack_start(g->hbx, g->bt_load);
        elm_object_text_set(g->bt_load, "Load File");
        elm_fileselector_button_path_set(g->bt_load, getenv("HOME"));
        evas_object_smart_callback_add(g->bt_load, "file,chosen",
              _bt_load_file, g);

        evas_object_show(g->bt_load);
     }
   else
     {
        elm_object_text_set(g->bt_load, "Reload");
        evas_object_smart_callback_add(g->bt_load, "clicked", _bt_clicked, g);

        /* Add the Save button to open save dialog */
        if (g->bt_save)
          evas_object_del(g->bt_save);

        g->bt_save = elm_button_add(g->hbx);
        elm_object_text_set(g->bt_save, "Save");
        evas_object_smart_callback_add(g->bt_save, "clicked",
              _save_file_dialog, (void *) gui);

        elm_box_pack_end(g->hbx, g->bt_save);
        evas_object_show(g->bt_save);

        elm_object_disabled_set(g->bt_load, (g->sel_app == NULL));
        elm_object_disabled_set(g->bt_save, (g->sel_app == NULL));
        evas_object_show(g->bt_save);

        if (!_connect_to_daemon(g))
          {
             ERR("Failed to connect to server.\n");
             elm_exit(); /* exit the program's main loop,runs in elm_run() */
          }
     }

   evas_object_show(g->bx);
}

static void
_cancel_bt_clicked(void *data,
      Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   _dismiss_inwin(data);
   elm_exit(); /* exit the program's main loop that runs in elm_run() */
}

static void
_ok_bt_clicked(void *data,
      Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{  /* Set the IP, PORT, then connect to server */
   _dismiss_inwin(data);
   _show_gui(data, EINA_FALSE);
}

static void
_ofl_bt_clicked(void *data,
      Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{  /* Disbale entry when working offline */
   _dismiss_inwin(data);
   _show_gui(data, EINA_TRUE);
}

static void
_jump_to_ptr(Gui_Elements *g, uintptr_t ptr)
{
   tree_data_st *td = (g->sel_app->td) ? g->sel_app->td : NULL;
   Eina_List *found = NULL;

   if (td && (found = _list_tree_item_pointer_find(td->tree, (uintptr_t) ptr)))
     {
        _tree_item_show(g->gl, found);
        eina_list_free(found);
     }
}

static void
_jump_to_entry_activated(void *data,
      Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Gui_Elements *g = data;
   long long unsigned int ptr = strtoul(elm_object_text_get(obj), NULL, 16);

   _jump_to_ptr(g, ptr);
}

static void
_control_buttons_create(Gui_Elements *g, Evas_Object *win)
{
   Evas_Object *highlight_check;
   Evas_Object *jump_to_entry, *frame;

   frame = elm_frame_add(gui->bx);
   elm_object_style_set(frame, "pad_medium");
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(g->bx, frame);
   evas_object_show(frame);
   
   g->hbx = elm_box_add(g->bx);
   evas_object_size_hint_align_set(g->hbx, 0.0, 0.5);
   elm_box_horizontal_set(g->hbx, EINA_TRUE);
   elm_object_content_set(frame, g->hbx);
   elm_box_padding_set(g->hbx, 4, 0);
   evas_object_size_hint_align_set(g->hbx, EVAS_HINT_FILL, 0.0);
   evas_object_size_hint_weight_set(g->hbx, EVAS_HINT_EXPAND, 0.0);
   evas_object_show(g->hbx);

   g->bt_load = elm_button_add(g->hbx);
   evas_object_size_hint_align_set(g->bt_load, 0.0, 0.3);
   elm_box_pack_end(g->hbx, g->bt_load);
   evas_object_show(g->bt_load);

   g->dd_list = elm_hoversel_add(g->hbx);
   elm_hoversel_hover_parent_set(g->dd_list, win);
   elm_object_text_set(g->dd_list, SELECT_APP_TEXT);

   evas_object_size_hint_align_set(g->dd_list, 0.0, 0.3);
   elm_box_pack_end(g->hbx, g->dd_list);
   evas_object_show(g->dd_list);

   highlight_check = elm_check_add(g->hbx);
   elm_object_text_set(highlight_check , "Highlight");
   elm_check_state_set(highlight_check , do_highlight);
   elm_box_pack_end(g->hbx, highlight_check);
   evas_object_show(highlight_check);

   evas_object_smart_callback_add(highlight_check, "changed",
                                  _highlight_check_check_changed, g);

   jump_to_entry = elm_entry_add(g->hbx);
   elm_entry_scrollable_set(jump_to_entry, EINA_TRUE);
   elm_entry_single_line_set(jump_to_entry, EINA_TRUE);
   elm_object_part_text_set(jump_to_entry, "guide", "Jump To Pointer");
   evas_object_size_hint_align_set(jump_to_entry,
                                   EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(jump_to_entry,
                                    EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_box_pack_end(g->hbx, jump_to_entry);
   evas_object_show(jump_to_entry);

   evas_object_smart_callback_add(jump_to_entry, "activated",
                                  _jump_to_entry_activated, g);

   Evas_Object *btn_extensions;

   btn_extensions = elm_button_add(g->hbx);
   elm_object_text_set(btn_extensions, "Extensions");
   evas_object_smart_callback_add(btn_extensions, "clicked",
         _extensions_btn_clicked, NULL);
   elm_box_pack_end(g->hbx, btn_extensions);
   evas_object_show(btn_extensions);

   Evas_Object *btn_settings;

   btn_settings = elm_button_add(g->hbx);
   elm_object_text_set(btn_settings, "Settings");
   evas_object_smart_callback_add(btn_settings, "clicked",
         _settings_btn_clicked, NULL);
   elm_box_pack_end(g->hbx, btn_settings);
   evas_object_show(btn_settings);
}

static void
_main_list_create(Evas_Object *panes)
{
   gui->gl = elm_genlist_add(panes);
   elm_genlist_select_mode_set(gui->gl, ELM_OBJECT_SELECT_MODE_ALWAYS);
   evas_object_size_hint_align_set(gui->gl,
                                   EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(gui->gl,
                                    EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_object_part_content_set(panes, "left", gui->gl);
   evas_object_show(gui->gl);

   itc.item_style = "default";
   itc.func.text_get = item_text_get;
   itc.func.content_get = item_icon_get;
   itc.func.state_get = NULL;
   itc.func.del = NULL;

   evas_object_smart_callback_add(gui->gl,
                                  "expand,request", gl_exp_req, gui->gl);
   evas_object_smart_callback_add(gui->gl,
                                  "contract,request", gl_con_req, gui->gl);
   evas_object_smart_callback_add(gui->gl,
                                  "expanded", gl_exp, gui->gl);
   evas_object_smart_callback_add(gui->gl,
                                  "contracted", gl_con, gui->gl);
   evas_object_smart_callback_add(gui->gl,
                                  "selected", _gl_selected, gui);
}

static void
_obj_info_compactable_list_to_buffer(Eo_Dbg_Info *root_eo, char* buffer, unsigned int buffer_size)
{
   Eina_List *l; // Iterator
   Eina_Value_List list; // list of the elements in root_eo
   eina_value_pget(&(root_eo->value), &list);
   Eo_Dbg_Info *eo; // List element
   buffer += snprintf(buffer, buffer_size, "%s:", root_eo->name);
   EINA_LIST_FOREACH(list.list, l, eo)
     {
        char *strval = eina_value_to_string(&(eo->value));
        buffer += snprintf(buffer, buffer_size, "   %s: %s", eo->name, strval);
        free(strval);
     }
}

static Eina_Bool
_obj_info_can_list_be_compacted(Eo_Dbg_Info *root_eo)
{
   Eina_List *l; // Iterator
   Eina_Value_List list; // list of the elements in root_eo
   Eo_Dbg_Info *eo; // List element
   eina_value_pget(&(root_eo->value), &list);
   // We check that there is no list into this list. If such list exists,
   // we can't compact the list.
   EINA_LIST_FOREACH(list.list, l, eo)
     {
        if (eina_value_type_get(&(eo->value)) == EINA_VALUE_TYPE_LIST)
           return EINA_FALSE;
     }
   return EINA_TRUE;
}

static void
_obj_info_gl_selected(void *data EINA_UNUSED, Evas_Object *pobj EINA_UNUSED,
      void *event_info EINA_UNUSED)
{
   /* Currently do nothing */
   return;
}

static void
_obj_info_gl_exp(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Elm_Object_Item *glit = event_info;

     {
        Eina_List *l;
        Eina_Value_List eo_list;
        Eo_Dbg_Info *eo_root, *eo;
        eo_root = elm_object_item_data_get(glit);
        eina_value_pget(&(eo_root->value), &eo_list);

        EINA_LIST_FOREACH(eo_list.list, l, eo)
          {
             Elm_Genlist_Item_Type iflag = ELM_GENLIST_ITEM_NONE;
             if (eina_value_type_get(&(eo->value)) == EINA_VALUE_TYPE_LIST)
               {
                  if (!_obj_info_can_list_be_compacted(eo))
                     iflag = ELM_GENLIST_ITEM_TREE;
               }
             elm_genlist_item_append(prop_list, &_obj_info_itc, eo, glit,
                   iflag, _obj_info_gl_selected, NULL);
          }
     }
}

static void
_obj_info_gl_con(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Elm_Object_Item *glit = event_info;
   elm_genlist_item_subitems_clear(glit);
}

static void
_obj_info_gl_exp_req(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Elm_Object_Item *glit = event_info;
   elm_genlist_item_expanded_set(glit, EINA_TRUE);
}

static void
_obj_info_gl_con_req(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Elm_Object_Item *glit = event_info;
   elm_genlist_item_expanded_set(glit, EINA_FALSE);
}

static Evas_Object *
_obj_info_gl_item_icon_get(void *data EINA_UNUSED, Evas_Object *parent EINA_UNUSED,
      const char *part EINA_UNUSED)
{
   return NULL;
}

static char *
_obj_info_gl_item_text_get(void *data, Evas_Object *obj EINA_UNUSED,
      const char *part EINA_UNUSED)
{
   Eo_Dbg_Info *eo = data;
   char buf[1024] = "";
   if (eina_value_type_get(&(eo->value)) == EINA_VALUE_TYPE_LIST)
     {
        if (_obj_info_can_list_be_compacted(eo))
           _obj_info_compactable_list_to_buffer(eo, buf, sizeof(buf));
        else
           snprintf(buf, sizeof(buf), "%s", eo->name);
     }
   else if (eina_value_type_get(&(eo->value)) == EINA_VALUE_TYPE_UINT64)
     {
        /* We treat UINT64 as a pointer. */

        uint64_t ptr = 0;
        eina_value_get(&(eo->value), &ptr);
        snprintf(buf, sizeof(buf), "%s: %llx", eo->name, (unsigned long long) ptr);
     }
   else
     {
        char *strval = eina_value_to_string(&(eo->value));
        snprintf(buf, sizeof(buf), "%s: %s", eo->name, strval);
        free(strval);
     }

   return strdup(buf);
}

// Classes are not displayed in the same way as infos.
// Infos lists can be compacted, not class infos.
static char *
_class_info_gl_item_text_get(void *data, Evas_Object *obj EINA_UNUSED,
      const char *part EINA_UNUSED)
{
   Eo_Dbg_Info *eo = data;
   return strdup(eo->name);
}

static Evas_Object *
_clouseau_object_information_list_add(Evas_Object *parent)
{
   prop_list = elm_genlist_add(parent);

   _class_info_itc.item_style = "default";
   _class_info_itc.func.text_get = _class_info_gl_item_text_get;
   _class_info_itc.func.content_get = _obj_info_gl_item_icon_get;
   _class_info_itc.func.state_get = NULL;
   _class_info_itc.func.del = NULL;

   _obj_info_itc.item_style = "default";
   _obj_info_itc.func.text_get = _obj_info_gl_item_text_get;
   _obj_info_itc.func.content_get = _obj_info_gl_item_icon_get;
   _obj_info_itc.func.state_get = NULL;
   _obj_info_itc.func.del = NULL;

   evas_object_smart_callback_add(prop_list, "expand,request", _obj_info_gl_exp_req,
         prop_list);
   evas_object_smart_callback_add(prop_list, "contract,request", _obj_info_gl_con_req,
         prop_list);
   evas_object_smart_callback_add(prop_list, "expanded", _obj_info_gl_exp, prop_list);
   evas_object_smart_callback_add(prop_list, "contracted", _obj_info_gl_con, prop_list);
   evas_object_smart_callback_add(prop_list, "selected", _obj_info_gl_selected, NULL);

   return prop_list;
}

static void
_property_list_create(Evas_Object *panes)
{
   Evas_Object *o= NULL;
   gui->prop_list = o = _clouseau_object_information_list_add(panes);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

   elm_object_part_content_set(panes, "right", o);
   evas_object_show(o);
}

int
main(int argc, char **argv)
{  /* Create Client Window */
   const char *log_dom = "clouseau_client";
   _clouseau_client_log_dom = eina_log_domain_register(log_dom, EINA_COLOR_LIGHTBLUE);
   if (_clouseau_client_log_dom < 0)
     {
        EINA_LOG_ERR("Could not register log domain: %s", log_dom);
        return EINA_FALSE;
     }

   Evas_Object *win, *panes, *frame;

   /* For inwin popup */
   Evas_Object *lb, *bxx, *bt_bx, *bt_ok, *bt_cancel;
   Evas_Object *bt_ofl; /* work_offline button  */
   void *st;

   gui = calloc(1, sizeof(Gui_Elements));

   setenv("ELM_CLOUSEAU", "0", 1);
   elm_init(argc, argv);

   clouseau_cfg_init(PACKAGE_NAME);
   clouseau_cfg_load();

   _modules_init();


   if (argc == 2) gui->address = strdup(argv[1]); // if the user executes the client with ip and port in the arguments line

   gui->win = win = elm_win_util_standard_add("client", CLIENT_NAME);
   elm_win_autodel_set(win, EINA_TRUE);
   _titlebar_string_set(gui, EINA_FALSE);

   gui->bx = elm_box_add(win);
   evas_object_size_hint_weight_set(gui->bx,
         EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(gui->bx, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_win_resize_object_add(win, gui->bx);

   _control_buttons_create(gui, win);
   
   frame = elm_frame_add(gui->bx);
   elm_object_style_set(frame, "pad_medium");
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(gui->bx, frame);
   evas_object_show(frame);
   
   panes = elm_panes_add(gui->bx);
   evas_object_size_hint_weight_set(panes, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(panes, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_content_set(frame, panes);
   evas_object_show(panes);

   _main_list_create(panes);
   _property_list_create(panes);

   /* Add progress wheel */
   gui->pb = elm_progressbar_add(win);
   elm_object_style_set(gui->pb, "wheel");
   elm_object_text_set(gui->pb, "Style: wheel");
   elm_progressbar_pulse_set(gui->pb, EINA_TRUE);
   elm_progressbar_pulse(gui->pb, EINA_FALSE);
   evas_object_size_hint_align_set(gui->pb, 0.5, 0.0);
   evas_object_size_hint_weight_set(gui->pb,
         EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_win_resize_object_add(win, gui->pb);

   /* Resize and show main window */
   evas_object_resize(win, 500, 500);
   evas_object_show(win);

   evas_object_smart_callback_add(win, "delete,request", client_win_del, NULL);

   eina_init();
   ecore_init();
   ecore_con_init();
   clouseau_data_init();

   if (gui->address)
     {
        _show_gui(gui, EINA_FALSE);
     }
   else
     {
        /* START - Popup to get IP, PORT from user */
        gui->connect_inwin = elm_win_inwin_add(win);
        evas_object_show(gui->connect_inwin);

        bxx = elm_box_add(gui->connect_inwin);
        evas_object_size_hint_weight_set(bxx, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_show(bxx);

        lb = elm_label_add(bxx);
        evas_object_size_hint_weight_set(lb, EVAS_HINT_EXPAND, 0.0);
        evas_object_size_hint_align_set(lb, EVAS_HINT_FILL, 0.0);
        elm_object_text_set(lb, "Enter remote address[:port]");
        elm_box_pack_end(bxx, lb);
        evas_object_show(lb);

        /* Single line selected entry */
        gui->en = elm_entry_add(bxx);
        elm_entry_scrollable_set(gui->en, EINA_TRUE);
        evas_object_size_hint_weight_set(gui->en,
              EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(gui->en, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_object_style_set(gui->connect_inwin, "minimal_vertical");
        elm_scroller_policy_set(gui->en, ELM_SCROLLER_POLICY_OFF,
              ELM_SCROLLER_POLICY_OFF);
        elm_object_text_set(gui->en, LOCALHOST);
        elm_entry_single_line_set(gui->en, EINA_TRUE);
        elm_entry_select_all(gui->en);
        evas_object_smart_callback_add(gui->en, "activated", _ok_bt_clicked, (void *)gui);
        elm_box_pack_end(bxx, gui->en);
        evas_object_show(gui->en);

        bt_bx = elm_box_add(bxx);
        elm_box_horizontal_set(bt_bx, EINA_TRUE);
        elm_box_homogeneous_set(bt_bx, EINA_TRUE);
        evas_object_size_hint_align_set(bt_bx, 0.5, 0.5);
        evas_object_size_hint_weight_set(bt_bx, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_show(bt_bx);
        elm_box_pack_end(bxx, bt_bx);

        /* Add the cancel button */
        bt_cancel = elm_button_add(bt_bx);
        elm_object_text_set(bt_cancel, "Cancel");
        evas_object_smart_callback_add(bt_cancel, "clicked",
              _cancel_bt_clicked, (void *) gui);

        elm_box_pack_end(bt_bx, bt_cancel);
        evas_object_show(bt_cancel);

        /* Add the OK button */
        bt_ok = elm_button_add(bt_bx);
        elm_object_text_set(bt_ok, "OK");
        evas_object_smart_callback_add(bt_ok, "clicked",
              _ok_bt_clicked, (void *) gui);

        elm_box_pack_end(bt_bx, bt_ok);
        evas_object_show(bt_ok);

        bt_ofl = elm_button_add(bt_bx);
        elm_object_text_set(bt_ofl, "Work Offline");
        evas_object_smart_callback_add(bt_ofl, "clicked",
              _ofl_bt_clicked, (void *) gui);

        elm_box_pack_end(bt_bx, bt_ofl);
        evas_object_show(bt_ofl);

        elm_win_inwin_content_set(gui->connect_inwin, bxx);
        /* END   - Popup to get IP, PORT from user */
     }

   elm_run();

   /* cleanup - free apps data */
   EINA_LIST_FREE(apps, st)
      _free_app(st);

   EINA_LIST_FREE(bmp_req, st)
      free(st);

   clouseau_data_shutdown();
   if (gui->address)
     free(gui->address);

   free(gui);

   _modules_shutdown();
   clouseau_cfg_save();
   clouseau_cfg_shutdown();
   elm_shutdown();

   return 0;
}
