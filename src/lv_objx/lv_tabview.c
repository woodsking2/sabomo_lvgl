/**
 * @file lv_tab.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_tabview.h"
#if LV_USE_TABVIEW != 0

#include "lv_btnm.h"
#include "../lv_core/lv_debug.h"
#include "../lv_themes/lv_theme.h"
#include "../lv_misc/lv_anim.h"
#include "../lv_core/lv_disp.h"

/*********************
 *      DEFINES
 *********************/
#define LV_OBJX_NAME "lv_tabview"

#if LV_USE_ANIMATION
#ifndef LV_TABVIEW_DEF_ANIM_TIME
#define LV_TABVIEW_DEF_ANIM_TIME 300 /*Animation time of focusing to the a list element [ms] (0: no animation)  */
#endif
#else
#undef LV_TABVIEW_DEF_ANIM_TIME
#define LV_TABVIEW_DEF_ANIM_TIME 0 /*No animations*/
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_res_t lv_tabview_signal(lv_obj_t * tabview, lv_signal_t sign, void * param);
static lv_res_t tabview_scrl_signal(lv_obj_t * tabview_scrl, lv_signal_t sign, void * param);

static void tab_btnm_event_cb(lv_obj_t * tab_btnm, lv_event_t event);
static void tabview_realign(lv_obj_t * tabview);
static void refr_indic_size(lv_obj_t * tabview);
static void refr_btns_size(lv_obj_t * tabview);
static void refr_content_size(lv_obj_t * tabview);
static void refr_align(lv_obj_t * tabview);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_signal_cb_t ancestor_signal;
static lv_signal_cb_t ancestor_scrl_signal;
static lv_signal_cb_t page_signal;
static const char * tab_def[] = {""};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Create a Tab view object
 * @param par pointer to an object, it will be the parent of the new tab
 * @param copy pointer to a tab object, if not NULL then the new object will be copied from it
 * @return pointer to the created tab
 */
lv_obj_t * lv_tabview_create(lv_obj_t * par, const lv_obj_t * copy)
{
    LV_LOG_TRACE("tab view create started");

    /*Create the ancestor of tab*/
    lv_obj_t * new_tabview = lv_obj_create(par, copy);
    LV_ASSERT_MEM(new_tabview);
    if(new_tabview == NULL) return NULL;
    if(ancestor_signal == NULL) ancestor_signal = lv_obj_get_signal_cb(new_tabview);

    /*Allocate the tab type specific extended data*/
    lv_tabview_ext_t * ext = lv_obj_allocate_ext_attr(new_tabview, sizeof(lv_tabview_ext_t));
    LV_ASSERT_MEM(ext);
    if(ext == NULL) {
        lv_obj_del(new_tabview);
        return NULL;
    }

    /*Initialize the allocated 'ext' */
    ext->tab_cur      = 0;
    ext->point_last.x = 0;
    ext->point_last.y = 0;
    ext->content      = NULL;
    ext->indic        = NULL;
    ext->btns         = NULL;
    ext->btns_pos     = LV_TABVIEW_BTNS_POS_TOP;
#if LV_USE_ANIMATION
    ext->anim_time = LV_TABVIEW_DEF_ANIM_TIME;
#endif

    /*The signal and design functions are not copied so set them here*/
    lv_obj_set_signal_cb(new_tabview, lv_tabview_signal);

    /*Init the new tab tab*/
    if(copy == NULL) {
        ext->tab_name_ptr = lv_mem_alloc(sizeof(char *));
        LV_ASSERT_MEM(ext->tab_name_ptr);
        if(ext->tab_name_ptr == NULL) return NULL;
        ext->tab_name_ptr[0] = "";
        ext->tab_cnt         = 0;

        /* Set a size which fits into the parent.
         * Don't use `par` directly because if the tabview is created on a page it is moved to the
         * scrollable so the parent has changed */
        lv_coord_t w;
        lv_coord_t h;
        if(par) {
            w = lv_obj_get_width_fit(lv_obj_get_parent(new_tabview));
            h = lv_obj_get_height_fit(lv_obj_get_parent(new_tabview));
        } else {
            w = lv_disp_get_hor_res(NULL);
            h = lv_disp_get_ver_res(NULL);
        }

        lv_obj_set_size(new_tabview, w, h);

        ext->content = lv_page_create(new_tabview, NULL);
        ext->btns    = lv_btnm_create(new_tabview, NULL);
        ext->indic   = lv_obj_create(ext->btns, NULL);

        if(ancestor_scrl_signal == NULL) ancestor_scrl_signal = lv_obj_get_signal_cb(lv_page_get_scrl(ext->content));
        lv_obj_set_signal_cb(lv_page_get_scrl(ext->content), tabview_scrl_signal);

        lv_btnm_set_map(ext->btns, tab_def);
        lv_obj_set_event_cb(ext->btns, tab_btnm_event_cb);

        lv_obj_set_click(ext->indic, false);

        lv_page_set_style(ext->content, LV_PAGE_STYLE_BG, &lv_style_transp_tight);
        lv_page_set_style(ext->content, LV_PAGE_STYLE_SCRL, &lv_style_transp_tight);
        lv_page_set_scrl_fit2(ext->content, LV_FIT_TIGHT, LV_FIT_FLOOD);
        lv_page_set_scrl_layout(ext->content, LV_LAYOUT_ROW_T);
        lv_page_set_sb_mode(ext->content, LV_SB_MODE_OFF);
        lv_obj_set_drag_dir(lv_page_get_scrl(ext->content), LV_DRAG_DIR_ONE);

        /*Set the default styles*/
        lv_theme_t * th = lv_theme_get_current();
        if(th) {
            lv_tabview_set_style(new_tabview, LV_TABVIEW_STYLE_BG, th->style.tabview.bg);
            lv_tabview_set_style(new_tabview, LV_TABVIEW_STYLE_INDIC, th->style.tabview.indic);
            lv_tabview_set_style(new_tabview, LV_TABVIEW_STYLE_BTN_BG, th->style.tabview.btn.bg);
            lv_tabview_set_style(new_tabview, LV_TABVIEW_STYLE_BTN_REL, th->style.tabview.btn.rel);
            lv_tabview_set_style(new_tabview, LV_TABVIEW_STYLE_BTN_PR, th->style.tabview.btn.pr);
            lv_tabview_set_style(new_tabview, LV_TABVIEW_STYLE_BTN_TGL_REL, th->style.tabview.btn.tgl_rel);
            lv_tabview_set_style(new_tabview, LV_TABVIEW_STYLE_BTN_TGL_PR, th->style.tabview.btn.tgl_pr);
        } else {
            lv_tabview_set_style(new_tabview, LV_TABVIEW_STYLE_BG, &lv_style_plain);
            lv_tabview_set_style(new_tabview, LV_TABVIEW_STYLE_BTN_BG, &lv_style_pretty);//transp);
            lv_tabview_set_style(new_tabview, LV_TABVIEW_STYLE_INDIC, &lv_style_plain_color);
        }
    }
    /*Copy an existing tab view*/
    else {
        lv_tabview_ext_t * copy_ext = lv_obj_get_ext_attr(copy);
        ext->point_last.x           = 0;
        ext->point_last.y           = 0;
        ext->btns                   = lv_btnm_create(new_tabview, copy_ext->btns);
        ext->indic                  = lv_obj_create(ext->btns, copy_ext->indic);
        ext->content                = lv_page_create(new_tabview, copy_ext->content);
#if LV_USE_ANIMATION
        ext->anim_time = copy_ext->anim_time;
#endif

        ext->tab_name_ptr = lv_mem_alloc(sizeof(char *));
        LV_ASSERT_MEM(ext->tab_name_ptr);
        if(ext->tab_name_ptr == NULL) return NULL;
        ext->tab_name_ptr[0] = "";
        lv_btnm_set_map(ext->btns, ext->tab_name_ptr);

        uint16_t i;
        lv_obj_t * new_tab;
        lv_obj_t * copy_tab;
        for(i = 0; i < copy_ext->tab_cnt; i++) {
            new_tab  = lv_tabview_add_tab(new_tabview, copy_ext->tab_name_ptr[i]);
            copy_tab = lv_tabview_get_tab(copy, i);
            lv_page_set_style(new_tab, LV_PAGE_STYLE_BG, lv_page_get_style(copy_tab, LV_PAGE_STYLE_BG));
            lv_page_set_style(new_tab, LV_PAGE_STYLE_SCRL, lv_page_get_style(copy_tab, LV_PAGE_STYLE_SCRL));
            lv_page_set_style(new_tab, LV_PAGE_STYLE_SB, lv_page_get_style(copy_tab, LV_PAGE_STYLE_SB));
        }

        /*Refresh the style with new signal function*/
        lv_obj_refresh_style(new_tabview);
    }

    tabview_realign(new_tabview);

    LV_LOG_INFO("tab view created");

    return new_tabview;
}

/**
 * Delete all children of the scrl object, without deleting scrl child.
 * @param tabview pointer to an object
 */
void lv_tabview_clean(lv_obj_t * tabview)
{
    LV_ASSERT_OBJ(tabview, LV_OBJX_NAME);

    lv_obj_t * scrl = lv_page_get_scrl(tabview);
    lv_obj_clean(scrl);
}

/*======================
 * Add/remove functions
 *=====================*/

/**
 * Add a new tab with the given name
 * @param tabview pointer to Tab view object where to ass the new tab
 * @param name the text on the tab button
 * @return pointer to the created page object (lv_page). You can create your content here
 */
lv_obj_t * lv_tabview_add_tab(lv_obj_t * tabview, const char * name)
{
    LV_ASSERT_OBJ(tabview, LV_OBJX_NAME);
    LV_ASSERT_STR(name);

    lv_tabview_ext_t * ext = lv_obj_get_ext_attr(tabview);

    /*Create the container page*/
    lv_obj_t * h = lv_page_create(ext->content, NULL);
    lv_obj_set_size(h, lv_obj_get_width(tabview), lv_obj_get_height(ext->content));
    lv_page_set_sb_mode(h, LV_SB_MODE_AUTO);
    lv_page_set_style(h, LV_PAGE_STYLE_BG, &lv_style_transp_tight);
    lv_page_set_style(h, LV_PAGE_STYLE_SCRL, &lv_style_transp);
    lv_page_set_scroll_propagation(h, true);

    if(page_signal == NULL) page_signal = lv_obj_get_signal_cb(h);

    /*Extend the button matrix map with the new name*/
    char * name_dm;
    name_dm = lv_mem_alloc(strlen(name) + 1); /*+1 for the the closing '\0' */
    LV_ASSERT_MEM(name_dm);
    if(name_dm == NULL) return NULL;
    strcpy(name_dm, name);

    ext->tab_cnt++;

    switch(ext->btns_pos) {
        case LV_TABVIEW_BTNS_POS_TOP:
        case LV_TABVIEW_BTNS_POS_BOTTOM:
            ext->tab_name_ptr = lv_mem_realloc((void*)ext->tab_name_ptr, sizeof(char *) * (ext->tab_cnt + 1));
            break;
        case LV_TABVIEW_BTNS_POS_LEFT:
        case LV_TABVIEW_BTNS_POS_RIGHT:
            ext->tab_name_ptr = lv_mem_realloc((void*)ext->tab_name_ptr, sizeof(char *) * (ext->tab_cnt * 2));
            break;
    }

    LV_ASSERT_MEM(ext->tab_name_ptr);
    if(ext->tab_name_ptr == NULL) return NULL;

    /* FIXME: It is not possible yet to switch tab button position from/to top/bottom from/to left/right at runtime.
     * Method: clean extra \n when switch from LV_TABVIEW_BTNS_POS_LEFT or LV_TABVIEW_BTNS_POS_RIGHT
     * to LV_TABVIEW_BTNS_POS_TOP or LV_TABVIEW_BTNS_POS_BOTTOM.
     */
    switch(ext->btns_pos) {
    default: /*default case is prevented in lv_tabview_set_btns_pos(), but here for safety*/
    case LV_TABVIEW_BTNS_POS_NONE:
    case LV_TABVIEW_BTNS_POS_TOP:
    case LV_TABVIEW_BTNS_POS_BOTTOM:
        ext->tab_name_ptr = lv_mem_realloc(ext->tab_name_ptr, sizeof(char *) * (ext->tab_cnt + 1));

        LV_ASSERT_MEM(ext->tab_name_ptr);
        if(ext->tab_name_ptr == NULL) return NULL;

        ext->tab_name_ptr[ext->tab_cnt - 1] = name_dm;
        ext->tab_name_ptr[ext->tab_cnt]     = "";

        break;
    case LV_TABVIEW_BTNS_POS_LEFT:
    case LV_TABVIEW_BTNS_POS_RIGHT:
        ext->tab_name_ptr = lv_mem_realloc(ext->tab_name_ptr, sizeof(char *) * (ext->tab_cnt * 2));

        LV_ASSERT_MEM(ext->tab_name_ptr);
        if(ext->tab_name_ptr == NULL) return NULL;

        if(ext->tab_cnt == 1) {
            ext->tab_name_ptr[0] = name_dm;
            ext->tab_name_ptr[1] = "";
        } else {
            ext->tab_name_ptr[ext->tab_cnt * 2 - 3] = "\n";
            ext->tab_name_ptr[ext->tab_cnt * 2 - 2] = name_dm;
            ext->tab_name_ptr[ext->tab_cnt * 2 - 1] = "";
        }
        break;
    }

    /* The button matrix's map still points to the old `tab_name_ptr` which might be freed by
     * `lv_mem_realloc`. So make its current map invalid*/
    lv_btnm_ext_t * btnm_ext = lv_obj_get_ext_attr(ext->btns);
    btnm_ext->map_p          = NULL;

    lv_btnm_set_map(ext->btns, ext->tab_name_ptr);
    lv_btnm_set_btn_ctrl(ext->btns, ext->tab_cur, LV_BTNM_CTRL_NO_REPEAT);

    /*Set the first btn as active*/
    if(ext->tab_cnt == 1)  ext->tab_cur = 0;

    tabview_realign(tabview); /*Set the size of the pages, tab buttons and indicator*/

    lv_tabview_set_tab_act(tabview, ext->tab_cur, false);

    return h;
}

/*=====================
 * Setter functions
 *====================*/

/**
 * Set a new tab
 * @param tabview pointer to Tab view object
 * @param id index of a tab to load
 * @param anim LV_ANIM_ON: set the value with an animation; LV_ANIM_OFF: change the value immediately
 */
void lv_tabview_set_tab_act(lv_obj_t * tabview, uint16_t id, lv_anim_enable_t anim)
{
    LV_ASSERT_OBJ(tabview, LV_OBJX_NAME);

#if LV_USE_ANIMATION == 0
    anim = LV_ANIM_OFF;
#endif
    lv_tabview_ext_t * ext = lv_obj_get_ext_attr(tabview);

    const lv_style_t * cont_style = lv_obj_get_style(ext->content);
    const lv_style_t * cont_scrl_style = lv_obj_get_style(lv_page_get_scrl(ext->content));

    if(id >= ext->tab_cnt) id = ext->tab_cnt - 1;

    lv_btnm_clear_btn_ctrl(ext->btns, ext->tab_cur, LV_BTNM_CTRL_TGL_STATE);

    ext->tab_cur = id;

    if(lv_obj_get_base_dir(tabview) == LV_BIDI_DIR_RTL) {
        id = (ext->tab_cnt - (id + 1));
    }

    lv_coord_t cont_x;

    switch(ext->btns_pos) {
    default: /*default case is prevented in lv_tabview_set_btns_pos(), but here for safety*/
    case LV_TABVIEW_BTNS_POS_NONE:
    case LV_TABVIEW_BTNS_POS_TOP:
    case LV_TABVIEW_BTNS_POS_BOTTOM:
        cont_x = -(lv_obj_get_width(tabview) * id + cont_scrl_style->body.padding.inner * id + cont_scrl_style->body.padding.left);
        break;
    case LV_TABVIEW_BTNS_POS_LEFT:
    case LV_TABVIEW_BTNS_POS_RIGHT:
        cont_x = -((lv_obj_get_width(tabview) - lv_obj_get_width(ext->btns)) * id + cont_scrl_style->body.padding.inner * id +
                cont_scrl_style->body.padding.left);
        break;
    }

    cont_x += cont_style->body.padding.left;

    if(anim == LV_ANIM_OFF || lv_tabview_get_anim_time(tabview) == 0) {
        lv_obj_set_x(lv_page_get_scrl(ext->content), cont_x);
    }
#if LV_USE_ANIMATION
    else {
        lv_anim_t a;
        a.var            = lv_page_get_scrl(ext->content);
        a.start          = lv_obj_get_x(lv_page_get_scrl(ext->content));
        a.end            = cont_x;
        a.exec_cb        = (lv_anim_exec_xcb_t)lv_obj_set_x;
        a.path_cb        = lv_anim_path_linear;
        a.ready_cb       = NULL;
        a.act_time       = 0;
        a.time           = ext->anim_time;
        a.playback       = 0;
        a.playback_pause = 0;
        a.repeat         = 0;
        a.repeat_pause   = 0;
        lv_anim_create(&a);
    }
#endif

    /*Move the indicator*/
    const lv_style_t * tabs_style = lv_obj_get_style(ext->btns);
    lv_coord_t indic_size;
    lv_coord_t indic_pos = 0; /*silence uninitialized variable warning*/;

    switch(ext->btns_pos) {
    default: /*default case is prevented in lv_tabview_set_btns_pos(), but here for safety*/
    case LV_TABVIEW_BTNS_POS_NONE:
        break;
    case LV_TABVIEW_BTNS_POS_TOP:
    case LV_TABVIEW_BTNS_POS_BOTTOM:
        indic_size = lv_obj_get_width(ext->indic);
        indic_pos  = indic_size * id + tabs_style->body.padding.inner * id + tabs_style->body.padding.left;
        break;
    case LV_TABVIEW_BTNS_POS_LEFT:
    case LV_TABVIEW_BTNS_POS_RIGHT:
        indic_size = lv_obj_get_height(ext->indic);
        indic_pos  = tabs_style->body.padding.top + id * (indic_size + tabs_style->body.padding.inner);
        break;
    }

#if LV_USE_ANIMATION
    if(anim == LV_ANIM_OFF || ext->anim_time == 0)
#endif
    {
        switch(ext->btns_pos) {
        default: /*default case is prevented in lv_tabview_set_btns_pos(), but here for safety*/
        case LV_TABVIEW_BTNS_POS_NONE: break;
        case LV_TABVIEW_BTNS_POS_TOP:
        case LV_TABVIEW_BTNS_POS_BOTTOM: lv_obj_set_x(ext->indic, indic_pos); break;
        case LV_TABVIEW_BTNS_POS_LEFT:
        case LV_TABVIEW_BTNS_POS_RIGHT: lv_obj_set_y(ext->indic, indic_pos); break;
        }
    }
#if LV_USE_ANIMATION
    else {
        lv_anim_t a;
        a.var = ext->indic;

        switch(ext->btns_pos) {
        default: /*default case is prevented in lv_tabview_set_btns_pos(), but here for safety*/
        case LV_TABVIEW_BTNS_POS_NONE:
            break;
        case LV_TABVIEW_BTNS_POS_TOP:
        case LV_TABVIEW_BTNS_POS_BOTTOM:
            a.start   = lv_obj_get_x(ext->indic);
            a.end     = indic_pos;
            a.exec_cb = (lv_anim_exec_xcb_t)lv_obj_set_x;
            break;
        case LV_TABVIEW_BTNS_POS_LEFT:
        case LV_TABVIEW_BTNS_POS_RIGHT:
            a.start   = lv_obj_get_y(ext->indic);
            a.end     = indic_pos;
            a.exec_cb = (lv_anim_exec_xcb_t)lv_obj_set_y;
            break;
        }

        a.path_cb        = lv_anim_path_linear;
        a.ready_cb       = NULL;
        a.act_time       = 0;
        a.time           = ext->anim_time;
        a.playback       = 0;
        a.playback_pause = 0;
        a.repeat         = 0;
        a.repeat_pause   = 0;
        lv_anim_create(&a);
    }
#endif

    lv_btnm_set_btn_ctrl(ext->btns, ext->tab_cur, LV_BTNM_CTRL_TGL_STATE);
}

/**
 * Set the animation time of tab view when a new tab is loaded
 * @param tabview pointer to Tab view object
 * @param anim_time_ms time of animation in milliseconds
 */
void lv_tabview_set_anim_time(lv_obj_t * tabview, uint16_t anim_time)
{
    LV_ASSERT_OBJ(tabview, LV_OBJX_NAME);

#if LV_USE_ANIMATION
    lv_tabview_ext_t * ext = lv_obj_get_ext_attr(tabview);
    ext->anim_time         = anim_time;
#else
    (void)tabview;
    (void)anim_time;
#endif
}

/**
 * Set the style of a tab view
 * @param tabview pointer to a tan view object
 * @param type which style should be set
 * @param style pointer to the new style
 */
void lv_tabview_set_style(lv_obj_t * tabview, lv_tabview_style_t type, const lv_style_t * style)
{
    LV_ASSERT_OBJ(tabview, LV_OBJX_NAME);

    lv_tabview_ext_t * ext = lv_obj_get_ext_attr(tabview);

    switch(type) {
    case LV_TABVIEW_STYLE_BG: lv_obj_set_style(tabview, style); break;
    case LV_TABVIEW_STYLE_BTN_BG:
        lv_btnm_set_style(ext->btns, LV_BTNM_STYLE_BG, style);
        tabview_realign(tabview);
        break;
    case LV_TABVIEW_STYLE_BTN_REL:
        lv_btnm_set_style(ext->btns, LV_BTNM_STYLE_BTN_REL, style);
        tabview_realign(tabview);
        break;
    case LV_TABVIEW_STYLE_BTN_PR: lv_btnm_set_style(ext->btns, LV_BTNM_STYLE_BTN_PR, style); break;
    case LV_TABVIEW_STYLE_BTN_TGL_REL: lv_btnm_set_style(ext->btns, LV_BTNM_STYLE_BTN_TGL_REL, style); break;
    case LV_TABVIEW_STYLE_BTN_TGL_PR: lv_btnm_set_style(ext->btns, LV_BTNM_STYLE_BTN_TGL_PR, style); break;
    case LV_TABVIEW_STYLE_INDIC:
        lv_obj_set_style(ext->indic, style);
        tabview_realign(tabview);
        break;
    }
}

/**
 * Set the position of tab select buttons
 * @param tabview pointer to a tan view object
 * @param btns_pos which button position
 */
void lv_tabview_set_btns_pos(lv_obj_t * tabview, lv_tabview_btns_pos_t btns_pos)
{
    if(btns_pos != LV_TABVIEW_BTNS_POS_NONE &&
       btns_pos != LV_TABVIEW_BTNS_POS_TOP &&
       btns_pos != LV_TABVIEW_BTNS_POS_BOTTOM &&
       btns_pos != LV_TABVIEW_BTNS_POS_LEFT &&
       btns_pos != LV_TABVIEW_BTNS_POS_RIGHT) {
        LV_LOG_WARN("lv_tabview_set_btns_pos: unexpected button position");
        return;
    }
    LV_ASSERT_OBJ(tabview, LV_OBJX_NAME);

    lv_tabview_ext_t * ext = lv_obj_get_ext_attr(tabview);

    ext->btns_pos = btns_pos;
    tabview_realign(tabview);
}

/*=====================
 * Getter functions
 *====================*/

/**
 * Get the index of the currently active tab
 * @param tabview pointer to Tab view object
 * @return the active btn index
 */
uint16_t lv_tabview_get_tab_act(const lv_obj_t * tabview)
{
    LV_ASSERT_OBJ(tabview, LV_OBJX_NAME);

    lv_tabview_ext_t * ext = lv_obj_get_ext_attr(tabview);
    return ext->tab_cur;
}

/**
 * Get the number of tabs
 * @param tabview pointer to Tab view object
 * @return btn count
 */
uint16_t lv_tabview_get_tab_count(const lv_obj_t * tabview)
{
    LV_ASSERT_OBJ(tabview, LV_OBJX_NAME);

    lv_tabview_ext_t * ext = lv_obj_get_ext_attr(tabview);
    return ext->tab_cnt;
}

/**
 * Get the page (content area) of a tab
 * @param tabview pointer to Tab view object
 * @param id index of the btn (>= 0)
 * @return pointer to page (lv_page) object
 */
lv_obj_t * lv_tabview_get_tab(const lv_obj_t * tabview, uint16_t id)
{
    LV_ASSERT_OBJ(tabview, LV_OBJX_NAME);

    lv_tabview_ext_t * ext = lv_obj_get_ext_attr(tabview);
    lv_obj_t * content_scrl = lv_page_get_scrl(ext->content);
    uint16_t i             = 0;
    lv_obj_t * page        = lv_obj_get_child_back(content_scrl, NULL);

    while(page != NULL && i != id) {
        if(lv_obj_get_signal_cb(page) == page_signal) i++;
        page = lv_obj_get_child_back(content_scrl, page);
    }

    if(i == id) return page;

    return NULL;
}

/**
 * Get the animation time of tab view when a new tab is loaded
 * @param tabview pointer to Tab view object
 * @return time of animation in milliseconds
 */
uint16_t lv_tabview_get_anim_time(const lv_obj_t * tabview)
{
    LV_ASSERT_OBJ(tabview, LV_OBJX_NAME);

#if LV_USE_ANIMATION
    lv_tabview_ext_t * ext = lv_obj_get_ext_attr(tabview);
    return ext->anim_time;
#else
    (void)tabview;
    return 0;
#endif
}

/**
 * Get a style of a tab view
 * @param tabview pointer to a ab view object
 * @param type which style should be get
 * @return style pointer to a style
 */
const lv_style_t * lv_tabview_get_style(const lv_obj_t * tabview, lv_tabview_style_t type)
{
    LV_ASSERT_OBJ(tabview, LV_OBJX_NAME);

    const lv_style_t * style = NULL;
    lv_tabview_ext_t * ext   = lv_obj_get_ext_attr(tabview);

    switch(type) {
    case LV_TABVIEW_STYLE_BG: style = lv_obj_get_style(tabview); break;
    case LV_TABVIEW_STYLE_BTN_BG: style = lv_btnm_get_style(ext->btns, LV_BTNM_STYLE_BG); break;
    case LV_TABVIEW_STYLE_BTN_REL: style = lv_btnm_get_style(ext->btns, LV_BTNM_STYLE_BTN_REL); break;
    case LV_TABVIEW_STYLE_BTN_PR: style = lv_btnm_get_style(ext->btns, LV_BTNM_STYLE_BTN_PR); break;
    case LV_TABVIEW_STYLE_BTN_TGL_REL: style = lv_btnm_get_style(ext->btns, LV_BTNM_STYLE_BTN_TGL_REL); break;
    case LV_TABVIEW_STYLE_BTN_TGL_PR: style = lv_btnm_get_style(ext->btns, LV_BTNM_STYLE_BTN_TGL_PR); break;
    default: style = NULL; break;
    }

    return style;
}

/**
 * Get position of tab select buttons
 * @param tabview pointer to a ab view object
 */
lv_tabview_btns_pos_t lv_tabview_get_btns_pos(const lv_obj_t * tabview)
{
    LV_ASSERT_OBJ(tabview, LV_OBJX_NAME);

    lv_tabview_ext_t * ext = lv_obj_get_ext_attr(tabview);
    return ext->btns_pos;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Signal function of the Tab view
 * @param tabview pointer to a Tab view object
 * @param sign a signal type from lv_signal_t enum
 * @param param pointer to a signal specific variable
 * @return LV_RES_OK: the object is not deleted in the function; LV_RES_INV: the object is deleted
 */
static lv_res_t lv_tabview_signal(lv_obj_t * tabview, lv_signal_t sign, void * param)
{
    lv_res_t res;

    /* Include the ancient signal function */
    res = ancestor_signal(tabview, sign, param);
    if(res != LV_RES_OK) return res;
    if(sign == LV_SIGNAL_GET_TYPE) return lv_obj_handle_get_type_signal(param, LV_OBJX_NAME);

    lv_tabview_ext_t * ext = lv_obj_get_ext_attr(tabview);
    if(sign == LV_SIGNAL_CLEANUP) {
        uint8_t i;
        for(i = 0; ext->tab_name_ptr[i][0] != '\0'; i++) lv_mem_free(ext->tab_name_ptr[i]);

        lv_mem_free(ext->tab_name_ptr);
        ext->tab_name_ptr = NULL;
        ext->btns         = NULL; /*These objects were children so they are already invalid*/
        ext->content      = NULL;
    } else if(sign == LV_SIGNAL_CORD_CHG) {
        if(ext->content != NULL && (lv_obj_get_width(tabview) != lv_area_get_width(param) ||
                lv_obj_get_height(tabview) != lv_area_get_height(param))) {
            tabview_realign(tabview);
        }
    } else if(sign == LV_SIGNAL_RELEASED) {
#if LV_USE_GROUP
        /*If released by a KEYPAD or ENCODER then really the tab buttons should be released.
         * So simulate a CLICK on the tab buttons*/
        lv_indev_t * indev         = lv_indev_get_act();
        lv_indev_type_t indev_type = lv_indev_get_type(indev);
        if(indev_type == LV_INDEV_TYPE_KEYPAD ||
                (indev_type == LV_INDEV_TYPE_ENCODER && lv_group_get_editing(lv_obj_get_group(tabview)))) {
            lv_event_send(ext->btns, LV_EVENT_CLICKED, lv_event_get_data());
        }
#endif
    } else if(sign == LV_SIGNAL_FOCUS || sign == LV_SIGNAL_DEFOCUS || sign == LV_SIGNAL_CONTROL) {
        /* The button matrix is not in a group (the tab view is in it) but it should handle the
         * group signals. So propagate the related signals to the button matrix manually*/
        if(ext->btns) {
            ext->btns->signal_cb(ext->btns, sign, param);
        }

        if(sign == LV_SIGNAL_FOCUS) {
            lv_indev_type_t indev_type = lv_indev_get_type(lv_indev_get_act());
            /*If not focused by an input device assume the last input device*/
            if(indev_type == LV_INDEV_TYPE_NONE) {
                indev_type = lv_indev_get_type(lv_indev_get_next(NULL));
            }

            /*With ENCODER select the first button only in edit mode*/
            if(indev_type == LV_INDEV_TYPE_ENCODER) {
#if LV_USE_GROUP
                lv_group_t * g = lv_obj_get_group(tabview);
                if(lv_group_get_editing(g)) {
                    lv_btnm_set_pressed(ext->btns, ext->tab_cur);
                }
#endif
            } else {
                lv_btnm_set_pressed(ext->btns, ext->tab_cur);
            }
        }
    } else if(sign == LV_SIGNAL_GET_EDITABLE) {
        bool * editable = (bool *)param;
        *editable       = true;
    }


    return res;
}

/**
 * Signal function of a tab views main scrollable area
 * @param tab pointer to a tab page object
 * @param sign a signal type from lv_signal_t enum
 * @param param pointer to a signal specific variable
 * @return LV_RES_OK: the object is not deleted in the function; LV_RES_INV: the object is deleted
 */
static lv_res_t tabview_scrl_signal(lv_obj_t * tabview_scrl, lv_signal_t sign, void * param)
{
    lv_res_t res;

    /* Include the ancient signal function */
    res = ancestor_scrl_signal(tabview_scrl, sign, param);
    if(res != LV_RES_OK) return res;
    if(sign == LV_SIGNAL_GET_TYPE) return lv_obj_handle_get_type_signal(param, "");

    lv_obj_t * cont    = lv_obj_get_parent(tabview_scrl);
    lv_obj_t * tabview = lv_obj_get_parent(cont);

    if(sign == LV_SIGNAL_DRAG_THROW_BEGIN) {
        lv_tabview_ext_t * ext = lv_obj_get_ext_attr(tabview);

        lv_indev_t * indev = lv_indev_get_act();
        lv_point_t point_act;
        lv_indev_get_point(indev, &point_act);
        lv_point_t vect;
        lv_indev_get_vect(indev, &vect);
        lv_coord_t x_predict = 0;

        while(vect.x != 0) {
            x_predict += vect.x;
            vect.x = vect.x * (100 - LV_INDEV_DEF_DRAG_THROW) / 100;
        }

        res = lv_indev_finish_drag(indev);
        if(res != LV_RES_OK) return res;
        lv_obj_t * tab_page = lv_tabview_get_tab(tabview, ext->tab_cur);
        lv_coord_t page_x1  = tab_page->coords.x1 - tabview->coords.x1 + x_predict;
        lv_coord_t page_x2  = page_x1 + lv_obj_get_width(tabview);
        lv_coord_t treshold = lv_obj_get_width(tabview) / 2;

        lv_bidi_dir_t base_dir = lv_obj_get_base_dir(tabview);
        int16_t tab_cur = ext->tab_cur;
        if(page_x1 > treshold) {
            if(base_dir != LV_BIDI_DIR_RTL) tab_cur--;
            else tab_cur ++;
        } else if(page_x2 < treshold) {
            if(base_dir != LV_BIDI_DIR_RTL) tab_cur++;
            else tab_cur --;
        }

        if(tab_cur > ext->tab_cnt - 1) tab_cur = ext->tab_cnt - 1;
        if(tab_cur < 0) tab_cur = 0;

        uint32_t id_prev = lv_tabview_get_tab_act(tabview);
        lv_tabview_set_tab_act(tabview, tab_cur, LV_ANIM_ON);
        uint32_t id_new = lv_tabview_get_tab_act(tabview);

        if(id_prev != id_new) res = lv_event_send(tabview, LV_EVENT_VALUE_CHANGED, &id_new);
        if(res != LV_RES_OK) return res;

    }
    return res;
}

/**
 * Called when a tab button is clicked
 * @param tab_btnm pointer to the tab's button matrix object
 * @param event type of the event
 */
static void tab_btnm_event_cb(lv_obj_t * tab_btnm, lv_event_t event)
{
    if(event != LV_EVENT_CLICKED) return;

    uint16_t btn_id = lv_btnm_get_active_btn(tab_btnm);
    if(btn_id == LV_BTNM_BTN_NONE) return;

    lv_btnm_clear_btn_ctrl_all(tab_btnm, LV_BTNM_CTRL_TGL_STATE);
    lv_btnm_set_btn_ctrl(tab_btnm, btn_id, LV_BTNM_CTRL_TGL_STATE);

    lv_obj_t * tabview = lv_obj_get_parent(tab_btnm);

    uint32_t id_prev = lv_tabview_get_tab_act(tabview);
    lv_tabview_set_tab_act(tabview, btn_id, LV_ANIM_ON);
    uint32_t id_new = lv_tabview_get_tab_act(tabview);

    lv_res_t res = LV_RES_OK;
    if(id_prev != id_new) res = lv_event_send(tabview, LV_EVENT_VALUE_CHANGED, &id_new);

    if(res != LV_RES_OK) return;
}

/**
 * Realign and resize the elements of Tab view
 * @param tabview pointer to a Tab view object
 */
static void tabview_realign(lv_obj_t * tabview)
{
    lv_tabview_ext_t * ext = lv_obj_get_ext_attr(tabview);

    refr_btns_size(tabview);
    refr_content_size(tabview);
    refr_indic_size(tabview);

    refr_align(tabview);

    lv_tabview_set_tab_act(tabview, ext->tab_cur, LV_ANIM_OFF);
}



static void refr_indic_size(lv_obj_t * tabview)
{
    lv_tabview_ext_t * ext = lv_obj_get_ext_attr(tabview);

    const lv_style_t * style_btn_bg  = lv_tabview_get_style(tabview, LV_TABVIEW_STYLE_BTN_BG);
    const lv_style_t * style_indic  = lv_tabview_get_style(tabview, LV_TABVIEW_STYLE_INDIC);

    if(style_indic == NULL) style_indic = &lv_style_plain_color;


    /*Set the indicator width/height*/
    lv_coord_t indic_w;
    lv_coord_t indic_h;
    lv_coord_t max_h;

    switch(ext->btns_pos) {
    default: /*default case is prevented in lv_tabview_set_btns_pos(), but here for safety*/
    case LV_TABVIEW_BTNS_POS_NONE:
        lv_obj_set_hidden(ext->indic, true);
        indic_w = 0;
        indic_h = 0;
        break;
    case LV_TABVIEW_BTNS_POS_TOP:
    case LV_TABVIEW_BTNS_POS_BOTTOM:
        lv_obj_set_hidden(ext->indic, false);
        if(ext->tab_cnt) {
            indic_h = style_indic->body.padding.inner;
            indic_w = (lv_obj_get_width(tabview) - style_btn_bg->body.padding.inner * (ext->tab_cnt - 1) -
                    style_btn_bg->body.padding.left - style_btn_bg->body.padding.right) /
                            ext->tab_cnt;
        } else {
            indic_w = 0;
            indic_h = 0;
        }
        break;
    case LV_TABVIEW_BTNS_POS_LEFT:
    case LV_TABVIEW_BTNS_POS_RIGHT:
        lv_obj_set_hidden(ext->indic, false);
        if(ext->tab_cnt) {
            indic_w = style_indic->body.padding.inner;
            max_h = lv_obj_get_height(ext->btns) - style_btn_bg->body.padding.top - style_btn_bg->body.padding.bottom;
            indic_h= max_h - ((ext->tab_cnt - 1) * style_btn_bg->body.padding.inner);
            indic_h = indic_h / ext->tab_cnt;
            indic_h--; /*-1 because e.g. height = 100 means 101 pixels (0..100)*/
        } else {
            indic_w = 0;
            indic_h = 0;
        }
        break;
    }

    lv_obj_set_width(ext->indic, indic_w);
    lv_obj_set_height(ext->indic, indic_h);
}


static void refr_btns_size(lv_obj_t * tabview)
{
    lv_tabview_ext_t * ext = lv_obj_get_ext_attr(tabview);
    const lv_style_t * style_btn_bg  = lv_tabview_get_style(tabview, LV_TABVIEW_STYLE_BTN_BG);
    const lv_style_t * style_btn_rel = lv_tabview_get_style(tabview, LV_TABVIEW_STYLE_BTN_REL);


    /*Set the tabs height/width*/
    lv_coord_t btns_w;
    lv_coord_t btns_h;

    switch(ext->btns_pos) {
    default: /*default case is prevented in lv_tabview_set_btns_pos(), but here for safety*/
    case LV_TABVIEW_BTNS_POS_NONE:
        btns_w = 0;
        btns_h = 0;
        lv_obj_set_hidden(ext->btns, true);
        break;
    case LV_TABVIEW_BTNS_POS_TOP:
    case LV_TABVIEW_BTNS_POS_BOTTOM:
        lv_obj_set_hidden(ext->btns, false);
        btns_h = lv_font_get_line_height(style_btn_rel->text.font) + style_btn_rel->body.padding.top +
                style_btn_rel->body.padding.bottom + style_btn_bg->body.padding.top +
                style_btn_bg->body.padding.bottom;
        btns_w = lv_obj_get_width(tabview);

        break;
    case LV_TABVIEW_BTNS_POS_LEFT:
    case LV_TABVIEW_BTNS_POS_RIGHT:
        lv_obj_set_hidden(ext->btns, false);
        btns_w = lv_font_get_glyph_width(style_btn_rel->text.font, 'A', '\0') +
                style_btn_rel->body.padding.left + style_btn_rel->body.padding.right +
                style_btn_bg->body.padding.left + style_btn_bg->body.padding.right;
        btns_h = lv_obj_get_height(tabview);
        break;
    }

    lv_obj_set_size(ext->btns, btns_w, btns_h);
}

static void refr_content_size(lv_obj_t * tabview)
{
    lv_tabview_ext_t * ext = lv_obj_get_ext_attr(tabview);
    const lv_style_t * style_cont = lv_obj_get_style(ext->content);
    lv_coord_t cont_w;
    lv_coord_t cont_h;

    switch(ext->btns_pos) {
    default: /*default case is prevented in lv_tabview_set_btns_pos(), but here for safety*/
    case LV_TABVIEW_BTNS_POS_NONE:
        cont_w = lv_obj_get_width(tabview);
        cont_h = lv_obj_get_height(tabview);
        break;
    case LV_TABVIEW_BTNS_POS_TOP:
    case LV_TABVIEW_BTNS_POS_BOTTOM:
        cont_w = lv_obj_get_width(tabview);
        cont_h = lv_obj_get_height(tabview) - lv_obj_get_height(ext->btns);
        break;
    case LV_TABVIEW_BTNS_POS_LEFT:
    case LV_TABVIEW_BTNS_POS_RIGHT:
        cont_w = lv_obj_get_width(tabview) - lv_obj_get_width(ext->btns);
        cont_h = lv_obj_get_height(tabview);
        break;
    }

    lv_obj_set_size(ext->content, cont_w, cont_h);


    /*Refresh the size of the tab pages too. `ext->content` has a layout to align the pages*/
    cont_h -= style_cont->body.padding.top + style_cont->body.padding.bottom;
    lv_obj_t * content_scrl = lv_page_get_scrl(ext->content);
    lv_obj_t * pages = lv_obj_get_child(content_scrl, NULL);
    while(pages != NULL) {
        /*Be sure adjust only the pages (user can other things)*/
        if(lv_obj_get_signal_cb(pages) == page_signal) {
            lv_obj_set_size(pages, cont_w, cont_h);
        }
        pages = lv_obj_get_child(content_scrl, pages);
    }
}

static void refr_align(lv_obj_t * tabview)
{
    lv_tabview_ext_t * ext = lv_obj_get_ext_attr(tabview);

    switch(ext->btns_pos) {
    default: /*default case is prevented in lv_tabview_set_btns_pos(), but here for safety*/
    case LV_TABVIEW_BTNS_POS_NONE:
        lv_obj_align(ext->content, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);
        break;
    case LV_TABVIEW_BTNS_POS_TOP:
        lv_obj_align(ext->btns, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);
        lv_obj_align(ext->content, ext->btns, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
        lv_obj_align(ext->indic, ext->btns, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
        break;
    case LV_TABVIEW_BTNS_POS_BOTTOM:
        lv_obj_align(ext->content, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);
        lv_obj_align(ext->btns, ext->content, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
        lv_obj_align(ext->indic, ext->btns, LV_ALIGN_IN_TOP_LEFT, 0, 0);
        break;
    case LV_TABVIEW_BTNS_POS_LEFT:
        lv_obj_align(ext->btns, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);
        lv_obj_align(ext->content, tabview, LV_ALIGN_IN_TOP_LEFT, lv_obj_get_width(ext->btns), 0);
        lv_obj_align(ext->indic, ext->btns, LV_ALIGN_IN_TOP_RIGHT, 0, 0);
        break;
    case LV_TABVIEW_BTNS_POS_RIGHT:
        lv_obj_align(ext->btns, NULL, LV_ALIGN_IN_TOP_RIGHT, 0, 0);
        lv_obj_align(ext->content, tabview, LV_ALIGN_IN_TOP_LEFT, 0, 0);
        lv_obj_align(ext->indic, ext->btns, LV_ALIGN_IN_TOP_LEFT, 0, 0);
        break;
    }
}
#endif
